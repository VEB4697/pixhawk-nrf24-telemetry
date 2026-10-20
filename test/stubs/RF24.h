// Host-side fake of the TMRh20 RF24 API with nRF24L01+ behaviour the firmware depends on:
// Enhanced ShockBurst retries, 2-bit PID duplicate suppression, ACK payloads (3-deep FIFO, re-sent on duplicates),
// 3-deep RX FIFO, config matching (channel / rate / address must agree), random packet and ACK loss, brown-out reset.
// Two RF24 objects talk to each other: the one that calls openWritingPipe() is the PTX, openReadingPipe() the PRX.
#pragma once
#include <stdint.h>
#include <string.h>
#include <deque>
#include <vector>
#include "Arduino.h"

typedef enum { RF24_PA_MIN = 0, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX } rf24_pa_dbm_e;
typedef enum { RF24_1MBPS = 0, RF24_2MBPS, RF24_250KBPS } rf24_datarate_e;
typedef enum { RF24_CRC_DISABLED = 0, RF24_CRC_8, RF24_CRC_16 } rf24_crclength_e;

class RF24;
struct Rf24Medium {
  double lossData = 0.0, lossAck = 0.0;
  bool up = true;
  uint32_t rng = 987654321u;
  RF24* ptx = nullptr;
  RF24* prx = nullptr;
  uint32_t exchanges = 0;
  bool noChip = false;  // simulate a missing / unpowered module
  double rnd() {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (rng & 0xFFFFFF) / double(0x1000000);
  }
  void reset() { *this = Rf24Medium(); }
};
inline Rf24Medium g_rf24;

class RF24 {
 public:
  RF24(uint16_t, uint16_t) {}

  // ---- configuration ----
  bool begin(SPIClass*) { if (g_rf24.noChip) return false; powerOnDefaults(); return true; }
  bool isChipConnected() { return !g_rf24.noChip; }
  void setPALevel(uint8_t p, bool = true) { pa_ = p; }
  uint8_t getPALevel() { return pa_; }
  bool setDataRate(rf24_datarate_e r) { rate_ = r; return true; }
  rf24_datarate_e getDataRate() { return rate_; }
  void setChannel(uint8_t c) { channel_ = c; }
  uint8_t getChannel() { return channel_; }
  void setCRCLength(rf24_crclength_e c) { crc_ = c; }
  void setAddressWidth(uint8_t w) { addrWidth_ = w; }
  void setAutoAck(bool a) { autoAck_ = a; }
  void enableDynamicPayloads() { dyn_ = true; }
  void enableAckPayload() { ackPayload_ = true; }
  void setRetries(uint8_t delay, uint8_t count) { ard_ = delay; arc_cfg_ = count; }
  void openWritingPipe(const uint8_t* a) { memcpy(txAddr_, a, 5); isPtx_ = true; g_rf24.ptx = this; }
  void openReadingPipe(uint8_t n, const uint8_t* a) { memcpy(rxAddr_, a, 5); rdPipe_ = n; isPrx_ = true; g_rf24.prx = this; }
  void stopListening() { listening_ = false; }
  void startListening() { listening_ = true; }
  void flush_tx() { ackFifo_.clear(); inflightValid_ = false; }
  void flush_rx() { rxFifo_.clear(); }
  void printPrettyDetails() {}

