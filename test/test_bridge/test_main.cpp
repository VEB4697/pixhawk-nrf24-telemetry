// Simulated-radio tests for PtxBridge / PrxBridge (run: pio test -e native)
// The SimLink models the nRF24 behaviours the bridge relies on: Enhanced ShockBurst retries, 2-bit PID duplicate
// suppression, ACK payloads (3-deep FIFO, re-sent on duplicates), 3-deep RX FIFO, random packet/ACK loss, link outage.
#include <stdint.h>
#include <string.h>
#include <deque>
#include <vector>
#include <unity.h>
#include "LinkCore.h"

using namespace linkcore;
using Ring = RingBuffer<2048>;
using Bytes = std::vector<uint8_t>;

struct SimLink {
  uint32_t rng = 12345;
  double lossData = 0.0, lossAck = 0.0;
  uint8_t maxRetries = 3;
  bool up = true;
  // PRX side state
  std::deque<Bytes> prxRx, ackFifo;
  bool havePid = false;
  uint8_t lastPid = 0, ptxPid = 0;
  Bytes inflightAck, lastPayload;
  bool inflightValid = false;
  uint32_t exchanges = 0;

  double rnd() {  // xorshift32 -> [0,1)
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (rng & 0xFFFFFF) / double(0x1000000);
  }
};

struct SimPtx {
  SimLink& L;
  bool exchange(const uint8_t* tx, uint8_t txLen, uint8_t* rx, uint8_t& rxLen, uint8_t& retries) {
    rxLen = 0;
    retries = 0;
    L.exchanges++;
    L.ptxPid = (L.ptxPid + 1) & 3;
    for (uint8_t attempt = 0; attempt <= L.maxRetries; ++attempt) {
      retries = attempt;
      if (!L.up || L.rnd() < L.lossData) continue;                      // packet never arrives
      const bool same = L.havePid && L.ptxPid == L.lastPid && L.lastPayload.size() == txLen &&
                        memcmp(L.lastPayload.data(), tx, txLen) == 0;            // real chip: PID + CRC must match
      if (!same) {                                                      // new packet
        if (L.prxRx.size() >= 3) continue;                              // RX FIFO full -> no ACK
        L.prxRx.push_back(Bytes(tx, tx + txLen));
        L.havePid = true;
        L.lastPid = L.ptxPid;
        L.lastPayload.assign(tx, tx + txLen);
        if (!L.ackFifo.empty()) {
          L.inflightAck = L.ackFifo.front();
          L.ackFifo.pop_front();
          L.inflightValid = true;
        } else {
          L.inflightValid = false;
        }
      }                                                                  // else duplicate: re-ACK same payload
      if (L.rnd() < L.lossAck) continue;                                // ACK lost on the way back
      if (L.inflightValid) {
        memcpy(rx, L.inflightAck.data(), L.inflightAck.size());
        rxLen = static_cast<uint8_t>(L.inflightAck.size());
      }
      return true;
    }
    retries = L.maxRetries;
    return false;
  }
};

struct SimPrx {
  SimLink& L;
  bool receive(uint8_t* rx, uint8_t& rxLen) {
    rxLen = 0;
    if (L.prxRx.empty()) return false;
    const Bytes& f = L.prxRx.front();
    memcpy(rx, f.data(), f.size());
    rxLen = static_cast<uint8_t>(f.size());
    L.prxRx.pop_front();
    return true;
  }
  bool loadAck(const uint8_t* tx, uint8_t len) {
    if (L.ackFifo.size() >= 3) return false;
    L.ackFifo.push_back(Bytes(tx, tx + len));
    return true;
  }
  void flushTx() {
    L.ackFifo.clear();
    L.inflightValid = false;
  }
};

