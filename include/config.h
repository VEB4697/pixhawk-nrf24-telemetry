// config.h - compile-time constants shared by both units. Role-specific settings live in air_unit.h / ground_unit.h.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "radio_config.h"

// ---- serial ports ----
constexpr uint32_t kSerialDebugBaud = 115200;  // USB console (debug builds only; ignored by USB CDC)
constexpr uint32_t kPixhawkBaud     = 57600;   // Pixhawk 2.4.8 TELEM1 default (SERIAL1_BAUD = 57)
constexpr uint32_t kPcUartBaud      = 115200;  // ground_uart build: select the SAME baud in Mission Planner / QGC
constexpr uint32_t kUartRxBuffer    = 1024;    // driver RX buffer (the hardware FIFO is only 128 B)
constexpr uint32_t kUsbRxBuffer     = 1024;

// ---- buffers ----
constexpr size_t kRingSize = 2048;  // per direction, power of two (~350 ms of 57600-baud traffic)

// ---- timing ----
constexpr uint32_t kPollIntervalUs   = 2000;    // ground: poll period while linked (500 polls/s)
constexpr uint32_t kSearchIntervalUs = 20000;   // ground: poll period while searching
constexpr uint32_t kLinkTimeoutMs    = 500;     // no link for this long -> purge stale data (both units)
constexpr uint32_t kHealthCheckMs    = 1000;    // radio health probe period

// Self-test pattern rate (bytes/s). 5760 = 57600 baud equivalent, 0 = unlimited (max throughput test).
#ifndef SELFTEST_RATE_BPS
#define SELFTEST_RATE_BPS 5760
#endif

// ---- compile-time sanity ----
static_assert((kRingSize & (kRingSize - 1)) == 0 && kRingSize >= 512, "kRingSize: power of two, >= 512");
static_assert(kPollIntervalUs >= 1000, "polling faster than 1 ms leaves no time for the blocking write()");
static_assert(kSearchIntervalUs >= kPollIntervalUs, "searching must not poll faster than linked");
static_assert(kSearchIntervalUs / 1000 * 4 < kLinkTimeoutMs, "link timeout must span several search polls");
static_assert(kPixhawkBaud >= 9600 && kPixhawkBaud <= 921600, "implausible Pixhawk baud");
