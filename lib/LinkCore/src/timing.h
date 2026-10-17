// timing.h - wrap-safe time helpers. All clocks are free-running uint32_t (millis()/micros()) that wrap.
#pragma once
#include <stdint.h>

namespace linkcore {

// True when at least `interval` ticks have passed since `since` (correct across the 2^32 wrap).
constexpr bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}

// Rate limiter: due() returns true at most once per `interval` ticks (and true on the very first call).
class Periodic {
 public:
  explicit constexpr Periodic(uint32_t interval = 0) : interval_(interval) {}
  bool due(uint32_t now) {
    if (armed_ && !elapsed(now, last_, interval_)) return false;
    armed_ = true;
    last_ = now;
    return true;
  }
  void reset() { armed_ = false; }
  constexpr uint32_t interval() const { return interval_; }

 private:
  uint32_t interval_;
  uint32_t last_ = 0;
  bool armed_ = false;
};

static_assert(elapsed(5u, 0xFFFFFFFEu, 7u), "elapsed() must survive the 32-bit wrap");
static_assert(!elapsed(5u, 0xFFFFFFFEu, 8u), "elapsed() boundary");

}  // namespace linkcore