struct Harness {
  SimLink link;
  SimPtx ptxRadio{link};
  SimPrx prxRadio{link};
  Ring pcToAir, airToPc;        // ground rings
  Ring pixToGround, groundToPix;  // air rings
  PatternGen genPc, genPix;
  PatternChecker chkAir, chkGround;  // chkAir = bytes arriving at the Pixhawk side, chkGround = at the PC side
  PtxBridge<SimPtx, Ring, Ring> ground;
  PrxBridge<SimPrx, Ring, Ring> air;
  uint32_t nowUs = 0;

  static BridgeConfig gcfg() {
    BridgeConfig c;
    c.holdMs = 0;
    return c;
  }
  static BridgeConfig acfg() {
    BridgeConfig c;
    c.holdMs = 2;
    return c;
  }
  explicit Harness(uint32_t rate)
      : genPc(rate), genPix(rate), ground(ptxRadio, pcToAir, airToPc, gcfg()), air(prxRadio, pixToGround, groundToPix, acfg()) {}

  // Let the link acquire BEFORE any data is generated (the air unit deliberately discards telemetry until it
  // has heard a ground poll - see ADR-015 - so exactness tests start feeding only once linked).
  void warmUp() {
    run(100, false);
  }

  // Advance simulated time by `ms` in 100 us ticks. `feed` = whether the byte generators run.
  void run(uint32_t ms, bool feed = true, bool drainPc = true) {
    const uint32_t ticks = ms * 10;
    for (uint32_t i = 0; i < ticks; ++i) {
      nowUs += 100;
      const uint32_t nowMs = nowUs / 1000;
      if (feed) {
        genPc.pump(pcToAir, nowMs);
        genPix.pump(pixToGround, nowMs);
      }
      ground.step(nowMs, nowUs);
      air.step(nowMs);
      chkAir.consume(groundToPix);
      if (drainPc) chkGround.consume(airToPc);
    }
  }
};

void setUp() {}
void tearDown() {}

// 1. Lossless, realistic rate (57600 baud both ways): must be byte-exact.
void test_lossless_57600_exact() {
  Harness h(5760);
  h.warmUp();
  h.run(5000);
  h.run(100, false);  // drain
  TEST_ASSERT_TRUE(h.ground.linked());
  TEST_ASSERT_TRUE(h.air.linked());
  TEST_ASSERT_EQUAL_UINT32(h.genPc.total(), h.chkAir.result().bytes);
  TEST_ASSERT_EQUAL_UINT32(h.genPix.total(), h.chkGround.result().bytes);
  TEST_ASSERT_EQUAL_UINT32(0, h.chkAir.result().gaps + h.chkAir.result().dups);
  TEST_ASSERT_EQUAL_UINT32(0, h.chkGround.result().gaps + h.chkGround.result().dups);
  TEST_ASSERT_TRUE(h.genPc.total() > 28000);
}

// 2. Saturation: both rings always full. Checks frames are filled and every poll carries data.
void test_saturation_throughput() {
  Harness h(0);
  h.warmUp();
  h.run(2000);
  // 500 polls/s * 31 B = 15.5 KB/s ideal per direction; require >= 90% of that, byte-exact.
  TEST_ASSERT_TRUE(h.chkAir.result().bytes >= 27900);
  TEST_ASSERT_TRUE(h.chkGround.result().bytes >= 27900);
  TEST_ASSERT_EQUAL_UINT32(0, h.chkAir.result().gaps + h.chkAir.result().dups);
  TEST_ASSERT_EQUAL_UINT32(0, h.chkGround.result().gaps + h.chkGround.result().dups);
}

// 3. 30% packet loss, ACKs never lost: retries must make both directions byte-exact.
void test_packet_loss_exact() {
  Harness h(5760);
  h.warmUp();
  h.link.lossData = 0.30;
  h.run(10000);
  h.link.lossData = 0.0;
  h.run(300, false);
  TEST_ASSERT_EQUAL_UINT32(h.genPc.total(), h.chkAir.result().bytes);
  TEST_ASSERT_EQUAL_UINT32(h.genPix.total(), h.chkGround.result().bytes);
  TEST_ASSERT_EQUAL_UINT32(0, h.chkAir.result().gaps + h.chkAir.result().dups);
  TEST_ASSERT_EQUAL_UINT32(0, h.chkGround.result().gaps + h.chkGround.result().dups);
}

