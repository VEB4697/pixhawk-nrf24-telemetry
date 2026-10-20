// Firmware-header tests: config validation, pins, StatusLed, RadioLink against the fake RF24, and the full bridge
// running through the REAL RadioLink (including brown-out recovery).
#include <unity.h>
#include "Arduino.h"
#include "RF24.h"
#include "../../src/common/radio_link.cpp"
#include "air_unit.h"
#include "ground_unit.h"

using namespace linkcore;

void setUp() {
  g_rf24.reset();
  g_simNowUs = 0;
  g_digitalWrites = 0;
}
void tearDown() {}

// ---------- config / pins (the static_asserts already guard the build; these cover the runtime helpers) ----------
void test_radio_config_validation() {
  RadioConfig c = kRadioConfig;
  TEST_ASSERT_TRUE(validRadioConfig(c));
  c.channel = 81;                 TEST_ASSERT_FALSE(validRadioConfig(c));   // outside the ISM band
  c = kRadioConfig; c.rate = RF24_250KBPS; c.retryDelay = 2;
  TEST_ASSERT_FALSE(validRadioConfig(c));                                    // ARD too short for 250 kbps
  c.retryDelay = 5;               TEST_ASSERT_TRUE(validRadioConfig(c));
  c = kRadioConfig; c.retryCount = 16; TEST_ASSERT_FALSE(validRadioConfig(c));
  const uint8_t bad1[5] = {0x55, 0x12, 0x34, 0x56, 0x78};
  const uint8_t bad2[5] = {0xA7, 0xA7, 0xA7, 0xA7, 0xA7};
  const uint8_t good[5] = {0xA7, 0x3C, 0x5E, 0x91, 0xD2};
  TEST_ASSERT_FALSE(addressOk(bad1));
  TEST_ASSERT_FALSE(addressOk(bad2));
  TEST_ASSERT_TRUE(addressOk(good));
}

void test_pin_helpers() {
  TEST_ASSERT_TRUE(pincheck::exposed(10));
  TEST_ASSERT_FALSE(pincheck::exposed(11));
  TEST_ASSERT_TRUE(pincheck::strapping(8));
  TEST_ASSERT_FALSE(pincheck::strapping(7));
  const uint8_t dup[3] = {1, 2, 1};
  TEST_ASSERT_FALSE(pincheck::allDistinct(dup, 3));
}

// ---------- StatusLed ----------
void test_led_patterns() {
  TEST_ASSERT_TRUE(ledPatternOn(LinkState::Linked, 12345));
  TEST_ASSERT_TRUE(ledPatternOn(LinkState::Searching, 100));    // (100/100)&1 = 1
  TEST_ASSERT_FALSE(ledPatternOn(LinkState::Searching, 0));
  TEST_ASSERT_TRUE(ledPatternOn(LinkState::Degraded, 500));
  TEST_ASSERT_FALSE(ledPatternOn(LinkState::Degraded, 1000));
}

void test_led_writes_only_on_change() {
  StatusLed led;
  led.begin();
  const uint32_t base = g_digitalWrites;
  for (uint32_t t = 0; t < 1000; ++t) led.update(LinkState::Linked, t);   // 1000 calls, solid on
  TEST_ASSERT_TRUE(g_digitalWrites - base <= 1);                          // one transition at most
  TEST_ASSERT_TRUE(led.isOn());
  // active-low board: ON must drive the pin LOW
  TEST_ASSERT_EQUAL_INT(LED_ACTIVE_LOW ? LOW : HIGH, g_pinLevel[PIN_LED]);
  const uint32_t b2 = g_digitalWrites;
  for (uint32_t t = 0; t < 1000; ++t) led.update(LinkState::Searching, t);  // 10 on/off cycles
  TEST_ASSERT_TRUE(g_digitalWrites - b2 <= 21);
}