  // ---- data path ----
  bool write(const void* buf, uint8_t len) {
    arcCnt_ = 0;
    g_rf24.exchanges++;
    ptxPid_ = (ptxPid_ + 1) & 3;
    RF24* p = g_rf24.prx;
    for (uint8_t attempt = 0; attempt <= arc_cfg_; ++attempt) {
      arcCnt_ = attempt;
      if (!g_rf24.up || !p || !p->listening_ || !compatible(*p) || g_rf24.rnd() < g_rf24.lossData) continue;
      const bool same = p->havePid_ && ptxPid_ == p->lastPid_ && p->lastPayload_.size() == len &&
                        memcmp(p->lastPayload_.data(), buf, len) == 0;   // real chip: PID + CRC (covers payload) must match
      if (!same) {                                             // new packet
        if (p->rxFifo_.size() >= 3) continue;                  // RX FIFO full -> no ACK
        p->rxFifo_.emplace_back(static_cast<const uint8_t*>(buf), static_cast<const uint8_t*>(buf) + len);
        p->havePid_ = true;
        p->lastPid_ = ptxPid_;
        p->lastPayload_.assign(static_cast<const uint8_t*>(buf), static_cast<const uint8_t*>(buf) + len);
        if (!p->ackFifo_.empty()) { p->inflight_ = p->ackFifo_.front(); p->ackFifo_.pop_front(); p->inflightValid_ = true; }
        else p->inflightValid_ = false;
      }                                                         // else duplicate: re-ACK with the same payload
      if (g_rf24.rnd() < g_rf24.lossAck) continue;              // ACK lost on the way back
      if (p->inflightValid_ && p->ackPayload_) rxFifo_.push_back(p->inflight_);
      return true;
    }
    flush_tx();
    return false;
  }
  uint8_t getARC() { return arcCnt_; }
  bool available() { return !rxFifo_.empty(); }
  uint8_t getDynamicPayloadSize() { return rxFifo_.empty() ? 0 : static_cast<uint8_t>(rxFifo_.front().size()); }
  void read(void* buf, uint8_t len) {
    if (rxFifo_.empty()) return;
    const std::vector<uint8_t>& f = rxFifo_.front();
    memcpy(buf, f.data(), len < f.size() ? len : f.size());
    rxFifo_.pop_front();
  }
  bool writeAckPayload(uint8_t pipe, const void* buf, uint8_t len) {
    if (!ackPayload_ || !dyn_ || pipe != rdPipe_ || ackFifo_.size() >= 3) return false;
    ackFifo_.emplace_back(static_cast<const uint8_t*>(buf), static_cast<const uint8_t*>(buf) + len);
    return true;
  }

  // ---- test hooks ----
  void fakeBrownOut() { powerOnDefaults(); listening_ = false; }  // registers reset, FIFOs lost, pipes closed
  size_t ackQueued() const { return ackFifo_.size(); }
  uint8_t cfgRetriesDelay() const { return ard_; }
  uint8_t cfgRetriesCount() const { return arc_cfg_; }
  uint8_t cfgPa() const { return pa_; }
  rf24_crclength_e cfgCrc() const { return crc_; }
  bool cfgDyn() const { return dyn_; }
  bool cfgAckPayload() const { return ackPayload_; }
  bool cfgAutoAck() const { return autoAck_; }
  bool isListening() const { return listening_; }
  const uint8_t* txAddress() const { return txAddr_; }
  const uint8_t* rxAddress() const { return rxAddr_; }

 private:
  void powerOnDefaults() {
    channel_ = 76; rate_ = RF24_1MBPS; pa_ = RF24_PA_MAX; crc_ = RF24_CRC_16; addrWidth_ = 5;
    autoAck_ = true; dyn_ = false; ackPayload_ = false; ard_ = 5; arc_cfg_ = 15;
    rxFifo_.clear(); ackFifo_.clear(); inflightValid_ = false; havePid_ = false;
  }
  bool compatible(const RF24& o) const {
    return channel_ == o.channel_ && rate_ == o.rate_ && memcmp(txAddr_, o.rxAddr_, 5) == 0 && o.autoAck_ && autoAck_;
  }
  uint8_t channel_ = 76, pa_ = RF24_PA_MAX, addrWidth_ = 5, ard_ = 5, arc_cfg_ = 15, rdPipe_ = 255;
  rf24_datarate_e rate_ = RF24_1MBPS;
  rf24_crclength_e crc_ = RF24_CRC_16;
  bool autoAck_ = true, dyn_ = false, ackPayload_ = false, listening_ = false, isPtx_ = false, isPrx_ = false;
  uint8_t txAddr_[5] = {}, rxAddr_[5] = {};
  uint8_t arcCnt_ = 0, ptxPid_ = 0, lastPid_ = 0;
  bool havePid_ = false, inflightValid_ = false;
  std::deque<std::vector<uint8_t>> rxFifo_, ackFifo_;
  std::vector<uint8_t> inflight_, lastPayload_;
};