// 4. Packet + ACK loss: uplink must be complete (dups allowed), downlink nearly complete and never duplicated.
void test_ack_loss_semantics() {
  Harness h(5760);
  h.warmUp();
  h.link.lossData = 0.20;
  h.link.lossAck = 0.10;
  h.run(10000);
  h.link.lossData = h.link.lossAck = 0.0;
  h.run(300, false);
  // uplink: no byte may be missing (a gap would mean lost command bytes)
  TEST_ASSERT_TRUE(h.chkAir.result().bytes >= h.genPc.total());
  TEST_ASSERT_EQUAL_UINT32(0, h.chkAir.result().gaps);
  // downlink: best effort, but >= 98% must arrive and nothing may be repeated
  TEST_ASSERT_TRUE(h.chkGround.result().bytes * 100u >= h.genPix.total() * 98u);
  TEST_ASSERT_EQUAL_UINT32(0, h.chkGround.result().dups);
}

// 5. Outage: stale data purged, link re-acquires quickly, stream continuity afterwards.
void test_outage_purges_and_recovers() {
  Harness h(5760);
  h.run(1000);
  TEST_ASSERT_TRUE(h.ground.linked());
  h.link.up = false;
  h.run(1500);  // longer than staleMs
  TEST_ASSERT_FALSE(h.ground.linked());
  TEST_ASSERT_FALSE(h.air.linked());
  TEST_ASSERT_TRUE(h.pcToAir.available() < 64);      // uplink not hoarded
  TEST_ASSERT_TRUE(h.pixToGround.available() < 64);  // downlink not hoarded
  h.link.up = true;
  h.run(300);
  TEST_ASSERT_TRUE(h.ground.linked());
  TEST_ASSERT_TRUE(h.air.linked());
  // after recovery data flows again at the normal rate: no burst of stale data, no duplicates
  h.chkGround.resync();                       // thousands of bytes were purged: re-baseline the 8-bit pattern
  const uint32_t before = h.chkGround.result().bytes;
  const uint32_t dupsBefore = h.chkGround.result().dups;
  h.run(1000);
  const uint32_t delta = h.chkGround.result().bytes - before;
  TEST_ASSERT_TRUE(delta > 4000);
  TEST_ASSERT_TRUE(delta < 6600);
  TEST_ASSERT_EQUAL_UINT32(dupsBefore, h.chkGround.result().dups);
}

// 6. Slow consumer: PC stops reading -> ring overflow is counted, nothing crashes, other direction unaffected.
void test_backpressure_no_crash() {
  Harness h(0);
  h.run(1500, true, /*drainPc=*/false);
  TEST_ASSERT_TRUE(h.airToPc.dropped() > 0);
  TEST_ASSERT_EQUAL_UINT32(0, h.chkAir.result().gaps + h.chkAir.result().dups);  // uplink still exact
  TEST_ASSERT_TRUE(h.chkAir.result().bytes > 20000);
}

// 7. Link comes up regardless of power-on order (air first, ground 700 ms later).
void test_power_on_order() {
  Harness h(5760);
  h.link.up = false;
  h.run(700);
  h.link.up = true;
  h.run(400);
  TEST_ASSERT_TRUE(h.ground.linked());
  TEST_ASSERT_TRUE(h.air.linked());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_lossless_57600_exact);
  RUN_TEST(test_saturation_throughput);
  RUN_TEST(test_packet_loss_exact);
  RUN_TEST(test_ack_loss_semantics);
  RUN_TEST(test_outage_purges_and_recovers);
  RUN_TEST(test_backpressure_no_crash);
  RUN_TEST(test_power_on_order);
  return UNITY_END();
}
