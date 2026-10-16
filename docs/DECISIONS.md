# Decision Log (ADR-lite)

| ID | Date | Decision | Why | Status |
|---|---|---|---|---|
| ADR-001 | 2026-09-29 | Ground = PTX (poller), Air = PRX (ACK-payload downlink) | Deterministic, no collisions, natural link-loss detect | Accepted |
| ADR-002 | 2026-09-29 | PlatformIO + Arduino framework + RF24 (TMRh20) library | Fastest path for a 7-day schedule; can move to ESP-IDF later | Accepted |
| ADR-003 | 2026-09-29 | v1.0 is a transparent byte bridge (no MAVLink parsing) | Simplest and most compatible; frame-aware in Week 2 | Accepted |
| ADR-004 | 2026-09-29 | Default UART 57600 8N1, RF 1 Mbps | Matches Pixhawk default; 1 Mbps balances speed and range | Accepted (validate Day 3) |
| ADR-005 | 2026-09-29 | Ground unit uses ESP32-C3 native USB CDC as PC interface | No extra USB-serial chip; appears as COM port | Accepted |
| ADR-006 | 2026-09-29 | ESP32-C3 Wi-Fi/BT permanently off; RF channel <= 80 | Avoid self-interference and out-of-band emissions | Accepted |
| ADR-007 | 2026-09-29 | Pin map: SPI SCK4/MISO5/MOSI6/CSN7, CE10, IRQ3, LED GPIO8 | SuperMini default SPI pins, avoids strapping pins 2/8/9 for signals | Accepted (confirm on hardware Day 2) |
| ADR-008 | 2026-09-29 | Monorepo `pixhawk-nrf24-telemetry`, ONE PlatformIO project, envs `air`/`ground`/`native` | Shared protocol code, one CI, one commit keeps both roles in sync | Accepted |
| ADR-009 | 2026-09-29 | `LinkCore` is header-only C++ with no Arduino dependency | Logic testable on PC (17 tests) before hardware arrives | Accepted |
| ADR-010 | 2026-09-29 | Pixhawk UART on UART1 remapped to GPIO0 (TX) / GPIO1 (RX), not GPIO20/21 | UART0 pins print ROM boot text at power-up -> garbage into Pixhawk RX | Accepted |
| ADR-011 | 2026-09-29 | Ring buffers drop NEWEST bytes on overflow; consumer `clear()`s stale data on link loss | Keeps lock-free SPSC safe; stale telemetry not replayed | Accepted |
| ADR-012 | 2026-09-29 | MIT license for software; hardware license decided Week 3 | Permissive, portfolio friendly | Accepted |
| ADR-013 | 2026-09-30 | Bridge algorithm: ground polls every 2 ms; air keeps 2 ACK payloads queued; uplink at-least-once (pending chunk retried), downlink best-effort | Matches nRF24 hardware semantics; deterministic, no collisions | Accepted |
| ADR-014 | 2026-09-30 | Ground PC link: native USB CDC by default (`ground`), USB-TTL on GPIO20/21 as fallback (`ground_uart`) | USB CDC needs no extra part; fallback covers DTR/RTS reset risk and Android phones without CDC support | Accepted (test T-14) |
| ADR-015 | 2026-09-30 | After 500 ms without link both sides purge queued data; air discards Pixhawk bytes until it hears the first poll | Never replay stale telemetry/commands on reconnect | Accepted |
| ADR-016 | 2026-09-30 | Verification strategy: hardware-free host tests incl. simulated nRF24 (loss, ACK loss, outage, backpressure), seed sweep, ASan/UBSan; hardware self-test envs for the bench | Gives real evidence before hardware exists; selftest envs re-use the same bridge code on the target | Accepted |
| ADR-017 | 2026-10-01 | Per-unit headers `air_unit.h` / `ground_unit.h` + `radio_config.h`; `main.cpp` files only wire things together | One place per unit for ports, bridge tuning and printing; main stays ~60 lines; headers are testable | Accepted |
| ADR-018 | 2026-10-01 | Compile-time validation of pins, RF config (channel <= 80, ARD vs rate, address) and radio contracts | Wrong config fails the build instead of the field test | Accepted |
| ADR-019 | 2026-10-01 | Air unit services the radio every 250 us (not every loop); ring buffers use load/store atomics only and memcpy | Cuts SPI traffic ~100x; ESP32-C3 (RV32IMC) has no atomic RMW | Accepted (measure on hardware) |
| ADR-020 | 2026-10-01 | `RadioLink::begin()` verifies channel/data rate by read-back and rejects invalid configs | Catches SPI wiring faults that a bare `begin()` hides | Accepted |
| ADR-021 | 2026-10-01 | Real `main.cpp` files are compiled into host tests against Arduino/RF24 fakes and a simulated peer | Tests the firmware as shipped, not a copy of its logic | Accepted |

## Template
```
ADR-NNN | date | decision | why | alternatives rejected | status
```
