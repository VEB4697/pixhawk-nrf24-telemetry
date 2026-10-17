// bridge.h - the transparent MAVLink-over-nRF24 bridge logic, independent of any hardware.
//
//   PtxBridge  (GROUND, polls)   : ring(PC->air) -> frame -> radio.exchange() -> ACK payload -> ring(air->PC)
//   PrxBridge  (AIR, answers)    : radio.receive() -> ring(ground->Pixhawk);  ring(Pixhawk->ground) -> radio.loadAck()
//
// `Radio` is any type with the methods used below (RadioLink on the target, a simulated radio in the tests),
// so the exact code that runs on the ESP32-C3 is exercised by the host test-suite.
//
// Delivery semantics (documented, tested):
//   * uplink  (ground -> air): at-least-once. A chunk stays "pending" until the radio reports an ACK.
//   * downlink (air -> ground): best-effort. Rarely a chunk is lost when every ACK of an exchange is lost (nRF24 limit).
//   * stale data is purged after `staleMs` without link, so old telemetry/commands are never replayed on reconnect.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "frame.h"
#include "link_stats.h"
#include "packetizer.h"
#include "radio_traits.h"
#include "timing.h"

namespace linkcore {

struct BridgeConfig {
  uint32_t pollIntervalUs = 2000;     // PTX: poll period while linked (500 polls/s)
  uint32_t searchIntervalUs = 20000;  // PTX: poll period while searching (saves air time / power)
  uint32_t holdMs = 0;                // packetizer hold: PTX 0 (send now), PRX 2 (fill ACK payloads better)
  uint32_t staleMs = 500;             // no link for this long -> purge stale data
  uint8_t ackDepth = 2;               // PRX: ACK payloads kept queued in the radio FIFO (1..3)
};

// ------------------------------------------------------------------------------------------------ GROUND
template <typename Radio, typename TxRing, typename RxRing>
class PtxBridge {
  static_assert(traits::IsPtxRadio<Radio>::value,
                "PtxBridge needs: bool Radio::exchange(const uint8_t*, uint8_t, uint8_t*, uint8_t&, uint8_t&)");

 public:
  PtxBridge(Radio& radio, TxRing& toAir, RxRing& fromAir, const BridgeConfig& cfg = BridgeConfig())
      : radio_(radio), tx_(toAir), rx_(fromAir), cfg_(cfg), pk_(cfg.holdMs), stats_(Role::Ptx, cfg.staleMs) {}

  // Call every loop. Returns true if a radio exchange was performed.
  bool step(uint32_t nowMs, uint32_t nowUs) {
    statsReady_ |= stats_.tick(nowMs);

    const bool searching = stats_.state() == LinkState::Searching;
    const uint32_t interval = searching ? cfg_.searchIntervalUs : cfg_.pollIntervalUs;
    if (!elapsed(nowUs, lastPollUs_, interval)) return false;
    lastPollUs_ = nowUs;

    if (!pending_) {
      pendLen_ = pk_.next(tx_, nowMs, pend_, kMaxData);
      pending_ = pendLen_ > 0;
    }

    uint8_t frame[kMaxFrame];
    const size_t n = encodeFrame(seq_, 0, pend_, pending_ ? pendLen_ : 0, frame);

    uint8_t rx[kMaxFrame];
    uint8_t rxLen = 0, retries = 0;
    const bool ok = radio_.exchange(frame, static_cast<uint8_t>(n), rx, rxLen, retries);
    stats_.onTx(pending_ ? pendLen_ : 0, retries, ok, nowMs);

    if (ok) {
      seq_ = nextSeq(seq_);
      pending_ = false;
      pendLen_ = 0;
      lastOkMs_ = nowMs;
      if (rxLen) {
        Header h;
        const uint8_t* d;
        size_t dl;
        if (decodeFrame(rx, rxLen, h, d, dl)) {
          stats_.onRx(dl, h.seq, nowMs);
          if (dl) rx_.write(d, dl);
        }
      }
    } else if (elapsed(nowMs, lastOkMs_, cfg_.staleMs + 1)) {
      tx_.clear();  // link is down: never replay stale commands later
      pending_ = false;
      pendLen_ = 0;
    }
    return true;
  }

  LinkStats& stats() { return stats_; }
  const LinkStats& stats() const { return stats_; }
  bool linked() const { return stats_.state() != LinkState::Searching; }
  // True once per closed 1 s stats window (good moment to print).
  bool takeStatsReady() {
    const bool r = statsReady_;
    statsReady_ = false;
    return r;
  }
  // Call after re-initialising the radio chip.
  void onRadioReinit() {
    pending_ = false;
    pendLen_ = 0;
  }

