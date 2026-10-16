# STATUS (single source of truth)

**Last updated:** 2026-10-01 (end of Day 3 software work)
**Current phase:** Week 1 / Day 3 - headers hardened + pin diagrams done; hardware bring-up and Gate G1 pending (yours)
**Overall health:** GREEN (no blockers; the real ESP32 toolchain build and all radio numbers are still unverified)

## Hardware confirmed
Pixhawk 2.4.8 (DF13 TELEM1) | ESP32-C3 SuperMini x2 | nRF24L01+PA+LNA (owned) | 3D-printed enclosures (self-made)

## Done
- [x] Day 1: docs, repo, LinkCore, CI
- [x] Day 2: transparent bridge, io_pump, selftest, 5 firmware envs, CLI scripts, GCS guide
- [x] Day 3: every `.h` reviewed against its `main.cpp`, hardened and tested -> `docs/HEADER_REFERENCE.md`
  - new: `radio_config.h`, `air_unit.h`, `ground_unit.h`, `timing.h`, `radio_traits.h`; `StatusLed`; `RadioLink` read-back checks
  - compile-time validation of pins, RF config (channel <= 80, ARD vs data rate, address) and radio contracts
  - optimised: memcpy ring (no atomic RMW), LED writes only on change, radio serviced every 250 us on the air unit
- [x] **63 host tests / 8 suites pass**, incl. the REAL `air` and `ground` `main.cpp` vs simulated peers (loss, outage, brown-out, back-pressure); ASan/UBSan clean; zero warnings with `-Wall -Wextra -Wshadow -Wconversion`
- [x] **Final pin diagrams:** `hardware/wiring/air_unit_pinout.svg`, `ground_unit_pinout.svg` + `docs/PIN_DIAGRAM.md` (tables, wire lists, continuity checklist) + `docs/COMPONENTS.md`
- [x] `scripts/git_day.py --day 3` (and `git_day2.py` wrapper)

## Needs you now
- [ ] `pip install platformio`; `pio test -e native` (63 tests); compile all 5 envs (first real build - expect tiny API fixes)
- [ ] Wire both units per `docs/PIN_DIAGRAM.md` (match by silkscreen name), run the continuity checklist
- [ ] T-01 (module sanity), T-02 (5 min, `gaps=0`) with `air_selftest` + `ground_selftest`
- [ ] T-03 at 1 Mbps (then 2 Mbps / 250 kbps) with `SELFTEST_RATE_BPS=0` -> record `rxB/s` in FEASIBILITY (Gate G1: >= 5,800 B/s)
- [ ] Confirm LED polarity (`LED_ACTIVE_LOW` in `pins.h`)
- [ ] `python scripts/git_day.py --day 3 --merge --push` (tag `day-03`); upload STATUS + DAY-03 log to Project knowledge

## Next actions (Day 4)
1. If G1 passes: wire Pixhawk TELEM1 (DF13) to the air unit, set params, flash `air` + `ground`.
2. `tools/mavlink_sniff.py` integrity check (T-04); Mission Planner connect (T-06); COM-open reset test (T-14).
3. If G1 fails: follow the actions in ROADMAP Day 3 (service interval, SPI wiring/capacitor, poll interval).

## Blockers
- None. Watch: supply noise on the PA+LNA module (capacitor + adapter regulator), first real compile.

## Open questions
- Pixhawk firmware type/version (ArduPilot / PX4)?
- Android phone model (native-USB CDC vs CP210x path)?
- Do the nRF24 adapters you own match the pin names in `PIN_DIAGRAM.md` (VCC GND CE CSN SCK MOSI MISO IRQ)?

## Key numbers (fill as measured)
| Metric | Target | Simulated | Measured (hardware) |
|---|---|---|---|
| Throughput per direction (1 Mbps) | >= 5.8 KB/s | >= 26 KB / 2 s with 250 us service (ideal 15.5 KB/s) | - |
| Downlink delivery @ 20% pkt + 10% ACK loss | >= 98% | >= 99% (60 seeds) | - |
| Re-link after outage / brown-out | < 1 s | < 0.4 s / < 2.5 s incl. health check | - |
| Packet loss @ 300 m LOS | < 5% | n/a | - |
| Range LOS | >= 300 m | n/a | - |

## Last decisions
ADR-017 per-unit headers, ADR-018 compile-time validation, ADR-019 250 us air service + load/store-only ring, ADR-020 radio read-back verification, ADR-021 real main.cpp in host tests (see `docs/DECISIONS.md`).
