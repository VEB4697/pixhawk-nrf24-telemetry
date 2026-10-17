// pins.h - FINAL pin map for the ESP32-C3 SuperMini (identical on air and ground units).
// Full wiring diagrams: docs/PIN_DIAGRAM.md and hardware/wiring/*.svg. Every rule below is checked at compile time.
//
//   GPIO4  SCK   -+
//   GPIO5  MISO   |  nRF24 on the Arduino-default SPI pins of the C3 SuperMini variant, so RF24's internal SPI
//   GPIO6  MOSI   |  init lands on the same pins even if it re-initialises the bus.
//   GPIO7  CSN    |
//   GPIO10 CE     |
//   GPIO3  IRQ   -+  (wired for the future IRQ-driven loop; unused by v1.0 firmware)
//   GPIO0  TX  -> Pixhawk TELEM1 RX      (air unit, UART1 remapped)
//   GPIO1  RX  <- Pixhawk TELEM1 TX      (air unit, UART1 remapped)
//   GPIO21 TX  -> USB-TTL RX             (ground_uart build only)
//   GPIO20 RX  <- USB-TTL TX             (ground_uart build only)
//   GPIO8  LED (onboard)
#pragma once
#include <stddef.h>
#include <stdint.h>

// ---- nRF24L01+ (via 5 V socket adapter with 3.3 V regulator) ----
constexpr uint8_t PIN_NRF_SCK  = 4;
constexpr uint8_t PIN_NRF_MISO = 5;
constexpr uint8_t PIN_NRF_MOSI = 6;
constexpr uint8_t PIN_NRF_CSN  = 7;
constexpr uint8_t PIN_NRF_CE   = 10;
constexpr uint8_t PIN_NRF_IRQ  = 3;

// ---- Pixhawk 2.4.8 TELEM1 UART (air unit only) ----
// UART1 is remapped to GPIO0/1 on purpose: GPIO20/21 are UART0, which prints ROM boot text at power-up and would
// spray garbage into the Pixhawk RX line.
constexpr uint8_t PIN_PIXHAWK_RX = 1;  // ESP32 RX <- Pixhawk TELEM1 TX (DF13 pin 2)
constexpr uint8_t PIN_PIXHAWK_TX = 0;  // ESP32 TX -> Pixhawk TELEM1 RX (DF13 pin 3)

// ---- PC link over a USB-TTL adapter (ground unit, env `ground_uart` only) ----
// Adapter TX -> GPIO20 (ESP RX), adapter RX <- GPIO21 (ESP TX), common GND. ROM boot text may appear for ~1 s at reset.
constexpr uint8_t PIN_PC_UART_RX = 20;
constexpr uint8_t PIN_PC_UART_TX = 21;

// ---- Status LED ----
constexpr uint8_t PIN_LED = 8;         // onboard LED on most C3 SuperMini boards
constexpr bool LED_ACTIVE_LOW = true;  // VERIFY on your board (flip if the LED logic is inverted)

// ------------------------------------------------------------------------------------------------ compile-time checks
namespace pincheck {

// GPIOs that are broken out on the C3 SuperMini.
constexpr bool exposed(uint8_t p) { return p <= 10 || p == 20 || p == 21; }
// Strapping pins: keep them off the nRF24 bus (they are sampled at reset).
constexpr bool strapping(uint8_t p) { return p == 2 || p == 8 || p == 9; }

constexpr uint8_t kAll[] = {PIN_NRF_SCK,  PIN_NRF_MISO,  PIN_NRF_MOSI,   PIN_NRF_CSN,   PIN_NRF_CE,   PIN_NRF_IRQ,
                            PIN_PIXHAWK_RX, PIN_PIXHAWK_TX, PIN_PC_UART_RX, PIN_PC_UART_TX, PIN_LED};
constexpr uint8_t kNrf[] = {PIN_NRF_SCK, PIN_NRF_MISO, PIN_NRF_MOSI, PIN_NRF_CSN, PIN_NRF_CE, PIN_NRF_IRQ};

constexpr bool allExposed(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; ++i) if (!exposed(p[i])) return false;
  return true;
}
constexpr bool noneStrapping(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; ++i) if (strapping(p[i])) return false;
  return true;
}
constexpr bool allDistinct(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; ++i)
    for (size_t j = i + 1; j < n; ++j)
      if (p[i] == p[j]) return false;
  return true;
}

static_assert(allExposed(kAll, sizeof(kAll)), "pin not broken out on the ESP32-C3 SuperMini");
static_assert(allDistinct(kAll, sizeof(kAll)), "two functions share a GPIO");
static_assert(noneStrapping(kNrf, sizeof(kNrf)), "nRF24 signals must not use strapping pins GPIO2/8/9");
static_assert(PIN_NRF_SCK == 4 && PIN_NRF_MISO == 5 && PIN_NRF_MOSI == 6 && PIN_NRF_CSN == 7,
              "SPI must stay on the C3 SuperMini default pins (SCK4/MISO5/MOSI6/SS7), see header note");
static_assert(PIN_PIXHAWK_RX != 20 && PIN_PIXHAWK_RX != 21 && PIN_PIXHAWK_TX != 20 && PIN_PIXHAWK_TX != 21,
              "Pixhawk UART must avoid UART0 pins (ROM boot text would hit the Pixhawk)");

}  // namespace pincheck