// ---------- RadioLink vs fake RF24 ----------
void test_radiolink_applies_config() {
  RadioLink ptx(PIN_NRF_CE, PIN_NRF_CSN), prx(PIN_NRF_CE, PIN_NRF_CSN);
  TEST_ASSERT_TRUE(ptx.begin(Role::Ptx, kRadioConfig));
  TEST_ASSERT_TRUE(prx.begin(Role::Prx, kRadioConfig));
  RF24& r = prx.raw();
  TEST_ASSERT_EQUAL_UINT8(kRadioConfig.channel, r.getChannel());
  TEST_ASSERT_EQUAL_INT(kRadioConfig.rate, r.getDataRate());
  TEST_ASSERT_EQUAL_UINT8(kRadioConfig.pa, r.cfgPa());
  TEST_ASSERT_EQUAL_INT(RF24_CRC_16, r.cfgCrc());
  TEST_ASSERT_TRUE(r.cfgDyn());
  TEST_ASSERT_TRUE(r.cfgAckPayload());
  TEST_ASSERT_TRUE(r.cfgAutoAck());
  TEST_ASSERT_EQUAL_UINT8(kRadioConfig.retryDelay, r.cfgRetriesDelay());
  TEST_ASSERT_EQUAL_UINT8(kRadioConfig.retryCount, r.cfgRetriesCount());
  TEST_ASSERT_TRUE(r.isListening());
  TEST_ASSERT_FALSE(ptx.raw().isListening());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(kRadioConfig.address, ptx.raw().txAddress(), 5);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(kRadioConfig.address, r.rxAddress(), 5);
  TEST_ASSERT_EQUAL_INT(PIN_NRF_SCK, SPI.sck);
  TEST_ASSERT_EQUAL_INT(PIN_NRF_CSN, SPI.ss);
}

void test_radiolink_begin_failures() {
  RadioLink r(PIN_NRF_CE, PIN_NRF_CSN);
  g_rf24.noChip = true;
  TEST_ASSERT_FALSE(r.begin(Role::Prx, kRadioConfig));         // module missing / unpowered
  g_rf24.noChip = false;
  RadioConfig bad = kRadioConfig;
  bad.channel = 100;
  TEST_ASSERT_FALSE(r.begin(Role::Prx, bad));                  // invalid config refused before touching hardware
  TEST_ASSERT_TRUE(r.begin(Role::Prx, kRadioConfig));
}

void test_radiolink_brownout_detect_and_reinit() {
  RadioLink r(PIN_NRF_CE, PIN_NRF_CSN);
  TEST_ASSERT_TRUE(r.begin(Role::Prx, kRadioConfig));
  TEST_ASSERT_TRUE(r.healthy());
  r.raw().fakeBrownOut();                                      // channel falls back to the chip default
  TEST_ASSERT_FALSE(r.healthy());
  TEST_ASSERT_TRUE(r.reinit());
  TEST_ASSERT_TRUE(r.healthy());
  TEST_ASSERT_TRUE(r.raw().isListening());
}

void test_radiolink_exchange_roundtrip() {
  RadioLink ptx(PIN_NRF_CE, PIN_NRF_CSN), prx(PIN_NRF_CE, PIN_NRF_CSN);
  TEST_ASSERT_TRUE(ptx.begin(Role::Ptx, kRadioConfig));
  TEST_ASSERT_TRUE(prx.begin(Role::Prx, kRadioConfig));
  const uint8_t ack[3] = {0x11, 0x22, 0x33};
  TEST_ASSERT_TRUE(prx.loadAck(ack, 3));
  const uint8_t tx[4] = {0xA1, 1, 2, 3};
  uint8_t rx[32] = {}, rxLen = 0, retries = 9;
  TEST_ASSERT_TRUE(ptx.exchange(tx, 4, rx, rxLen, retries));
  TEST_ASSERT_EQUAL_UINT8(0, retries);
  TEST_ASSERT_EQUAL_UINT8(3, rxLen);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(ack, rx, 3);
  uint8_t got[32] = {}, gotLen = 0;
  TEST_ASSERT_TRUE(prx.receive(got, gotLen));
  TEST_ASSERT_EQUAL_UINT8(4, gotLen);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(tx, got, 4);
  TEST_ASSERT_FALSE(prx.receive(got, gotLen));                 // FIFO empty again
}

