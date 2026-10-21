// Runs the REAL src/ground/main.cpp, USB-CDC variant (`ground` env), against a simulated air unit.
// The USB port is the MAVLink port here, so the test also proves the firmware prints NOTHING on it.
#include <unity.h>
#include "Arduino.h"
#include "RF24.h"
#include "../../src/common/radio_link.cpp"
#include "../../src/ground/main.cpp"
#include "peer_sim.h"

static sim::AirPeer* g_peer;
static sim::Feeder* g_pc;        // the PC (Mission Planner) talking on USB
static sim::Sink* g_pcSink;      // what the PC receives
static sim::Feeder* g_pix;       // the Pixhawk behind the simulated air unit

void setUp() {}
void tearDown() {}

static void run(uint32_t ms, bool feed = true) {
  for (uint32_t i = 0; i < ms * 10; ++i) {
    g_simNowUs += 100;
    const uint32_t nowMs = millis();
    if (feed) g_pc->feed(Serial, nowMs);
    if (feed) g_pix->feed(Serial1 /*unused sink port*/, nowMs), Serial1.in.clear();
    g_peer->step(nowMs, feed);
    loop();                                  // <-- the real firmware
    g_pcSink->drain(Serial);
  }
}

void test_ground_boot_is_silent_and_configured() {
  setup();
  TEST_ASSERT_EQUAL_UINT32(0, Serial.out.size());                  // nothing printed on the MAVLink port
  TEST_ASSERT_EQUAL_UINT32(kUsbRxBuffer, Serial.rxBuffer);
  TEST_ASSERT_EQUAL_UINT32(0, Serial.txTimeoutMs);                 // a closed PC app must not block radio polling
  TEST_ASSERT_TRUE(g_peer->begin());
}

void test_ground_streams_exact_and_stays_silent() {
  run(100, false);
  run(5000);
  run(150, false);
  TEST_ASSERT_TRUE(g_bridge.linked());
  TEST_ASSERT_EQUAL_UINT32(g_peer->gen.total(), g_pcSink->r().bytes);             // air -> PC exact
  TEST_ASSERT_EQUAL_UINT32(g_pc->total(), g_peer->chk.result().bytes);            // PC -> air exact
  TEST_ASSERT_EQUAL_UINT32(0, g_pcSink->r().gaps + g_pcSink->r().dups);           // any stray text would show here
  TEST_ASSERT_EQUAL_UINT32(0, g_peer->chk.result().gaps + g_peer->chk.result().dups);
  TEST_ASSERT_EQUAL_UINT32(0, g_toAir.dropped() + g_toPc.dropped());
}

void test_ground_pc_stall_drops_counted_not_fatal() {
  Serial.txRoom = 0;                         // PC app frozen / closed: USB TX full
  run(1200);
  TEST_ASSERT_TRUE(g_toPc.dropped() > 0);
  TEST_ASSERT_TRUE(g_bridge.linked());       // radio link unaffected
  Serial.txRoom = 1 << 20;
  run(300);
}

void test_ground_outage_purges_uplink_then_relinks() {
  g_rf24.up = false;
  run(1500);
  TEST_ASSERT_FALSE(g_bridge.linked());
  TEST_ASSERT_TRUE(g_toAir.available() < 64);                      // stale PC commands not queued up for later
  g_rf24.up = true;
  run(600);
  TEST_ASSERT_TRUE(g_bridge.linked());
  g_pcSink->resync();                        // the air unit purged ~8 KB during the outage
  const uint32_t before = g_pcSink->r().bytes;
  const uint32_t dupsBefore = g_pcSink->r().dups;
  run(1000);
  const uint32_t delta = g_pcSink->r().bytes - before;
  TEST_ASSERT_TRUE(delta > 4000);
  TEST_ASSERT_TRUE(delta < 6600);            // no burst of stale data after reconnect
  TEST_ASSERT_EQUAL_UINT32(dupsBefore, g_pcSink->r().dups);
}

void test_ground_recovers_from_radio_brownout() {
  g_radio.raw().fakeBrownOut();
  run(2500);
  TEST_ASSERT_TRUE(g_bridge.linked());
  g_pcSink->resync();
  const uint32_t before = g_pcSink->r().bytes;
  run(1000);
  TEST_ASSERT_TRUE(g_pcSink->r().bytes > before + 4000);
  TEST_ASSERT_EQUAL_UINT32(0, Serial.out.size());                  // still silent after recovery
}

int main(int, char**) {
  g_peer = new sim::AirPeer();
  g_pc = new sim::Feeder(5760);
  g_pcSink = new sim::Sink();
  g_pix = new sim::Feeder(5760);
  UNITY_BEGIN();
  RUN_TEST(test_ground_boot_is_silent_and_configured);
  RUN_TEST(test_ground_streams_exact_and_stays_silent);
  RUN_TEST(test_ground_pc_stall_drops_counted_not_fatal);
  RUN_TEST(test_ground_outage_purges_uplink_then_relinks);
  RUN_TEST(test_ground_recovers_from_radio_brownout);
  return UNITY_END();
}
