# Changelog
Format: [Keep a Changelog](https://keepachangelog.com), versions follow SemVer.

## [Unreleased]
### Fixed
- `platformio.ini`: `src/common` (radio_link.h, status_led.h) was missing from the include path of the `air`/`ground` environments; only the host-test environment had it. Added `-Iinclude -Isrc/common` to the shared flags.

### Added
- Day 3: per-unit headers (`air_unit.h`, `ground_unit.h`, `radio_config.h`), `timing.h`, `radio_traits.h`; compile-time validation of pins/RF config; `StatusLed`; `RadioLink` read-back verification; `memcpy` ring buffer without atomic RMW; 250 us air radio service period.
- Day 3: Arduino/RF24 fakes and 63 host tests (8 suites) incl. the real `main.cpp` files against simulated peers; final pin diagrams (`hardware/wiring/*.svg`, `scripts/gen_pin_diagrams.py`); `PIN_DIAGRAM.md`, `COMPONENTS.md`, `HEADER_REFERENCE.md`.
- Day 2: transparent MAVLink bridge (`PtxBridge`/`PrxBridge`), non-blocking `io_pump`, selftest pattern generator/checker; duplicate-aware link stats; `RingBuffer::discard`.
- AIR/GROUND firmware: Pixhawk UART <-> radio, USB-CDC or USB-TTL <-> radio, selftest builds; envs `ground_uart`, `air_selftest`, `ground_selftest`.
- 31 host tests incl. simulated nRF24 (packet/ACK loss, outage, backpressure).
- `scripts/create_hierarchy.py`, `scripts/git_day2.py`; docs `DAY2_GUIDE.md`, `GCS_CONNECTIVITY.md`.
- Project base: docs set (PRD, feasibility, architecture, BOM, budget, roadmap, test plan, risks, decisions).
- PlatformIO workspace with `air`, `ground` and `native` environments.
- `LinkCore` library: frame format, SPSC ring buffer, packetizer, link statistics + state machine.
- 17 host-side unit tests for LinkCore.
- Day 2 bring-up firmware skeletons (PRX counter reply / PTX poll) and RF24 wrapper (`RadioLink`).
- CI: native tests + firmware build on every push.