void test_radiolink_ack_fifo_limit_and_flush() {
  RadioLink prx(PIN_NRF_CE, PIN_NRF_CSN);
  TEST_ASSERT_TRUE(prx.begin(Role::Prx, kRadioConfig));
  const uint8_t d[1] = {7};
  TEST_ASSERT_TRUE(prx.loadAck(d, 1));
  TEST_ASSERT_TRUE(prx.loadAck(d, 1));
  TEST_ASSERT_TRUE(prx.loadAck(d, 1));
  TEST_ASSERT_FALSE(prx.loadAck(d, 1));                        // 3-deep hardware FIFO
  prx.flushTx();
  TEST_ASSERT_EQUAL_UINT32(0, prx.raw().ackQueued());
  TEST_ASSERT_TRUE(prx.loadAck(d, 1));
}

void test_radiolink_config_mismatch_no_link() {
  RadioLink ptx(PIN_NRF_CE, PIN_NRF_CSN), prx(PIN_NRF_CE, PIN_NRF_CSN);
  RadioConfig other = kRadioConfig;
  other.channel = 41;
  TEST_ASSERT_TRUE(ptx.begin(Role::Ptx, kRadioConfig));
  TEST_ASSERT_TRUE(prx.begin(Role::Prx, other));
  const uint8_t tx[1] = {1};
  uint8_t rx[32], rxLen, retries;
  TEST_ASSERT_FALSE(ptx.exchange(tx, 1, rx, rxLen, retries));  // different channel -> never ACKed
  TEST_ASSERT_EQUAL_UINT8(kRadioConfig.retryCount, retries);
}

// ---------- full bridge through the real RadioLink ----------
struct Rig {
  RadioLink gRadio{PIN_NRF_CE, PIN_NRF_CSN}, aRadio{PIN_NRF_CE, PIN_NRF_CSN};
  using Ring = RingBuffer<kRingSize>;
  Ring pcToAir, airToPc, pixToGround, groundToPix;
  PtxBridge<RadioLink, Ring, Ring> ground{gRadio, pcToAir, airToPc, ground_unit::bridgeConfig()};
  PrxBridge<RadioLink, Ring, Ring> air{aRadio, pixToGround, groundToPix, air_unit::bridgeConfig()};
  PatternGen genPc{5760}, genPix{5760};
  PatternChecker chkAir, chkGround;
  Periodic svc{air_unit::kServiceIntervalUs}, health{kHealthCheckMs};
  uint32_t reinits = 0;

  Rig() {
    g_rf24.reset();
    g_simNowUs = 0;
    gRadio.begin(Role::Ptx, kRadioConfig);
    aRadio.begin(Role::Prx, kRadioConfig);
  }
  // Same scheduling as the real air main: throttled radio service, 1 s health check with re-init.
  void run(uint32_t ms, bool feed = true) {
    for (uint32_t i = 0; i < ms * 10; ++i) {
      g_simNowUs += 100;
      const uint32_t nowMs = millis(), nowUs = micros();
      if (feed) { genPc.pump(pcToAir, nowMs); genPix.pump(pixToGround, nowMs); }
      ground.step(nowMs, nowUs);
      if (svc.due(nowUs)) air.step(nowMs);
      if (health.due(nowMs) && !aRadio.healthy()) { reinits++; if (aRadio.reinit()) air.onRadioReinit(); }
      chkAir.consume(groundToPix);
      chkGround.consume(airToPc);
    }
  }
};

