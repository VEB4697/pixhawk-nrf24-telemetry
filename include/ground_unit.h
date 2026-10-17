// ground_unit.h - everything specific to the GROUND unit (GCS side, PTX) that is not hardware-pin or RF config.
// Used by src/ground/main.cpp. Pure declarations/templates: no globals, safe to include from tests.
//
// Build variants (platformio.ini):
//   ground          : PC link = native USB CDC (`Serial`). Silent: NOTHING but MAVLink may be written to it.
//   ground_uart     : PC link = USB-TTL adapter on GPIO20/21 (Serial1). USB CDC is free -> debug stats.
//   ground_selftest : no PC link; pattern generator/checker, stats on the USB console.
#pragma once
#include <Arduino.h>
#include "LinkCore.h"
#include "config.h"
#include "pins.h"
#include "radio_link.h"
#include "status_led.h"
#include "version.h"

namespace ground_unit {

using Ring = linkcore::RingBuffer<kRingSize>;
using Bridge = linkcore::PtxBridge<RadioLink, Ring, Ring>;

#if defined(SELFTEST)
constexpr bool kHaveDebug = true;
constexpr const char* kMode = "selftest";
#elif defined(GROUND_PC_UART)
constexpr bool kHaveDebug = true;
constexpr const char* kMode = "usb-ttl";
#else
constexpr bool kHaveDebug = false;  // USB CDC carries MAVLink: any text here would corrupt the stream
constexpr const char* kMode = "usb-cdc";
#endif

constexpr linkcore::BridgeConfig bridgeConfig() {
  linkcore::BridgeConfig c;
  c.pollIntervalUs = kPollIntervalUs;
  c.searchIntervalUs = kSearchIntervalUs;
  c.holdMs = 0;               // uplink is low-rate: send immediately
  c.staleMs = kLinkTimeoutMs;
  return c;
}

template <class Port>
void openDebugPort(Port& port) {
  port.begin(kSerialDebugBaud);
  port.setTxTimeoutMs(0);
}

// PC link over native USB CDC. Baud is ignored by CDC. setRxBufferSize() must precede begin().
template <class Port>
void openPcUsb(Port& port) {
  port.setRxBufferSize(kUsbRxBuffer);
  port.begin(kSerialDebugBaud);
  port.setTxTimeoutMs(0);     // if the PC app is closed, drop instead of blocking the radio loop
}

// PC link over a USB-TTL adapter on GPIO20/21.
template <class Port>
void openPcUart(Port& port) {
  port.setRxBufferSize(kUartRxBuffer);
  port.begin(kPcUartBaud, SERIAL_8N1, PIN_PC_UART_RX, PIN_PC_UART_TX);
}

template <class Out>
void printBoot(Out& out) {
  out.printf("[GND] boot, role=PTX, mode=%s, fw %s, proto %d\n", kMode, FW_VERSION, FW_PROTOCOL);
}

template <class Out>
void printStats(Out& out, const linkcore::LinkSnapshot& s, const Ring& toAir, const Ring& toPc,
                uint32_t reinits, const linkcore::CheckResult* chk = nullptr) {
  if (chk) {
    out.printf("[GND] link=%u txB/s=%lu rxB/s=%lu fail=%lu retries=%lu loss=%u%% lost=%lu dup=%lu | down rx=%lu gaps=%lu lostB=%lu dupB=%lu | drop=%lu reinit=%lu\n",
               (unsigned)s.state, (unsigned long)s.txBytesPerSec, (unsigned long)s.rxBytesPerSec,
               (unsigned long)s.txFailed, (unsigned long)s.retries, s.lossPct, (unsigned long)s.seqLost,
               (unsigned long)s.seqDup, (unsigned long)chk->bytes, (unsigned long)chk->gaps,
               (unsigned long)chk->lostBytes, (unsigned long)chk->dupBytes, (unsigned long)toPc.dropped(),
               (unsigned long)reinits);
  } else {
    out.printf("[GND] link=%u up=%luB/s down=%luB/s fail=%lu retries=%lu loss=%u%% dropPC=%lu dropRadio=%lu reinit=%lu\n",
               (unsigned)s.state, (unsigned long)s.txBytesPerSec, (unsigned long)s.rxBytesPerSec,
               (unsigned long)s.txFailed, (unsigned long)s.retries, s.lossPct,
               (unsigned long)toAir.dropped(), (unsigned long)toPc.dropped(), (unsigned long)reinits);
  }
}

}  // namespace ground_unit
