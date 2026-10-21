// REAL src/ground/main.cpp, `ground_uart` variant: PC link over a USB-TTL adapter on GPIO20/21 (Serial1),
// USB CDC (`Serial`) free for debug text. Also the Android-friendly path.
#define GROUND_PC_UART 1
#include <unity.h>
#include "Arduino.h"
#include "RF24.h"
#include "../../src/common/radio_link.cpp"
#include "../../src/ground/main.cpp"
#include "peer_sim.h"

static sim::AirPeer* g_peer;
static sim::Feeder* g_pc;
static sim::Sink* g_pcSink;

void setUp() {}
void tearDown() {}

static void run(uint32_t ms, bool feed = true) {
  for (uint32_t i = 0; i < ms * 10; ++i) {
    g_simNowUs += 100;
    const uint32_t nowMs = millis();
    if (feed) g_pc->feed(Serial1, nowMs);
    g_peer->step(nowMs, feed);
    loop();
    g_pcSink->drain(Serial1);
  }
}

void test_uart_variant_configures_ports() {
  setup();
  TEST_ASSERT_TRUE(sim::contains(Serial.text(), "[GND] boot, role=PTX, mode=usb-ttl"));
  TEST_ASSERT_EQUAL_UINT32(kPcUartBaud, Serial1.baud);
  TEST_ASSERT_EQUAL_INT(PIN_PC_UART_RX, Serial1.rxPin);
  TEST_ASSERT_EQUAL_INT(PIN_PC_UART_TX, Serial1.txPin);
  TEST_ASSERT_EQUAL_UINT32(kUartRxBuffer, Serial1.rxBuffer);
  TEST_ASSERT_TRUE(g_peer->begin());
}

void test_uart_variant_streams_exact_and_prints_stats_on_usb() {
  run(100, false);
  run(5000);
  run(150, false);
  TEST_ASSERT_EQUAL_UINT32(g_peer->gen.total(), g_pcSink->r().bytes);
  TEST_ASSERT_EQUAL_UINT32(g_pc->total(), g_peer->chk.result().bytes);
  TEST_ASSERT_EQUAL_UINT32(0, g_pcSink->r().gaps + g_pcSink->r().dups + g_peer->chk.result().gaps + g_peer->chk.result().dups);
  TEST_ASSERT_EQUAL_INT(1, sim::lastField(Serial.text(), "[GND] link="));   // debug stats on the free USB port
}

int main(int, char**) {
  g_peer = new sim::AirPeer();
  g_pc = new sim::Feeder(5760);
  g_pcSink = new sim::Sink();
  UNITY_BEGIN();
  RUN_TEST(test_uart_variant_configures_ports);
  RUN_TEST(test_uart_variant_streams_exact_and_prints_stats_on_usb);
  return UNITY_END();
}
