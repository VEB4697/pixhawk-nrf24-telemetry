// selftest.h - deterministic byte-pattern generator + checker for bench tests (T-02 / T-03) without a Pixhawk.
//
// Pattern = incrementing byte (0,1,2,...,255,0,...). The checker classifies every discontinuity:
//   forward jump  (< 128)  -> bytes LOST
//   backward jump (>= 128) -> bytes DUPLICATED (retransmitted chunk)
// Because a radio chunk is <= 31 bytes, a lost/duplicated chunk is always unambiguous.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace linkcore {

class PatternGen {
 public:
  // bytesPerSec = 0 -> unlimited (keeps the ring full). 5760 = 57600 baud 8N1 equivalent.
  explicit PatternGen(uint32_t bytesPerSec = 5760) : rate_(bytesPerSec) {}

  template <typename Ring>
  size_t pump(Ring& ring, uint32_t nowMs) {
    if (!started_) {
      started_ = true;
      last_ = nowMs;
      return 0;
    }
    const uint32_t dt = static_cast<uint32_t>(nowMs - last_);
    last_ = nowMs;
    size_t want;
    if (rate_ == 0) {
      want = ring.space();
    } else {
      acc_ += static_cast<uint64_t>(rate_) * dt;       // milli-bytes
      want = static_cast<size_t>(acc_ / 1000u);
      acc_ -= static_cast<uint64_t>(want) * 1000u;
      const size_t space = ring.space();
      if (want > space) want = space;                  // best effort: surplus credit is dropped
    }
    size_t done = 0;
    uint8_t buf[32];
    while (done < want) {
      size_t n = want - done;
      if (n > sizeof(buf)) n = sizeof(buf);
      for (size_t i = 0; i < n; ++i) buf[i] = next_++;
      ring.write(buf, n);
      done += n;
    }
    total_ += static_cast<uint32_t>(done);
    return done;
  }

  uint32_t total() const { return total_; }

 private:
  uint32_t rate_;
  uint32_t last_ = 0;
  uint64_t acc_ = 0;
  uint32_t total_ = 0;
  uint8_t next_ = 0;
  bool started_ = false;
};

struct CheckResult {
  uint32_t bytes = 0;      // bytes received (including duplicates)
  uint32_t gaps = 0;       // number of forward discontinuities
  uint32_t lostBytes = 0;  // total bytes skipped
  uint32_t dups = 0;       // number of backward discontinuities
  uint32_t dupBytes = 0;   // total bytes repeated
};

class PatternChecker {
 public:
  void feed(const uint8_t* d, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      const uint8_t b = d[i];
      r_.bytes++;
      if (!have_) {
        have_ = true;
      } else if (b != exp_) {
        const uint8_t gap = static_cast<uint8_t>(b - exp_);
        if (gap < 128) {
          r_.gaps++;
          r_.lostBytes += gap;
        } else {
          r_.dups++;
          r_.dupBytes += static_cast<uint32_t>(256u - gap);
        }
      }
      exp_ = static_cast<uint8_t>(b + 1);
    }
  }

  template <typename Ring>
  void consume(Ring& ring) {
    uint8_t buf[64];
    size_t n;
    while ((n = ring.read(buf, sizeof(buf))) > 0) feed(buf, n);
  }

  const CheckResult& result() const { return r_; }

  // Accept the next byte as a fresh baseline (use after an outage that purged more than 127 bytes: the 8-bit
  // pattern would otherwise be ambiguous between a big forward gap and a backward jump).
  void resync() { have_ = false; }

 private:
  CheckResult r_;
  uint8_t exp_ = 0;
  bool have_ = false;
};

}  // namespace linkcore
