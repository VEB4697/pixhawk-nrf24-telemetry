// ring_buffer.h - fixed-size single-producer / single-consumer byte ring.
//
// * Capacity N must be a power of two (index masking, and 2^32 % N == 0 keeps head-tail correct after wrap).
// * One thread/task may write, one may read: lock-free via std::atomic load/store only. No read-modify-write
//   atomics are used on purpose - the ESP32-C3 core (RV32IMC) has no atomic extension, so RMW would fall back to
//   slow library calls.
// * Overflow policy: DROP NEWEST bytes and count them (never touches the consumer index, which keeps the SPSC
//   guarantee). Stale data is purged by the *consumer* via clear()/discard().
// * Copies use at most two memcpy segments (wrap split) instead of a per-byte masked loop.
#pragma once
#include <atomic>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace linkcore {

template <size_t N>
class RingBuffer {
  static_assert(N >= 2 && (N & (N - 1)) == 0, "RingBuffer size must be a power of two");

 public:
  static constexpr size_t kCapacity = N;

  // Bytes currently stored.
  size_t available() const {
    return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
  }
  // Free space (bytes that can still be written).
  size_t space() const { return N - available(); }
  bool empty() const { return available() == 0; }

  // Producer: write up to `len` bytes; returns bytes accepted. Rejected bytes are counted in dropped().
  size_t write(const uint8_t* data, size_t len) {
    const size_t head = head_.load(std::memory_order_relaxed);
    const size_t tail = tail_.load(std::memory_order_acquire);
    const size_t room = N - (head - tail);
    const size_t n = len < room ? len : room;
    if (n) copyIn(head, data, n);
    head_.store(head + n, std::memory_order_release);
    if (n < len) dropped_.store(dropped_.load(std::memory_order_relaxed) + static_cast<uint32_t>(len - n),
                                std::memory_order_relaxed);
    return n;
  }

  bool writeByte(uint8_t b) { return write(&b, 1) == 1; }

  // Consumer: copy up to `max` bytes out and remove them; returns bytes read.
  size_t read(uint8_t* out, size_t max) {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t avail = head_.load(std::memory_order_acquire) - tail;
    const size_t n = max < avail ? max : avail;
    if (n) copyOut(tail, out, n);
    tail_.store(tail + n, std::memory_order_release);
    return n;
  }

  // Consumer: copy without removing.
  size_t peek(uint8_t* out, size_t max) const {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t avail = head_.load(std::memory_order_acquire) - tail;
    const size_t n = max < avail ? max : avail;
    if (n) copyOut(tail, out, n);
    return n;
  }

  // Consumer: drop up to `n` bytes without copying; returns bytes dropped.
  size_t discard(size_t n) {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t avail = head_.load(std::memory_order_acquire) - tail;
    const size_t k = n < avail ? n : avail;
    tail_.store(tail + k, std::memory_order_release);
    return k;
  }

  // Consumer: discard everything (e.g. stale telemetry after link loss).
  void clear() { tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release); }

  // Total bytes rejected because the buffer was full, since boot.
  uint32_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

 private:
  void copyIn(size_t pos, const uint8_t* src, size_t n) {
    const size_t idx = pos & (N - 1);
    const size_t first = n < N - idx ? n : N - idx;
    memcpy(buf_ + idx, src, first);
    if (n > first) memcpy(buf_, src + first, n - first);
  }
  void copyOut(size_t pos, uint8_t* dst, size_t n) const {
    const size_t idx = pos & (N - 1);
    const size_t first = n < N - idx ? n : N - idx;
    memcpy(dst, buf_ + idx, first);
    if (n > first) memcpy(dst + first, buf_, n - first);
  }

  uint8_t buf_[N] = {};
  std::atomic<size_t> head_{0};      // written by the producer only
  std::atomic<size_t> tail_{0};      // written by the consumer only
  std::atomic<uint32_t> dropped_{0}; // written by the producer only (load+store, no RMW)
};

}  // namespace linkcore
