// Runs the REAL src/air/main.cpp (setup()/loop()) on the host against fake ports + fake RF24 + a simulated ground unit.
#include <unity.h>
#include "Arduino.h"
#include "RF24.h"
#include "../../src/common/radio_link.cpp"
#include "../../src/air/main.cpp"
#include "peer_sim.h"

static sim::GroundPeer* g_peer;
static sim::Feeder* g_pix;     // the "Pixhawk" talking on Serial1
static sim::Sink* g_pixSink;   // what the Pixhawk receives from the ground station

void setUp() {}
void tearDown() {}

// Advance `ms` of simulated time in 100 us ticks, running the real loop() every tick.
static void run(uint32_t ms, bool feed = true) {
  for (uint32_t i = 0; i < ms * 10; ++i) {
    g_simNowUs += 100;
    const uint32_t nowMs = millis(), nowUs = micros();
    if (feed) g_pix->feed(Serial1, nowMs);
    g_peer->step(nowMs, nowUs, feed);
    loop();                                  // <-- the real firmware
    g_pixSink->drain(Serial1);
  }
}

void test_air_boot_configures_ports_and_radio() {
  setup();                                   // <-- the real firmware
  TEST_ASSERT_TRUE(sim::contains(Serial.text(), "[AIR] boot, role=PRX"));
  TEST_ASSERT_TRUE(sim::contains(Serial.text(), "[AIR] radio ready"));
  TEST_ASSERT_EQUAL_UINT32(kSerialDebugBaud, Serial.baud);
  TEST_ASSERT_EQUAL_UINT32(0, Serial.txTimeoutMs);                 // USB debug never blocks the radio loop
  TEST_ASSERT_EQUAL_UINT32(kPixhawkBaud, Serial1.baud);
  TEST_ASSERT_EQUAL_INT(PIN_PIXHAWK_RX, Serial1.rxPin);
  TEST_ASSERT_EQUAL_INT(PIN_PIXHAWK_TX, Serial1.txPin);
  TEST_ASSERT_EQUAL_UINT32(kUartRxBuffer, Serial1.rxBuffer);
  TEST_ASSERT_EQUAL_INT(PIN_NRF_SCK, SPI.sck);
  TEST_ASSERT_TRUE(g_peer->begin());
}

void test_air_streams_exact_both_ways() {
  run(100, false);                           // link acquisition, no data yet
  run(5000);
  run(150, false);
  TEST_ASSERT_TRUE(g_peer->bridge.linked());
  TEST_ASSERT_EQUAL_UINT32(g_pix->total(), g_peer->chk.result().bytes);          // Pixhawk -> ground
  TEST_ASSERT_EQUAL_UINT32(g_peer->gen.total(), g_pixSink->r().bytes);           // ground -> Pixhawk
  TEST_ASSERT_EQUAL_UINT32(0, g_peer->chk.result().gaps + g_peer->chk.result().dups);
  TEST_ASSERT_EQUAL_UINT32(0, g_pixSink->r().gaps + g_pixSink->r().dups);
  TEST_ASSERT_EQUAL_INT(1, sim::lastField(Serial.text(), "[AIR] link="));      // 1 Hz stats line says LINKED
  TEST_ASSERT_EQUAL_UINT32(0, g_toGround.dropped() + g_toPixhawk.dropped());
}

void test_air_survives_outage_and_relinks() {
  g_rf24.up = false;
  run(1500);
  TEST_ASSERT_FALSE(g_peer->bridge.linked());
  TEST_ASSERT_TRUE(g_toGround.available() < 64);                   // stale telemetry purged, not hoarded
  TEST_ASSERT_EQUAL_INT(0, sim::lastField(Serial.text(), "[AIR] link="));
  g_rf24.up = true;
  run(400);
  TEST_ASSERT_TRUE(g_peer->bridge.linked());
  TEST_ASSERT_EQUAL_INT(1, sim::lastField(Serial.text(), "[AIR] link="));
  g_peer->chk.resync();                      // ~8 KB were purged during the outage: re-baseline the pattern checker
  const uint32_t before = g_peer->chk.result().bytes;
  const uint32_t dupsBefore = g_peer->chk.result().dups;
  run(1000);
  const uint32_t delta = g_peer->chk.result().bytes - before;
  TEST_ASSERT_TRUE(delta > 4000);
  TEST_ASSERT_TRUE(delta < 6600);            // ~5760 B/s; a replay of stale ring contents would show up as a burst
  TEST_ASSERT_EQUAL_UINT32(dupsBefore, g_peer->chk.result().dups);
}

void test_air_recovers_from_radio_brownout() {
  g_radio.raw().fakeBrownOut();
  run(2500);
  TEST_ASSERT_TRUE(sim::contains(Serial.text(), "[AIR] radio unhealthy -> reinit"));
  TEST_ASSERT_TRUE(g_peer->bridge.linked());
  TEST_ASSERT_EQUAL_INT(1, sim::lastField(Serial.text(), "[AIR] link="));
  const uint32_t before = g_peer->chk.result().bytes;
  run(1000);
  TEST_ASSERT_TRUE(g_peer->chk.result().bytes > before + 4000);
}

void test_air_uart_backpressure_loses_nothing() {
  // Pixhawk UART TX FIFO is slow/full for a while: bytes must wait in the ring, not vanish.
  Serial1.txRoom = 3;                        // only 3 bytes per pumpOut() call can be written
  Serial1.acceptMax = 2;                     // and the port accepts even fewer
  run(300, false);
  Serial1.txRoom = 1 << 20;
  Serial1.acceptMax = 1 << 20;
  run(200, false);
  TEST_ASSERT_EQUAL_UINT32(0, g_toPixhawk.dropped());
}

int main(int, char**) {
  g_peer = new sim::GroundPeer();
  g_pix = new sim::Feeder(5760);
  g_pixSink = new sim::Sink();
  UNITY_BEGIN();
  RUN_TEST(test_air_boot_configures_ports_and_radio);
  RUN_TEST(test_air_streams_exact_both_ways);
  RUN_TEST(test_air_survives_outage_and_relinks);
  RUN_TEST(test_air_recovers_from_radio_brownout);
  RUN_TEST(test_air_uart_backpressure_loses_nothing);
  return UNITY_END();
}
