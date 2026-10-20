// Host-side unit tests for lib/LinkCore (run: pio test -e native)
#include <string.h>
#include <unity.h>
#include <deque>
#include <random>
#include <vector>
#include "LinkCore.h"

using namespace linkcore;

void setUp() {}
void tearDown() {}

// ---------- frame ----------
void test_header_pack_unpack() {
  for (uint8_t seq = 0; seq < 16; ++seq)
    for (uint8_t fl = 0; fl < 16; ++fl) {
      Header h = unpackHeader(packHeader(seq, fl));
      TEST_ASSERT_EQUAL_UINT8(seq, h.seq);
      TEST_ASSERT_EQUAL_UINT8(fl, h.flags);
    }
}

void test_header_masks_out_of_range() {
  Header h = unpackHeader(packHeader(0x1F, 0xFF));  // upper bits must be masked
  TEST_ASSERT_EQUAL_UINT8(0x0F, h.seq);
  TEST_ASSERT_EQUAL_UINT8(0x0F, h.flags);
}

void test_seq_wrap_and_gap() {
  TEST_ASSERT_EQUAL_UINT8(0, nextSeq(15));
  TEST_ASSERT_EQUAL_UINT8(0, seqGap(5, 5));
  TEST_ASSERT_EQUAL_UINT8(3, seqGap(5, 8));
  TEST_ASSERT_EQUAL_UINT8(2, seqGap(14, 0));  // wrap: 14 -> (15 skipped) ... got 0 => 2 skipped
}

void test_encode_decode_roundtrip() {
  uint8_t data[31];
  for (int i = 0; i < 31; ++i) data[i] = static_cast<uint8_t>(i * 7 + 1);
  uint8_t buf[kMaxFrame];
  size_t n = encodeFrame(9, kFlagCtrl, data, 31, buf);
  TEST_ASSERT_EQUAL_UINT32(32, n);

  Header h;
  const uint8_t* d;
  size_t dl;
  TEST_ASSERT_TRUE(decodeFrame(buf, n, h, d, dl));
  TEST_ASSERT_EQUAL_UINT8(9, h.seq);
  TEST_ASSERT_EQUAL_UINT8(kFlagCtrl, h.flags);
  TEST_ASSERT_EQUAL_UINT32(31, dl);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(data, d, 31);
}

void test_encode_clamps_len() {
  uint8_t data[64] = {0};
  uint8_t buf[kMaxFrame];
  TEST_ASSERT_EQUAL_UINT32(32, encodeFrame(0, 0, data, 64, buf));
}

void test_empty_poll_frame() {
  uint8_t buf[kMaxFrame];
  size_t n = encodeFrame(3, 0, nullptr, 0, buf);
  TEST_ASSERT_EQUAL_UINT32(1, n);
  Header h;
  const uint8_t* d;
  size_t dl;
  TEST_ASSERT_TRUE(decodeFrame(buf, n, h, d, dl));
  TEST_ASSERT_EQUAL_UINT32(0, dl);
}

void test_decode_rejects_bad() {
  Header h;
  const uint8_t* d;
  size_t dl;
  uint8_t buf[40] = {0};
  TEST_ASSERT_FALSE(decodeFrame(buf, 0, h, d, dl));
  TEST_ASSERT_FALSE(decodeFrame(buf, 33, h, d, dl));
  TEST_ASSERT_FALSE(decodeFrame(nullptr, 5, h, d, dl));
}

// ---------- ring buffer ----------
void test_ring_basic_and_wrap() {
  RingBuffer<8> r;
  TEST_ASSERT_TRUE(r.empty());
  uint8_t in[6] = {1, 2, 3, 4, 5, 6}, out[8];
  TEST_ASSERT_EQUAL_UINT32(6, r.write(in, 6));
  TEST_ASSERT_EQUAL_UINT32(4, r.read(out, 4));  // tail moves to 4
  TEST_ASSERT_EQUAL_UINT32(6, r.write(in, 6));  // wraps around the end
  TEST_ASSERT_EQUAL_UINT32(8, r.available());
  TEST_ASSERT_EQUAL_UINT32(0, r.space());
  TEST_ASSERT_EQUAL_UINT32(8, r.read(out, 8));
  const uint8_t expect[8] = {5, 6, 1, 2, 3, 4, 5, 6};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, out, 8);
}

