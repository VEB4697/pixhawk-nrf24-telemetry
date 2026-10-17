// link_stats.h - counters, per-second rates, windowed loss and link state machine.
// Pure C++; feed it events from the radio loop, call tick() every loop iteration.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "frame.h"

namespace linkcore {

enum class Role : uint8_t { Ptx, Prx };               // Ptx = ground (polls), Prx = air (replies)
enum class LinkState : uint8_t { Searching, Linked, Degraded };

struct LinkSnapshot {
  // cumulative since boot
  uint32_t txFrames = 0, rxFrames = 0;
  uint32_t txBytes = 0, rxBytes = 0;     // user data bytes only (no header)
  uint32_t txFailed = 0;                 // PTX: no ACK after all retries
  uint32_t retries = 0;                  // sum of ARC (retransmit counts) reported by radio
  uint32_t seqLost = 0;                  // frames skipped according to 4-bit SEQ
  uint32_t seqDup = 0;                   // repeated/old frames (retransmits) - NOT counted as loss
  // last completed 1 s window
  uint32_t txBytesPerSec = 0, rxBytesPerSec = 0, framesPerSec = 0;
  uint8_t lossPct = 0;                   // (failed + seqLost) / (ok + failed + seqLost)
  LinkState state = LinkState::Searching;
};

class LinkStats {
 public:
  explicit LinkStats(Role role, uint32_t linkTimeoutMs = 500, uint8_t degradedLossPct = 10)
      : role_(role), timeoutMs_(linkTimeoutMs), degradedPct_(degradedLossPct) {}

  // Call after each transmit attempt. `retries` = radio's ARC_CNT (0 if unknown).
  void onTx(size_t dataBytes, uint8_t retries, bool acked, uint32_t nowMs) {
    s_.retries += retries;
    if (acked) {
      s_.txFrames++;
      s_.txBytes += static_cast<uint32_t>(dataBytes);
      winTxBytes_ += static_cast<uint32_t>(dataBytes);
      if (role_ == Role::Ptx) markOk(nowMs);
    } else {
      s_.txFailed++;
      winFail_++;
    }
  }

  // Call for every received frame (header already decoded).
  void onRx(size_t dataBytes, uint8_t seq, uint32_t nowMs) {
    s_.rxFrames++;
    s_.rxBytes += static_cast<uint32_t>(dataBytes);
    winRxBytes_ += static_cast<uint32_t>(dataBytes);
    winFrames_++;
    if (haveSeq_) {
      const uint8_t gap = seqGap(expSeq_, seq);
      if (gap >= 8) {
        s_.seqDup++;  // "behind" us in the 4-bit space: a retransmit, not a loss
      } else {
        if (gap) {
          s_.seqLost += gap;
          winLost_ += gap;
        }
        expSeq_ = nextSeq(seq);
      }
    } else {
      haveSeq_ = true;
      expSeq_ = nextSeq(seq);
    }
    if (role_ == Role::Prx) markOk(nowMs);
  }

  // Call every loop. Returns true when a new 1 s window was just closed (good time to print).
  bool tick(uint32_t nowMs) {
    if (!started_) {
      started_ = true;
      winStart_ = nowMs;
    }
    s_.state = computeState(nowMs);
    const uint32_t dt = static_cast<uint32_t>(nowMs - winStart_);
    if (dt < 1000) return false;
    s_.txBytesPerSec = winTxBytes_ * 1000u / dt;
    s_.rxBytesPerSec = winRxBytes_ * 1000u / dt;
    s_.framesPerSec = winFrames_ * 1000u / dt;
    const uint32_t bad = winFail_ + winLost_;
    const uint32_t total = winOk_ + bad;
    lastLossPct_ = total ? static_cast<uint8_t>(bad * 100u / total) : 0;
    s_.lossPct = lastLossPct_;
    s_.state = computeState(nowMs);
    winStart_ = nowMs;
    winTxBytes_ = winRxBytes_ = winFrames_ = winOk_ = winFail_ = winLost_ = 0;
    return true;
  }

  const LinkSnapshot& snapshot() const { return s_; }
  LinkState state() const { return s_.state; }

 private:
  void markOk(uint32_t nowMs) {
    winOk_++;
    lastOkMs_ = nowMs;
    everOk_ = true;
  }
  LinkState computeState(uint32_t nowMs) const {
    if (!everOk_ || static_cast<uint32_t>(nowMs - lastOkMs_) > timeoutMs_) return LinkState::Searching;
    return lastLossPct_ >= degradedPct_ ? LinkState::Degraded : LinkState::Linked;
  }

  Role role_;
  uint32_t timeoutMs_;
  uint8_t degradedPct_;
  LinkSnapshot s_;
  bool started_ = false, everOk_ = false, haveSeq_ = false;
  uint8_t expSeq_ = 0, lastLossPct_ = 0;
  uint32_t winStart_ = 0, lastOkMs_ = 0;
  uint32_t winTxBytes_ = 0, winRxBytes_ = 0, winFrames_ = 0, winOk_ = 0, winFail_ = 0, winLost_ = 0;
};

}  // namespace linkcore
