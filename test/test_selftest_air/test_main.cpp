// REAL src/air/main.cpp, `air_selftest` variant (no Pixhawk), against a simulated ground unit.
#define SELFTEST 1
#include <unity.h>
#include "Arduino.h"
#include "RF24.h"
#include "../../src/common/radio_link.cpp"
#include "../../src/air/main.cpp"
#include "peer_sim.h"

static sim::GroundPeer* g_peer;

void setUp() {}
void tearDown() {}

static void run(uint32_t ms, bool feed = true) {
  for (uint32_t i = 0; i < ms * 10; ++i) {
    g_simNowUs += 100;
    g_peer->step(millis(), micros(), feed);
    loop();
  }
}

void test_air_selftest_reports_clean_stream() {
  setup();
  TEST_ASSERT_EQUAL_INT(0, Serial1.beginCalls);                    // no Pixhawk port opened in selftest
  TEST_ASSERT_TRUE(g_peer->begin());
  run(100, false);
  run(5000);
  const std::string t = Serial.text();
  TEST_ASSERT_EQUAL_INT(1, sim::lastField(t, "[AIR] link="));
  TEST_ASSERT_TRUE(sim::lastField(t, "up rx=") >= 28000);          // ground pattern arrived at the air checker
  TEST_ASSERT_EQUAL_INT(0, sim::lastField(t, "gaps="));
  TEST_ASSERT_EQUAL_INT(0, sim::lastField(t, "lostB="));
  TEST_ASSERT_EQUAL_INT(0, sim::lastField(t, "dupB="));
  TEST_ASSERT_TRUE(g_peer->chk.result().bytes >= 28000);           // air pattern arrived at the ground checker
  TEST_ASSERT_EQUAL_UINT32(0, g_peer->chk.result().gaps + g_peer->chk.result().dups);
}

int main(int, char**) {
  g_peer = new sim::GroundPeer();
  UNITY_BEGIN();
  RUN_TEST(test_air_selftest_reports_clean_stream);
  return UNITY_END();
}
