// radio_config.h - RF parameters shared by air and ground (BOTH UNITS MUST BE FLASHED WITH THE SAME VALUES).
#pragma once
#include <RF24.h>
#include <stddef.h>
#include <stdint.h>

struct RadioConfig {
  uint8_t channel;        // 0..80 ONLY (2400-2480 MHz, inside the ISM band)
  rf24_datarate_e rate;   // RF24_1MBPS default; RF24_250KBPS = range mode; RF24_2MBPS = short range
  rf24_pa_dbm_e pa;       // RF24_PA_LOW on the bench, RF24_PA_MAX only at range with the antenna fitted
  uint8_t retryDelay;     // ARD: (n+1)*250 us
  uint8_t retryCount;     // ARC: 0..15
  uint8_t address[5];     // pipe address
};

// Datasheet rule for auto-retransmit delay with ACK payloads: >= 500 us at 1 and 2 Mbps (n >= 1);
// 250 kbps needs a much longer wait because the ACK itself is slow -> 1500 us (n = 5) to be safe.
constexpr uint8_t minRetryDelay(rf24_datarate_e r) { return r == RF24_250KBPS ? 5 : 1; }

// Address quality: not all bytes equal, first byte not 0x00/0x55/0xAA/0xFF (bad preamble/ sync patterns).
constexpr bool addressOk(const uint8_t* a) {
  bool allSame = true;
  for (size_t i = 1; i < 5; ++i) if (a[i] != a[0]) allSame = false;
  return !allSame && a[0] != 0x00 && a[0] != 0x55 && a[0] != 0xAA && a[0] != 0xFF;
}

constexpr bool validRadioConfig(const RadioConfig& c) {
  return c.channel <= 80 && c.retryDelay <= 15 && c.retryCount <= 15 &&
         c.retryDelay >= minRetryDelay(c.rate) && addressOk(c.address);
}

constexpr RadioConfig kRadioConfig = {
    /*channel*/ 40,
    /*rate*/ RF24_1MBPS,
    /*pa*/ RF24_PA_LOW,
    /*retryDelay*/ 2,        // 750 us
    /*retryCount*/ 3,
    /*address*/ {0xA7, 0x3C, 0x5E, 0x91, 0xD2},
};

static_assert(validRadioConfig(kRadioConfig),
              "kRadioConfig invalid: channel <= 80, retry delay >= datasheet minimum for the rate, sane address");
