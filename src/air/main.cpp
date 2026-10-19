// AIR UNIT (vehicle side) - PRX. Transparent MAVLink bridge: Pixhawk TELEM1 UART <-> nRF24 <-> ground unit.
//   * Normal build : Pixhawk UART (UART1 on GPIO0/1) <-> radio. USB CDC prints 1 Hz debug stats (non-blocking).
//   * -DSELFTEST   : no Pixhawk needed; generates/validates a byte pattern (bench tests T-02 / T-03).
// Logic: lib/LinkCore (bridge.h). Role settings: include/air_unit.h. RF config: include/radio_config.h.
#include "air_unit.h"

using namespace linkcore;
using namespace air_unit;

static Ring g_toGround;    // Pixhawk -> radio (downlink)
static Ring g_toPixhawk;   // radio -> Pixhawk (uplink commands)
static RadioLink g_radio(PIN_NRF_CE, PIN_NRF_CSN);
static Bridge g_bridge(g_radio, g_toGround, g_toPixhawk, bridgeConfig());
static StatusLed g_led;
static Periodic g_serviceTimer(kServiceIntervalUs);  // micros
static Periodic g_healthTimer(kHealthCheckMs);       // millis
static uint32_t g_reinits = 0;

#ifdef SELFTEST
static PatternGen g_gen(SELFTEST_RATE_BPS);
static PatternChecker g_chk;
#endif

void setup() {
  g_led.begin();
  openDebugPort(Serial);
  printBoot(Serial);
#ifndef SELFTEST
  openPixhawkPort(Serial1);
#endif
  if (!g_radio.begin(Role::Prx, kRadioConfig)) {
    Serial.println("[AIR] radio init FAILED - check wiring / 3.3V / capacitor / antenna");
    g_led.fatal(3);
  }
  Serial.println("[AIR] radio ready");
}

void loop() {
  const uint32_t nowMs = millis();
  const uint32_t nowUs = micros();

#ifdef SELFTEST
  g_gen.pump(g_toGround, nowMs);
  g_chk.consume(g_toPixhawk);
#else
  pumpIn(Serial1, g_toGround);        // Pixhawk -> ring (every loop: never let the UART buffer fill)
#endif

  if (g_serviceTimer.due(nowUs)) g_bridge.step(nowMs);   // radio <-> rings

#ifndef SELFTEST
  pumpOut(g_toPixhawk, Serial1);      // ring -> Pixhawk
#endif

  if (g_healthTimer.due(nowMs) && !g_radio.healthy()) {  // brown-out / SPI glitch: re-init chip + bridge state
    g_reinits++;
    Serial.println("[AIR] radio unhealthy -> reinit");
    if (g_radio.reinit()) g_bridge.onRadioReinit();
  }

  if (g_bridge.takeStatsReady()) {
#ifdef SELFTEST
    printStats(Serial, g_bridge.stats().snapshot(), g_toGround, g_toPixhawk, g_reinits, &g_chk.result());
#else
    printStats(Serial, g_bridge.stats().snapshot(), g_toGround, g_toPixhawk, g_reinits);
#endif
  }
  g_led.update(g_bridge.stats().state(), nowMs);
}