void test_ring_overflow_drops_newest() {
  RingBuffer<4> r;
  uint8_t in[6] = {1, 2, 3, 4, 5, 6}, out[4];
  TEST_ASSERT_EQUAL_UINT32(4, r.write(in, 6));
  TEST_ASSERT_EQUAL_UINT32(2, r.dropped());
  TEST_ASSERT_EQUAL_UINT32(4, r.read(out, 4));
  const uint8_t expect[4] = {1, 2, 3, 4};  // oldest preserved
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, out, 4);
}

void test_ring_peek_and_clear() {
  RingBuffer<8> r;
  uint8_t in[3] = {9, 8, 7}, out[3];
  r.write(in, 3);
  TEST_ASSERT_EQUAL_UINT32(3, r.peek(out, 3));
  TEST_ASSERT_EQUAL_UINT32(3, r.available());  // peek does not consume
  r.clear();
  TEST_ASSERT_TRUE(r.empty());
}

// ---------- packetizer ----------
void test_packetizer_full_packet_immediate() {
  RingBuffer<256> r;
  Packetizer p(3);
  uint8_t in[40], out[kMaxData];
  for (int i = 0; i < 40; ++i) in[i] = static_cast<uint8_t>(i);
  r.write(in, 40);
  TEST_ASSERT_EQUAL_UINT32(31, p.next(r, 100, out));   // full -> no wait
  TEST_ASSERT_EQUAL_UINT32(9, r.available());
  TEST_ASSERT_EQUAL_UINT32(0, p.next(r, 100, out));    // leftover 9 must age
  TEST_ASSERT_EQUAL_UINT32(0, p.next(r, 102, out));
  TEST_ASSERT_EQUAL_UINT32(9, p.next(r, 103, out));    // aged >= 3 ms
}

void test_packetizer_partial_after_hold() {
  RingBuffer<64> r;
  Packetizer p(3);
  uint8_t in[5] = {1, 2, 3, 4, 5}, out[kMaxData];
  r.write(in, 5);
  TEST_ASSERT_EQUAL_UINT32(0, p.next(r, 1000, out));
  TEST_ASSERT_EQUAL_UINT32(0, p.next(r, 1002, out));
  TEST_ASSERT_EQUAL_UINT32(5, p.next(r, 1003, out));
  TEST_ASSERT_EQUAL_UINT32(0, p.next(r, 1010, out));  // empty
}

void test_packetizer_zero_hold_sends_now() {
  RingBuffer<64> r;
  Packetizer p(0);
  uint8_t in[2] = {1, 2}, out[kMaxData];
  r.write(in, 2);
  TEST_ASSERT_EQUAL_UINT32(2, p.next(r, 5, out));
}

void test_packetizer_millis_wraparound() {
  RingBuffer<64> r;
  Packetizer p(3);
  uint8_t in[2] = {1, 2}, out[kMaxData];
  r.write(in, 2);
  TEST_ASSERT_EQUAL_UINT32(0, p.next(r, 0xFFFFFFFEu, out));
  TEST_ASSERT_EQUAL_UINT32(2, p.next(r, 2u, out));  // 4 ms later across the wrap
}

// ---------- link stats ----------
void test_stats_seq_loss_detection() {
  LinkStats s(Role::Prx);
  s.onRx(10, 0, 0);
  s.onRx(10, 1, 1);
  s.onRx(10, 4, 2);  // 2,3 missing
  TEST_ASSERT_EQUAL_UINT32(2, s.snapshot().seqLost);
  s.onRx(10, 5, 3);
  TEST_ASSERT_EQUAL_UINT32(2, s.snapshot().seqLost);
  TEST_ASSERT_EQUAL_UINT32(40, s.snapshot().rxBytes);
}

