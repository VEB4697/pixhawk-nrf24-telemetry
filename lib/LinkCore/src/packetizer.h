// packetizer.h - decides WHEN to cut a packet from the byte stream and how many bytes it holds.
//
// Rule (v1.0, transparent bridge):
//   emit a packet when   available >= kMaxData (31)   -> full packet
//                  or    data has waited >= holdMs     -> partial packet (low latency)
// holdMs = 0 means "send whatever is there immediately".
// Frame-aware flushing on MAVLink boundaries is a Week 2 feature (FR-10).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "frame.h"
#include "timing.h"

namespace linkcore {

class Packetizer {
 public:
  explicit Packetizer(uint32_t holdMs = 3) : holdMs_(holdMs) {}

  void setHoldMs(uint32_t ms) { holdMs_ = ms; }
  uint32_t holdMs() const { return holdMs_; }

  // Pull the next chunk (<= maxLen, <= kMaxData) from `ring` into `out`.
  // Returns number of bytes copied; 0 means "nothing to send yet".
  // `nowMs` is any monotonic millisecond clock (millis() on target).
  template <typename Ring>
  size_t next(Ring& ring, uint32_t nowMs, uint8_t* out, size_t maxLen = kMaxData) {
    if (maxLen > kMaxData) maxLen = kMaxData;
    const size_t avail = ring.available();
    if (avail == 0) {
      pending_ = false;
      return 0;
    }
    if (!pending_) {  // first time we see data waiting
      pending_ = true;
      since_ = nowMs;
    }
    const bool full = avail >= maxLen;
    const bool aged = elapsed(nowMs, since_, holdMs_);  // wrap-safe
    if (!full && !aged) return 0;

    const size_t n = ring.read(out, maxLen);
    if (ring.available() > 0) {
      since_ = nowMs;  // leftover bytes start a new hold window
    } else {
      pending_ = false;
    }
    return n;
  }

 private:
  uint32_t holdMs_;
  uint32_t since_ = 0;
  bool pending_ = false;
};

}  // namespace linkcore
