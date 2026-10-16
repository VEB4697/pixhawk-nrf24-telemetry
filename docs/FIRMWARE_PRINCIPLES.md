# Firmware Principles & LinkCore API

## Layering
```
 src/air/main.cpp        src/ground/main.cpp          <- wiring of ports, rings, bridge, LED, health check (~60 lines each)
          |                      |
   include/air_unit.h     include/ground_unit.h       <- per-unit settings: port setup, bridge tuning, printing, variants
          |                      |
          +--- include/config.h  radio_config.h  pins.h  version.h      <- constants + compile-time validation
          +--- src/common/radio_link.*  status_led.h                    <- hardware adapters (RF24, GPIO) - Arduino-only
                            |
                    lib/LinkCore (header-only, NO Arduino)              <- logic: framing, rings, packetizer, stats, bridge, pumps
```
Rule: **logic goes down into LinkCore so it can be unit-tested on the PC; hardware calls stay in adapters; per-unit choices live in the role headers.** Every header is documented in `HEADER_REFERENCE.md`.

## Engineering rules
1. **Transparent stream (v1.0):** never alter, reorder or interpret MAVLink bytes. Bridge = bytes in, same bytes out.
2. **Never block the loop:** no `delay()` in `loop()` (only in `setup()` and the fatal-error blinker). The one blocking call is `RF24::write()` (a few ms max).
3. **No dynamic allocation after `setup()`.** Fixed-size rings (2 KB per direction).
4. **Time is wrap-safe:** always `uint32_t(now - then) >= x`; never compare absolute millis.
5. **Count everything:** every drop, retry, loss and re-init increments a counter shown in the 1 s stats line.
6. **Radio problems must never stall the Pixhawk UART:** UART RX keeps draining into the ring; on overflow the newest bytes are dropped and counted. After link loss the consumer calls `clear()` so stale telemetry is not replayed.
7. **USB console discipline (ground):** once the MAVLink bridge is enabled (Day 5), `Serial` carries *only* MAVLink. Debug prints are compile-time gated (`-DDEBUG_CONSOLE`) or moved to a CLI mode (Week 2).
8. **Config in one place:** `include/config.h` (RF/timing) and `include/pins.h` (hardware). No magic numbers in `main.cpp`.
9. **Every reproducible bug gets a host test** in `test/test_core`.
10. **Document-as-you-go:** a design change = update `DECISIONS.md` the same day.

## LinkCore API (v0.2)
| Header | API | Purpose |
|---|---|---|
| `frame.h` | `packHeader/unpackHeader`, `nextSeq`, `seqGap`, `encodeFrame`, `decodeFrame`, `kMaxData=31` | 1-byte header (4-bit SEQ + 4-bit FLAGS) + <=31 B payload |
| `ring_buffer.h` | `RingBuffer<N>`: `write/read/peek/discard/clear/available/space/dropped` | Lock-free SPSC byte ring, drop-newest on overflow |
| `packetizer.h` | `Packetizer(holdMs).next(ring, nowMs, out)` | Emit packet at 31 B or after hold time; wrap-safe |
| `link_stats.h` | `LinkStats(Role)`: `onTx/onRx/tick/snapshot/state` | Counters, 1 s rates, windowed loss %, duplicate-aware SEQ, Searching/Linked/Degraded |
| `bridge.h` | `PtxBridge<Radio,TxRing,RxRing>`, `PrxBridge<...>`, `BridgeConfig` | The whole transparent bridge; `Radio` is `RadioLink` on target, a simulated radio in tests |
| `io_pump.h` | `pumpIn(port, ring)`, `pumpOut(ring, port)` | Non-blocking, loss-free port <-> ring transfer |
| `selftest.h` | `PatternGen`, `PatternChecker` | Deterministic stream + loss/duplicate classifier for bench tests |

## Running the host tests
- PlatformIO: `pio test -e native` (needs g++; on Windows use WSL or MinGW).
- Without PlatformIO native toolchain (what was used to validate Day 1):
```bash
git clone --depth 1 https://github.com/ThrowTheSwitch/Unity.git /tmp/unity
for t in test_core test_bridge test_firmware test_air_main test_ground_main test_ground_uart_main test_selftest_air test_selftest_ground; do
  g++ -std=gnu++17 -Wall -Wextra -Itest/stubs -Isrc/common -Iinclude -Ilib/LinkCore/src -I/tmp/unity/src \
      test/$t/test_main.cpp /tmp/unity/src/unity.c -o /tmp/$t && /tmp/$t
done
```
Result (Day 3): **63 tests in 8 suites, 0 failures**, zero warnings with `-Wall -Wextra -Wshadow -Wconversion`; clean under `-fsanitize=address,undefined`; loss scenarios swept over 60 random seeds with 0 failures.

## Verification status (honest)
| Item | Status |
|---|---|
| LinkCore incl. bridge logic | Compiled, 31 host tests pass, sanitizers clean, 60-seed loss sweep clean |
| Simulated nRF24 / fake RF24 | Models ESB retries, PID+CRC duplicate suppression, ACK-payload FIFO, RX FIFO limit, config matching, packet/ACK loss, outage, brown-out. It is a *model*: real SPI timing, RF24 library behaviour and interference are only measured on hardware |
| `radio_link.*`, `air/main.cpp`, `ground/main.cpp` (5 env variants) | Compiled and RUN on the host against Arduino/RF24 fakes (63 tests) and syntax-checked for every variant. **First compile with the real ESP32 toolchain + hardware run happens on your machine.** Likely small fixes: RF24/Arduino-ESP32 version APIs (`getARC`, `setTxTimeoutMs`), LED polarity |
| `platformio.ini` | Not run in my sandbox (PlatformIO registry unreachable) |
| `scripts/*.py` | Executed: hierarchy script on empty dir + repo; Git script end-to-end on a scratch repo |
| `tools/mavlink_sniff.py` | Untested draft |

## Flashing the SuperMini
1. `pio run -e air -t upload` (or `-e ground`).
2. If the port does not appear: hold **BOOT**, plug USB (or tap RESET), release BOOT -> download mode, upload again.
3. Serial monitor: `pio device monitor -b 115200` (needs `ARDUINO_USB_CDC_ON_BOOT=1`, already set).
4. Flash/debug one unit at a time so you know which COM port is which (label cables).
