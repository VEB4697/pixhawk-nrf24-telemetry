// GROUND UNIT (GCS side) - PTX. Transparent MAVLink bridge: PC/Android <-> nRF24 <-> air unit.
// Variants and rules: include/ground_unit.h. Logic: lib/LinkCore (bridge.h). RF config: include/radio_config.h.
#include "ground_unit.h"

using namespace linkcore;
using namespace ground_unit;

#if defined(SELFTEST)
// no PC port
#elif defined(GROUND_PC_UART)
#define PC_PORT Serial1
#else
#define PC_PORT Serial
#endif

static Ring g_toAir;     // PC -> radio (uplink)
static Ring g_toPc;      // radio -> PC (downlink)
static RadioLink g_radio(PIN_NRF_CE, PIN_NRF_CSN);
static Bridge g_bridge(g_radio, g_toAir, g_toPc, bridgeConfig());
static StatusLed g_led;
static Periodic g_healthTimer(kHealthCheckMs);
static uint32_t g_reinits = 0;

#ifdef SELFTEST
static PatternGen g_gen(SELFTEST_RATE_BPS);
static PatternChecker g_chk;
#endif

void setup() {
  g_led.begin();
#if defined(SELFTEST)
  openDebugPort(Serial);
#elif defined(GROUND_PC_UART)
  openDebugPort(Serial);
  openPcUart(Serial1);
#else
  openPcUsb(Serial);
#endif
  if (kHaveDebug) printBoot(Serial);

  if (!g_radio.begin(Role::Ptx, kRadioConfig)) {
    if (kHaveDebug) Serial.println("[GND] radio init FAILED - check wiring / 3.3V / capacitor / antenna");
    g_led.fatal(3);
  }
  if (kHaveDebug) Serial.println("[GND] radio ready");
}

void loop() {
  const uint32_t nowMs = millis();
  const uint32_t nowUs = micros();

#ifdef SELFTEST
  g_gen.pump(g_toAir, nowMs);
  g_chk.consume(g_toPc);
#else
  pumpIn(PC_PORT, g_toAir);           // PC -> ring
#endif

  g_bridge.step(nowMs, nowUs);        // poll the air unit: uplink out, downlink in (self-paced)

#ifndef SELFTEST
  pumpOut(g_toPc, PC_PORT);           // ring -> PC
#endif

  if (g_healthTimer.due(nowMs) && !g_radio.healthy()) {
    g_reinits++;
    if (kHaveDebug) Serial.println("[GND] radio unhealthy -> reinit");
    if (g_radio.reinit()) g_bridge.onRadioReinit();
  }

  if (g_bridge.takeStatsReady() && kHaveDebug) {
#ifdef SELFTEST
    printStats(Serial, g_bridge.stats().snapshot(), g_toAir, g_toPc, g_reinits, &g_chk.result());
#else
    printStats(Serial, g_bridge.stats().snapshot(), g_toAir, g_toPc, g_reinits);
#endif
  }
  g_led.update(g_bridge.stats().state(), nowMs);
}
