// air_unit.h - everything specific to the AIR unit (vehicle side, PRX) that is not hardware-pin or RF config.
// Used by src/air/main.cpp. Pure declarations/templates: no globals, safe to include from tests.
#pragma once
#include <Arduino.h>
#include "LinkCore.h"
#include "config.h"
#include "pins.h"
#include "radio_link.h"
#include "status_led.h"
#include "version.h"

namespace air_unit {

using Ring = linkcore::RingBuffer<kRingSize>;
using Bridge = linkcore::PrxBridge<RadioLink, Ring, Ring>;

// How often the radio is serviced. Polls arrive every 2 ms and the ACK queue is 2 deep, so a 250 us service period
// is invisible to the link but cuts SPI traffic by orders of magnitude versus spinning the loop flat out.
constexpr uint32_t kServiceIntervalUs = 250;
static_assert(kServiceIntervalUs * 4 <= kPollIntervalUs, "air must service the radio at least 4x per ground poll");

constexpr linkcore::BridgeConfig bridgeConfig() {
  linkcore::BridgeConfig c;
  c.holdMs = 2;               // let ~11 bytes accumulate -> fuller ACK payloads
  c.staleMs = kLinkTimeoutMs;
  c.ackDepth = 2;             // 2 payloads queued ahead of the polls
  return c;
}

// USB console: debug text only on the air unit. Never blocks the radio loop if nobody reads it.
template <class Port>
void openDebugPort(Port& port) {
  port.begin(kSerialDebugBaud);
  port.setTxTimeoutMs(0);
}

// Pixhawk TELEM1: UART1 remapped to GPIO0 (TX) / GPIO1 (RX). setRxBufferSize() must precede begin().
template <class Port>
void openPixhawkPort(Port& port) {
  port.setRxBufferSize(kUartRxBuffer);
  port.begin(kPixhawkBaud, SERIAL_8N1, PIN_PIXHAWK_RX, PIN_PIXHAWK_TX);
}

template <class Out>
void printBoot(Out& out) {
  out.printf("[AIR] boot, role=PRX, fw %s, proto %d\n", FW_VERSION, FW_PROTOCOL);
}

// One stats line per second. `chk` is non-null only in the selftest build.
template <class Out>
void printStats(Out& out, const linkcore::LinkSnapshot& s, const Ring& toGround, const Ring& toPixhawk,
                uint32_t reinits, const linkcore::CheckResult* chk = nullptr) {
  if (chk) {
    out.printf("[AIR] link=%u rxB/s=%lu txB/s=%lu rx=%lu lost=%lu dup=%lu | up rx=%lu gaps=%lu lostB=%lu dupB=%lu | drop=%lu reinit=%lu\n",
               (unsigned)s.state, (unsigned long)s.rxBytesPerSec, (unsigned long)s.txBytesPerSec,
               (unsigned long)s.rxFrames, (unsigned long)s.seqLost, (unsigned long)s.seqDup,
               (unsigned long)chk->bytes, (unsigned long)chk->gaps, (unsigned long)chk->lostBytes,
               (unsigned long)chk->dupBytes, (unsigned long)toGround.dropped(), (unsigned long)reinits);
  } else {
    out.printf("[AIR] link=%u up=%luB/s down=%luB/s lost=%lu dup=%lu dropUART=%lu dropRadio=%lu reinit=%lu\n",
               (unsigned)s.state, (unsigned long)s.rxBytesPerSec, (unsigned long)s.txBytesPerSec,
               (unsigned long)s.seqLost, (unsigned long)s.seqDup, (unsigned long)toGround.dropped(),
               (unsigned long)toPixhawk.dropped(), (unsigned long)reinits);
  }
}

}  // namespace air_unit