void test_e2e_real_radiolink_exact_lossless() {
  Rig r;
  r.run(100, false);
  r.run(5000);
  r.run(100, false);
  TEST_ASSERT_EQUAL_UINT32(r.genPc.total(), r.chkAir.result().bytes);
  TEST_ASSERT_EQUAL_UINT32(r.genPix.total(), r.chkGround.result().bytes);
  TEST_ASSERT_EQUAL_UINT32(0, r.chkAir.result().gaps + r.chkAir.result().dups);
  TEST_ASSERT_EQUAL_UINT32(0, r.chkGround.result().gaps + r.chkGround.result().dups);
  TEST_ASSERT_EQUAL_UINT32(0, r.reinits);
}

void test_e2e_real_radiolink_loss() {
  Rig r;
  r.run(100, false);
  g_rf24.lossData = 0.25;
  r.run(8000);
  g_rf24.lossData = 0.0;
  r.run(300, false);
  TEST_ASSERT_EQUAL_UINT32(r.genPc.total(), r.chkAir.result().bytes);
  TEST_ASSERT_EQUAL_UINT32(r.genPix.total(), r.chkGround.result().bytes);
  TEST_ASSERT_EQUAL_UINT32(0, r.chkAir.result().gaps + r.chkAir.result().dups + r.chkGround.result().gaps + r.chkGround.result().dups);
}

void test_e2e_saturation_with_throttled_service() {
  Rig r;
  r.genPc = PatternGen(0);
  r.genPix = PatternGen(0);
  r.run(100, false);
  r.run(2000);
  // ideal 15.5 KB/s per direction; the 250 us radio service period must not cost more than ~15%
  TEST_ASSERT_TRUE(r.chkAir.result().bytes >= 26000);
  TEST_ASSERT_TRUE(r.chkGround.result().bytes >= 26000);
  TEST_ASSERT_EQUAL_UINT32(0, r.chkAir.result().gaps + r.chkAir.result().dups + r.chkGround.result().gaps + r.chkGround.result().dups);
}

void test_e2e_brownout_recovery() {
  Rig r;
  r.run(100, false);
  r.run(1500);
  TEST_ASSERT_TRUE(r.ground.linked());
  r.aRadio.raw().fakeBrownOut();             // air radio loses its registers mid-flight
  r.run(2500);                               // health check notices (<= 1 s), re-inits, link re-forms
  TEST_ASSERT_TRUE(r.reinits >= 1);
  TEST_ASSERT_TRUE(r.ground.linked());
  TEST_ASSERT_TRUE(r.air.linked());
  r.chkGround.resync();
  const uint32_t before = r.chkGround.result().bytes;
  const uint32_t dupsBefore = r.chkGround.result().dups;
  r.run(1000);
  const uint32_t delta = r.chkGround.result().bytes - before;
  TEST_ASSERT_TRUE(delta > 4000);                                  // data flows again
  TEST_ASSERT_TRUE(delta < 6600);                                  // no stale burst
  TEST_ASSERT_EQUAL_UINT32(dupsBefore, r.chkGround.result().dups);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_radio_config_validation);
  RUN_TEST(test_pin_helpers);
  RUN_TEST(test_led_patterns);
  RUN_TEST(test_led_writes_only_on_change);
  RUN_TEST(test_radiolink_applies_config);
  RUN_TEST(test_radiolink_begin_failures);
  RUN_TEST(test_radiolink_brownout_detect_and_reinit);
  RUN_TEST(test_radiolink_exchange_roundtrip);
  RUN_TEST(test_radiolink_ack_fifo_limit_and_flush);
  RUN_TEST(test_radiolink_config_mismatch_no_link);
  RUN_TEST(test_e2e_real_radiolink_exact_lossless);
  RUN_TEST(test_e2e_real_radiolink_loss);
  RUN_TEST(test_e2e_saturation_with_throttled_service);
  RUN_TEST(test_e2e_brownout_recovery);
  return UNITY_END();
}
