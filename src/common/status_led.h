// status_led.h - LED patterns: fast blink = searching, solid = linked, slow blink = degraded.
// Writes the GPIO only when the level changes (the loop runs tens of thousands of times per second).
#pragma once
#include <Arduino.h>
#include "LinkCore.h"
#include "pins.h"

// Pure pattern function (unit-tested): should the LED be ON at `nowMs` in state `st`?
constexpr bool ledPatternOn(linkcore::LinkState st, uint32_t nowMs) {
  return st == linkcore::LinkState::Linked     ? true
         : st == linkcore::LinkState::Degraded ? ((nowMs / 500) & 1) != 0
                                               : ((nowMs / 100) & 1) != 0;
}

class StatusLed {
 public:
  void begin() {
    pinMode(PIN_LED, OUTPUT);
    known_ = false;
    write(false);
  }

  void update(linkcore::LinkState st, uint32_t nowMs) { write(ledPatternOn(st, nowMs)); }

  // Fatal init error: blink `blinks` short pulses, pause, repeat forever. Never returns.
  [[noreturn]] void fatal(uint8_t blinks) {
    for (;;) {
      for (uint8_t i = 0; i < blinks; ++i) {
        write(true);
        delay(150);
        write(false);
        delay(250);
      }
      delay(1200);
    }
  }

  bool isOn() const { return on_; }

 private:
  void write(bool on) {
    if (known_ && on == on_) return;
    known_ = true;
    on_ = on;
    digitalWrite(PIN_LED, (on != LED_ACTIVE_LOW) ? HIGH : LOW);
  }
  bool on_ = false;
  bool known_ = false;
};
