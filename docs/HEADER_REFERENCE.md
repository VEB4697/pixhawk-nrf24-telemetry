# Header Reference (every .h in the firmware)

Day 3 review: every header was checked against its `main.cpp`, hardened with compile-time checks and covered by host tests. "Used by" = who includes it; "Tested by" = which host suite exercises it.

## Board / unit headers (`include/`)
| Header | Purpose | Key contents | Compile-time checks | Used by | Tested by |
|---|---|---|---|---|---|
| `version.h` | Firmware and protocol version | `FW_VERSION`, `FW_PROTOCOL` | - | both mains via role headers | air/ground main tests (boot line) |
| `pins.h` | **Single source of truth for GPIOs** | nRF24 SPI/CE/CSN/IRQ, Pixhawk UART (GPIO0/1), PC UART (GPIO20/21), LED | pins exist on the SuperMini, all distinct, nRF24 not on strapping pins, SPI on default pins, Pixhawk UART avoids UART0 pins | everything | `test_firmware` (helpers), air/ground main tests (pins configured) |
| `radio_config.h` | RF parameters (must match on both units) | `RadioConfig`, `kRadioConfig`, `validRadioConfig`, `minRetryDelay`, `addressOk` | channel <= 80, retry delay >= datasheet minimum for the rate, sane address | `config.h`, `RadioLink` | `test_firmware` |
| `config.h` | Shared constants: baud rates, buffer sizes, timing | `kPixhawkBaud`, `kPcUartBaud`, `kRingSize`, `kPollIntervalUs`, `kLinkTimeoutMs`, `SELFTEST_RATE_BPS` | ring power of two, poll >= 1 ms, search interval vs timeout | role headers | `test_firmware`, main tests |
| `air_unit.h` | AIR-specific: bridge config, port setup, stats/boot printing | `air_unit::Ring/Bridge`, `bridgeConfig()`, `openPixhawkPort()`, `openDebugPort()`, `kServiceIntervalUs`, `printStats()` | radio serviced >= 4x per ground poll | `src/air/main.cpp` | `test_air_main`, `test_selftest_air`, `test_firmware` |
| `ground_unit.h` | GROUND-specific: variants (usb-cdc / usb-ttl / selftest), bridge config, port setup, printing | `ground_unit::kHaveDebug/kMode`, `bridgeConfig()`, `openPcUsb()`, `openPcUart()`, `printStats()` | - | `src/ground/main.cpp` | `test_ground_main`, `test_ground_uart_main`, `test_selftest_ground` |

## Hardware adapter headers (`src/common/`)
| Header | Purpose | Notes | Tested by |
|---|---|---|---|
| `radio_link.h` (+ `.cpp`) | RF24 wrapper: `begin/healthy/reinit/exchange/receive/loadAck/flushTx` | `begin()` validates config, checks chip, verifies channel/rate by read-back; `static_assert`s it satisfies the PTX and PRX radio contracts | `test_firmware` against the fake RF24, plus all main tests |
| `status_led.h` | `StatusLed` + pure `ledPatternOn()` | writes the GPIO only on a level change; fast blink / solid / slow blink; `fatal(n)` error blinker | `test_firmware` |

## Portable core (`lib/LinkCore/src/`, no Arduino dependency)
| Header | Purpose | Day 3 changes | Tested by |
|---|---|---|---|
| `frame.h` | 1-byte header (4-bit SEQ + 4-bit FLAGS) + <= 31 B payload | `constexpr` helpers, `memcpy`, compile-time layout checks | `test_core` |
| `ring_buffer.h` | Lock-free SPSC byte ring | two-segment `memcpy` instead of per-byte loops, **no atomic RMW** (RV32IMC has none), `discard()`, atomic `dropped_` | `test_core` incl. 200,000-op randomised comparison against a `std::deque` model |
| `packetizer.h` | When to cut a packet (31 B or hold time) | uses `elapsed()` | `test_core` |
| `link_stats.h` | Counters, rates, windowed loss, duplicate-aware SEQ, link state | (Day 2) retransmits not counted as loss | `test_core`, all bridge tests |
| `timing.h` | `elapsed()`, `Periodic` (wrap-safe) | **new** | `test_core` |
| `radio_traits.h` | Compile-time radio contract (readable errors) | **new** | `test_core` (static_asserts), `radio_link.h` |
| `bridge.h` | `PtxBridge` / `PrxBridge` / `BridgeConfig` | radio-contract `static_assert`s, `elapsed()` | `test_bridge`, `test_firmware`, main tests |
| `io_pump.h` | Non-blocking loss-free port <-> ring transfer | - | `test_core`, main tests (backpressure) |
| `selftest.h` | `PatternGen` / `PatternChecker` (+ `resync()`) | `resync()` for post-outage baselines | `test_core`, selftest main tests |
| `LinkCore.h` | Umbrella include | adds timing + traits | all |

## Host test suites (`pio test -e native`) - 63 tests
| Suite | Tests | What it proves |
|---|---|---|
| `test_core` | 28 | framing, ring (fuzz), packetizer, stats, pumps, patterns, timing, contracts |
| `test_bridge` | 7 | bridge vs simulated nRF24: lossless, saturation, 30% loss, ACK loss, outage, backpressure, power-on order |
| `test_firmware` | 14 | config/pins helpers, LED, `RadioLink` vs fake RF24 (config applied, failures, brown-out, ACK FIFO, mismatch), bridge through the real `RadioLink` incl. brown-out recovery |
| `test_air_main` | 5 | the REAL `src/air/main.cpp` against a simulated ground unit: boot/ports/pins, exact streaming both ways, outage + re-link, radio brown-out recovery, UART back-pressure |
| `test_ground_main` | 5 | the REAL `src/ground/main.cpp` (USB-CDC) against a simulated air unit: silent MAVLink port, exact streaming, PC stall, outage purge, brown-out |
| `test_ground_uart_main` | 2 | the `ground_uart` variant (CP210x path) |
| `test_selftest_air` / `test_selftest_ground` | 1 + 1 | bench self-test builds report clean streams |