void test_stats_state_machine() {
  LinkStats s(Role::Ptx, 500, 10);
  s.tick(0);
  TEST_ASSERT_TRUE(s.state() == LinkState::Searching);
  s.onTx(5, 0, true, 100);
  s.tick(101);
  TEST_ASSERT_TRUE(s.state() == LinkState::Linked);
  s.tick(700);  // 600 ms since last ack -> timeout
  TEST_ASSERT_TRUE(s.state() == LinkState::Searching);
}

void test_stats_rates_and_loss() {
  LinkStats s(Role::Ptx, 500, 10);
  s.tick(0);
  for (int i = 0; i < 90; ++i) s.onTx(31, 0, true, 500);
  for (int i = 0; i < 10; ++i) s.onTx(31, 3, false, 500);
  TEST_ASSERT_TRUE(s.tick(1000));  // window closes
  TEST_ASSERT_EQUAL_UINT32(2790, s.snapshot().txBytesPerSec);
  TEST_ASSERT_EQUAL_UINT8(10, s.snapshot().lossPct);
  TEST_ASSERT_EQUAL_UINT32(30, s.snapshot().retries);
  TEST_ASSERT_TRUE(s.state() == LinkState::Degraded);
}


// ---------- ring discard ----------
void test_ring_discard() {
  RingBuffer<8> r;
  uint8_t in[5] = {1, 2, 3, 4, 5}, out[5];
  r.write(in, 5);
  TEST_ASSERT_EQUAL_UINT32(2, r.discard(2));
  TEST_ASSERT_EQUAL_UINT32(3, r.read(out, 5));
  TEST_ASSERT_EQUAL_UINT8(3, out[0]);
  TEST_ASSERT_EQUAL_UINT32(0, r.discard(10));
}

// ---------- stats: duplicates are not losses ----------
void test_stats_duplicate_not_loss() {
  LinkStats s(Role::Prx);
  s.onRx(5, 3, 0);
  s.onRx(5, 4, 1);
  s.onRx(5, 4, 2);  // retransmit of seq 4
  s.onRx(5, 5, 3);
  TEST_ASSERT_EQUAL_UINT32(0, s.snapshot().seqLost);
  TEST_ASSERT_EQUAL_UINT32(1, s.snapshot().seqDup);
}

// ---------- io_pump with a fake port ----------
struct FakePort {
  std::deque<uint8_t> in;      // bytes the "device" has for us
  std::vector<uint8_t> out;    // bytes we wrote
  int room = 1000;             // availableForWrite()
  int acceptMax = 1000;        // write() accepts at most this many per call
  int available() { return static_cast<int>(in.size()); }
  int availableForWrite() { return room; }
  size_t readBytes(uint8_t* b, size_t n) {
    size_t k = 0;
    while (k < n && !in.empty()) { b[k++] = in.front(); in.pop_front(); }
    return k;
  }
  size_t write(const uint8_t* b, size_t n) {
    size_t k = n < static_cast<size_t>(acceptMax) ? n : static_cast<size_t>(acceptMax);
    out.insert(out.end(), b, b + k);
    return k;
  }
};

void test_pump_in_moves_all_and_limits() {
  FakePort p;
  RingBuffer<512> r;
  for (int i = 0; i < 300; ++i) p.in.push_back(static_cast<uint8_t>(i));
  TEST_ASSERT_EQUAL_UINT32(256, pumpIn(p, r));  // maxBytes default
  TEST_ASSERT_EQUAL_UINT32(44, pumpIn(p, r));
  TEST_ASSERT_EQUAL_UINT32(300, r.available());
  uint8_t o[300];
  r.read(o, 300);
  for (int i = 0; i < 300; ++i) TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(i), o[i]);
}

void test_pump_out_no_loss_with_partial_writes() {
  FakePort p;
  RingBuffer<512> r;
  uint8_t in[200];
  for (int i = 0; i < 200; ++i) in[i] = static_cast<uint8_t>(i * 3);
  r.write(in, 200);
  p.room = 7;
  p.acceptMax = 5;  // port takes only 5 bytes per call even though it advertised 7
  size_t guard = 0;
  while (!r.empty() && guard++ < 1000) pumpOut(r, p);
  TEST_ASSERT_EQUAL_UINT32(200, p.out.size());
  for (int i = 0; i < 200; ++i) TEST_ASSERT_EQUAL_UINT8(in[i], p.out[i]);
}

