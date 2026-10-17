// frame.h - over-the-air frame format (max 32 bytes, nRF24 limit)
//
//   byte 0      : [7:4] SEQ (4-bit, wraps) | [3:0] FLAGS
//   bytes 1..31 : payload (0..31 bytes of raw serial stream)
//
// Payload length is implied by the nRF24 dynamic payload width (width - 1).
// Pure C++: no Arduino/RF24 dependency so it can be unit-tested on the host.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace linkcore {

constexpr size_t kMaxFrame = 32;                 // nRF24 hardware limit
constexpr size_t kHeaderSize = 1;
constexpr size_t kMaxData = kMaxFrame - kHeaderSize;  // 31 user bytes

// FLAGS bit assignments (low nibble)
constexpr uint8_t kFlagCtrl = 0x01;  // payload carries a control/status record, not stream data
// bits 1..3 reserved - must be 0 on transmit, ignored on receive

struct Header {
  uint8_t seq;    // 0..15
  uint8_t flags;  // 0..15
};

constexpr uint8_t packHeader(uint8_t seq, uint8_t flags) {
  return static_cast<uint8_t>(((seq & 0x0F) << 4) | (flags & 0x0F));
}

constexpr Header unpackHeader(uint8_t b) {
  return Header{static_cast<uint8_t>(b >> 4), static_cast<uint8_t>(b & 0x0F)};
}

// Next sequence number (wraps 15 -> 0).
constexpr uint8_t nextSeq(uint8_t seq) { return static_cast<uint8_t>((seq + 1) & 0x0F); }

// Forward distance from `expected` to `got` in the 4-bit sequence space (0..15).
// 0 means "in order"; N means N frames were skipped.
constexpr uint8_t seqGap(uint8_t expected, uint8_t got) {
  return static_cast<uint8_t>((got - expected) & 0x0F);
}

// Encode header + data into `out` (capacity >= kMaxFrame). `len` is clamped to kMaxData.
// Returns total frame size (1 + clamped len).
inline size_t encodeFrame(uint8_t seq, uint8_t flags, const uint8_t* data, size_t len,
                          uint8_t* out) {
  if (len > kMaxData) len = kMaxData;
  out[0] = packHeader(seq, flags);
  if (len) memcpy(out + kHeaderSize, data, len);
  return kHeaderSize + len;
}

// Decode a received frame. Returns false if empty or too long.
// `data` points into `in` (no copy); `dataLen` may be 0 (poll / empty ACK).
inline bool decodeFrame(const uint8_t* in, size_t inLen, Header& hdr, const uint8_t*& data,
                        size_t& dataLen) {
  if (in == nullptr || inLen < kHeaderSize || inLen > kMaxFrame) return false;
  hdr = unpackHeader(in[0]);
  data = in + kHeaderSize;
  dataLen = inLen - kHeaderSize;
  return true;
}

static_assert(kMaxFrame == 32 && kMaxData == 31, "nRF24 payload limit is 32 bytes");
static_assert(packHeader(0xA, 0x5) == 0xA5, "header layout: SEQ high nibble, FLAGS low nibble");
static_assert(unpackHeader(0xA5).seq == 0xA && unpackHeader(0xA5).flags == 0x5, "header round trip");
static_assert(nextSeq(15) == 0 && seqGap(14, 0) == 2 && seqGap(3, 3) == 0, "sequence arithmetic");

}  // namespace linkcore
