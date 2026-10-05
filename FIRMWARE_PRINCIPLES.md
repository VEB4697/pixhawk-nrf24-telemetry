# Firmware Principles & LinkCore API

## Layering
```
 src/air/main.cpp   src/ground/main.cpp     <- application: wiring of UART/USB <-> rings <-> radio, LEDs, stats
          |                  |
          +------ src/common/radio_link, status_led   <- hardware adapters (RF24, GPIO) - Arduino-only
                            |
                    lib/LinkCore (header-only, NO Arduino)   <- logic: framing, rings, packetizer, stats
```
Rule: **logic goes down into LinkCore so it can be unit-tested on the PC; hardware calls stay in adapters.**

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

## LinkCore API (v0.1)
| Header | API | Purpose |
|---|---|---|
| `frame.h` | `packHeader/unpackHeader`, `nextSeq`, `seqGap`, `encodeFrame`, `decodeFrame`, `kMaxData=31` | 1-byte header (4-bit SEQ + 4-bit FLAGS) + <=31 B payload |
| `ring_buffer.h` | `RingBuffer<N>`: `write/read/peek/clear/available/space/dropped` | Lock-free SPSC byte ring, drop-newest on overflow |
| `packetizer.h` | `Packetizer(holdMs).next(ring, nowMs, out)` | Emit packet at 31 B or after hold time; wrap-safe |
| `link_stats.h` | `LinkStats(Role)`: `onTx/onRx/tick/snapshot/state` | Counters, 1 s rates, windowed loss %, Searching/Linked/Degraded |

## Running the host tests
- PlatformIO: `pio test -e native` (needs g++; on Windows use WSL or MinGW).
- Without PlatformIO native toolchain (what was used to validate Day 1):
```bash
git clone --depth 1 https://github.com/ThrowTheSwitch/Unity.git /tmp/unity
g++ -std=gnu++17 -Wall -Wextra -Ilib/LinkCore/src -I/tmp/unity/src \
    test/test_core/test_main.cpp /tmp/unity/src/unity.c -o /tmp/test_core && /tmp/test_core
```
Day 1 result: **17 tests, 0 failures**, no warnings with `-Wall -Wextra`.

## Verification status (honest)
| Item | Status |
|---|---|
| LinkCore logic | Compiled + 17/17 host tests pass |
| `radio_link.*`, `air/main.cpp`, `ground/main.cpp` | Syntax-checked against stub headers only. **First real compile + hardware run happens on your machine (Day 2).** Expect small fixes (RF24 version API such as `getARC()`, LED polarity). |
| `platformio.ini` | Not run here (PlatformIO package registry unreachable from my sandbox). Standard SuperMini settings; adjust if upload/CDC differs. |

## Flashing the SuperMini
1. `pio run -e air -t upload` (or `-e ground`).
2. If the port does not appear: hold **BOOT**, plug USB (or tap RESET), release BOOT -> download mode, upload again.
3. Serial monitor: `pio device monitor -b 115200` (needs `ARDUINO_USB_CDC_ON_BOOT=1`, already set).
4. Flash/debug one unit at a time so you know which COM port is which (label cables).