void test_pump_out_full_port_keeps_data() {
  FakePort p;
  RingBuffer<64> r;
  uint8_t in[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  r.write(in, 10);
  p.room = 0;  // port full
  TEST_ASSERT_EQUAL_UINT32(0, pumpOut(r, p));
  TEST_ASSERT_EQUAL_UINT32(10, r.available());
}

// ---------- selftest pattern ----------
void test_pattern_gen_rate() {
  RingBuffer<1024> r;
  PatternGen g(5760);
  uint32_t total = 0;
  uint8_t sink[64];
  for (uint32_t ms = 0; ms <= 1000; ++ms) {
    g.pump(r, ms);
    size_t n;
    while ((n = r.read(sink, sizeof(sink))) > 0) total += static_cast<uint32_t>(n);
  }
  TEST_ASSERT_UINT32_WITHIN(2, 5760, total);
  TEST_ASSERT_EQUAL_UINT32(total, g.total());
}

void test_pattern_checker_resync() {
  PatternChecker c;
  uint8_t a[4] = {0, 1, 2, 3}, b[4] = {200, 201, 202, 203};  // 197 forward = ambiguous without resync
  c.feed(a, 4);
  c.resync();
  c.feed(b, 4);
  TEST_ASSERT_EQUAL_UINT32(0, c.result().gaps + c.result().dups);
  TEST_ASSERT_EQUAL_UINT32(8, c.result().bytes);
}

void test_pattern_checker_classifies() {
  PatternChecker c;
  uint8_t a[10], b[10], d[10];
  for (int i = 0; i < 10; ++i) a[i] = static_cast<uint8_t>(i);
  for (int i = 0; i < 10; ++i) b[i] = static_cast<uint8_t>(40 + i);  // 30 bytes skipped
  for (int i = 0; i < 10; ++i) d[i] = static_cast<uint8_t>(45 + i);  // jumps back 5 bytes -> duplicate
  c.feed(a, 10);
  TEST_ASSERT_EQUAL_UINT32(0, c.result().gaps);
  c.feed(b, 10);
  TEST_ASSERT_EQUAL_UINT32(1, c.result().gaps);
  TEST_ASSERT_EQUAL_UINT32(30, c.result().lostBytes);
  c.feed(d, 10);
  TEST_ASSERT_EQUAL_UINT32(1, c.result().dups);
  TEST_ASSERT_EQUAL_UINT32(5, c.result().dupBytes);
  TEST_ASSERT_EQUAL_UINT32(30, c.result().bytes);
}


// ---------- timing ----------
void test_elapsed_and_periodic() {
  TEST_ASSERT_TRUE(elapsed(100, 90, 10));
  TEST_ASSERT_FALSE(elapsed(100, 91, 10));
  TEST_ASSERT_TRUE(elapsed(3u, 0xFFFFFFFCu, 7u));  // across the wrap
  Periodic p(10);
  TEST_ASSERT_TRUE(p.due(1000));   // first call is due
  TEST_ASSERT_FALSE(p.due(1005));
  TEST_ASSERT_TRUE(p.due(1010));
  TEST_ASSERT_FALSE(p.due(1011));
  Periodic w(4);
  TEST_ASSERT_TRUE(w.due(0xFFFFFFFEu));
  TEST_ASSERT_FALSE(w.due(0xFFFFFFFFu));
  TEST_ASSERT_TRUE(w.due(2u));     // 4 ticks later, wrapped
}

// ---------- ring buffer: randomised comparison against a std::deque model ----------
void test_ring_fuzz_vs_model() {
  RingBuffer<64> r;
  std::deque<uint8_t> model;
  std::mt19937 rng(1234);
  uint8_t next = 0;
  uint32_t droppedModel = 0;
  for (int iter = 0; iter < 200000; ++iter) {
    const int op = rng() % 5;
    if (op <= 1) {                       // write 0..40 bytes
      uint8_t buf[40];
      const size_t n = rng() % 41;
      for (size_t i = 0; i < n; ++i) buf[i] = next++;
      const size_t room = 64 - model.size();
      const size_t acc = n < room ? n : room;
      for (size_t i = 0; i < acc; ++i) model.push_back(buf[i]);
      droppedModel += static_cast<uint32_t>(n - acc);
      TEST_ASSERT_EQUAL_UINT32(acc, r.write(buf, n));
    } else if (op == 2) {                // read 0..40
      uint8_t out[40];
      const size_t want = rng() % 41;
      const size_t got = r.read(out, want);
      TEST_ASSERT_EQUAL_UINT32(want < model.size() ? want : model.size(), got);
      for (size_t i = 0; i < got; ++i) { TEST_ASSERT_EQUAL_UINT8(model.front(), out[i]); model.pop_front(); }
    } else if (op == 3) {                // peek must not consume
      uint8_t out[40];
      const size_t got = r.peek(out, 40);
      for (size_t i = 0; i < got; ++i) TEST_ASSERT_EQUAL_UINT8(model[i], out[i]);
    } else {                             // discard or (rarely) clear
      if (rng() % 16 == 0) { r.clear(); model.clear(); }
      else { const size_t n = rng() % 20; const size_t k = r.discard(n); TEST_ASSERT_EQUAL_UINT32(n < model.size() ? n : model.size(), k); for (size_t i = 0; i < k; ++i) model.pop_front(); }
    }
    TEST_ASSERT_EQUAL_UINT32(model.size(), r.available());
  }
  TEST_ASSERT_EQUAL_UINT32(droppedModel, r.dropped());
}

// ---------- traits: contract checks ----------
struct GoodPtx { bool exchange(const uint8_t*, uint8_t, uint8_t*, uint8_t&, uint8_t&) { return true; } };
struct GoodPrx { bool receive(uint8_t*, uint8_t&) { return false; } bool loadAck(const uint8_t*, uint8_t) { return true; } void flushTx() {} };
struct BadPtx { void exchange(const uint8_t*, uint8_t) {} };
struct BadPrx { bool receive(uint8_t*, uint8_t&) { return false; } };
static_assert(traits::IsPtxRadio<GoodPtx>::value, "good PTX radio accepted");
static_assert(traits::IsPrxRadio<GoodPrx>::value, "good PRX radio accepted");
static_assert(!traits::IsPtxRadio<BadPtx>::value, "wrong PTX signature rejected");
static_assert(!traits::IsPrxRadio<BadPrx>::value, "incomplete PRX radio rejected");
void test_traits_compile_time() { TEST_PASS(); }

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_header_pack_unpack);
  RUN_TEST(test_header_masks_out_of_range);
  RUN_TEST(test_seq_wrap_and_gap);
  RUN_TEST(test_encode_decode_roundtrip);
  RUN_TEST(test_encode_clamps_len);
  RUN_TEST(test_empty_poll_frame);
  RUN_TEST(test_decode_rejects_bad);
  RUN_TEST(test_ring_basic_and_wrap);
  RUN_TEST(test_ring_overflow_drops_newest);
  RUN_TEST(test_ring_peek_and_clear);
  RUN_TEST(test_packetizer_full_packet_immediate);
  RUN_TEST(test_packetizer_partial_after_hold);
  RUN_TEST(test_packetizer_zero_hold_sends_now);
  RUN_TEST(test_packetizer_millis_wraparound);
  RUN_TEST(test_stats_seq_loss_detection);
  RUN_TEST(test_stats_state_machine);
  RUN_TEST(test_stats_rates_and_loss);
  RUN_TEST(test_ring_discard);
  RUN_TEST(test_stats_duplicate_not_loss);
  RUN_TEST(test_pump_in_moves_all_and_limits);
  RUN_TEST(test_pump_out_no_loss_with_partial_writes);
  RUN_TEST(test_pump_out_full_port_keeps_data);
  RUN_TEST(test_pattern_gen_rate);
  RUN_TEST(test_pattern_checker_classifies);
  RUN_TEST(test_pattern_checker_resync);
  RUN_TEST(test_elapsed_and_periodic);
  RUN_TEST(test_ring_fuzz_vs_model);
  RUN_TEST(test_traits_compile_time);
  return UNITY_END();
}