 private:
  Radio& radio_;
  TxRing& tx_;
  RxRing& rx_;
  BridgeConfig cfg_;
  Packetizer pk_;
  LinkStats stats_;
  uint8_t pend_[kMaxData] = {};
  size_t pendLen_ = 0;
  bool pending_ = false;
  uint8_t seq_ = 0;
  uint32_t lastPollUs_ = 0;
  uint32_t lastOkMs_ = 0;
  bool statsReady_ = false;
};

// ------------------------------------------------------------------------------------------------ AIR
template <typename Radio, typename TxRing, typename RxRing>
class PrxBridge {
  static_assert(traits::IsPrxRadio<Radio>::value,
                "PrxBridge needs: bool receive(uint8_t*, uint8_t&), bool loadAck(const uint8_t*, uint8_t), void flushTx()");

 public:
  PrxBridge(Radio& radio, TxRing& toGround, RxRing& fromGround, const BridgeConfig& cfg = BridgeConfig())
      : radio_(radio), tx_(toGround), rx_(fromGround), cfg_(cfg), pk_(cfg.holdMs), stats_(Role::Prx, cfg.staleMs) {}

  // Call every loop (fast: it only touches the radio when there is something to do).
  void step(uint32_t nowMs) {
    // 1) drain received polls (each carries uplink bytes and consumed one queued ACK payload)
    for (uint8_t i = 0; i < 4; ++i) {
      uint8_t rx[kMaxFrame];
      uint8_t rxLen = 0;
      if (!radio_.receive(rx, rxLen)) break;
      Header h;
      const uint8_t* d;
      size_t dl;
      if (!decodeFrame(rx, rxLen, h, d, dl)) continue;
      haveRx_ = true;
      lastRxMs_ = nowMs;
      staleHandled_ = false;
      if (ackQueued_ > 0) ackQueued_--;
      stats_.onRx(dl, h.seq, nowMs);
      if (dl) rx_.write(d, dl);
    }

    const bool stale = !haveRx_ || elapsed(nowMs, lastRxMs_, cfg_.staleMs + 1);
    if (stale) {
      // 2a) no ground station: do not hoard telemetry, drop anything queued in the radio too
      if (!staleHandled_) {
        radio_.flushTx();
        ackQueued_ = 0;
        pending_ = false;
        pendLen_ = 0;
        staleHandled_ = true;
      }
      tx_.clear();
    } else {
      // 2b) keep `ackDepth` ACK payloads queued so every poll gets an answer immediately
      while (ackQueued_ < cfg_.ackDepth) {
        if (!pending_) {
          pendLen_ = pk_.next(tx_, nowMs, pend_, kMaxData);
          if (pendLen_ == 0) break;
          pending_ = true;
        }
        uint8_t frame[kMaxFrame];
        const size_t n = encodeFrame(seq_, 0, pend_, pendLen_, frame);
        if (radio_.loadAck(frame, static_cast<uint8_t>(n))) {
          stats_.onTx(pendLen_, 0, true, nowMs);
          seq_ = nextSeq(seq_);
          pending_ = false;
          pendLen_ = 0;
          ackQueued_++;
        } else {
          ackQueued_ = cfg_.ackDepth;  // FIFO full: our estimate was low, retry after the next poll
          break;
        }
      }
    }
    statsReady_ |= stats_.tick(nowMs);
  }

  LinkStats& stats() { return stats_; }
  const LinkStats& stats() const { return stats_; }
  bool linked() const { return stats_.state() != LinkState::Searching; }
  bool takeStatsReady() {
    const bool r = statsReady_;
    statsReady_ = false;
    return r;
  }
  void onRadioReinit() {
    ackQueued_ = 0;
    pending_ = false;
    pendLen_ = 0;
    staleHandled_ = false;
  }

 private:
  Radio& radio_;
  TxRing& tx_;
  RxRing& rx_;
  BridgeConfig cfg_;
  Packetizer pk_;
  LinkStats stats_;
  uint8_t pend_[kMaxData] = {};
  size_t pendLen_ = 0;
  bool pending_ = false;
  uint8_t seq_ = 0;
  uint8_t ackQueued_ = 0;
  uint32_t lastRxMs_ = 0;
  bool haveRx_ = false;
  bool staleHandled_ = false;
  bool statsReady_ = false;
};

}  // namespace linkcore
