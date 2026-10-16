# pixhawk-nrf24-telemetry

> Open-source MAVLink telemetry link for **Pixhawk** using **nRF24L01+ (PA+LNA)** and **ESP32-C3**: an air unit that plugs into TELEM1 and a ground unit that shows up as a USB serial port for Mission Planner / QGroundControl.

![status](https://img.shields.io/badge/status-Day%203%20%7C%20headers%20%2B%20pinout-blue) ![license](https://img.shields.io/badge/license-MIT-green) ![platform](https://img.shields.io/badge/platform-ESP32--C3-orange)

**Owner:** Vaibhav ([VEB4697](https://github.com/VEB4697)) | **Target:** working v1.0 prototype in 7 days, market-ready hardware by Week 4.

## How it works
```
Pixhawk 2.4.8 --TELEM1 UART 57600--> [AIR unit: ESP32-C3 + nRF24 (PRX)] ~~2.4 GHz~~ [GROUND unit: ESP32-C3 + nRF24 (PTX)] --USB-C--> PC (Mission Planner / QGC)
```
The ground unit polls ~500x/s; the air unit answers inside the ACK packet, so MAVLink flows both ways with no collisions (theoretical 15.5 KB/s per direction vs 5.76 KB/s needed). v1.0 is a transparent byte bridge (31 bytes per packet); stale data is purged after 500 ms without link. Details: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Hardware
Pixhawk 2.4.8 | 2x ESP32-C3 SuperMini | 2x nRF24L01+PA+LNA with 3.3 V socket adapters | 3D-printed enclosures. Wiring: [`docs/HARDWARE_SETUP.md`](docs/HARDWARE_SETUP.md). Parts and costs: [`docs/BOM.md`](docs/BOM.md), [`docs/BUDGET.md`](docs/BUDGET.md).

## Quick start
```bash
python scripts/create_hierarchy.py --check      # verify the folder tree
pip install platformio
pio test -e native                              # 63 host tests (incl. the real main.cpp files vs a simulated peer)
pio run -e air -e ground -e ground_uart -e air_selftest -e ground_selftest   # compile all variants

# bench test, no Pixhawk needed (see docs/DAY2_GUIDE.md):
pio run -e air_selftest -t upload               # unit 1
pio run -e ground_selftest -t upload            # unit 2
pio device monitor -b 115200

# real system:
pio run -e air -t upload                        # vehicle unit on Pixhawk TELEM1
pio run -e ground -t upload                     # PC unit: appears as a COM port for Mission Planner / QGC
```
Hold BOOT while plugging USB if no port appears. Never connect USB and Pixhawk 5 V to the air unit at the same time.

| Env | Purpose |
|---|---|
| `air` | Pixhawk UART <-> radio (vehicle) |
| `ground` | PC/Android via native USB CDC <-> radio (silent: MAVLink only) |
| `ground_uart` | PC/Android via USB-TTL adapter (GPIO20/21), USB CDC prints stats |
| `air_selftest` / `ground_selftest` | Bench pattern test with live stats |

**GCS:** Mission Planner (PC), QGroundControl (PC/Android), MAVProxy - see [`docs/GCS_CONNECTIVITY.md`](docs/GCS_CONNECTIVITY.md).

## Project status
See [`STATUS.md`](STATUS.md). Verified so far: 63 host tests pass, including the REAL `air` and `ground` `main.cpp` running against a simulated peer with packet loss, outages, brown-outs and back-pressure (sanitizer-clean, zero warnings). The ESP32 toolchain build and the radio itself are still unverified - first real build and bench run are on your hardware.

## Documentation map
| Doc | Purpose |
|---|---|
| [`STATUS.md`](STATUS.md) | Single source of truth: now / next / blockers (update daily) |
| [`docs/PRD.md`](docs/PRD.md) | Requirements & acceptance criteria |
| [`docs/FEASIBILITY.md`](docs/FEASIBILITY.md) | Throughput/range/power math, alternatives, go/no-go gates |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | System design, pin map, OTA frame format, firmware design |
| [`docs/FIRMWARE_PRINCIPLES.md`](docs/FIRMWARE_PRINCIPLES.md) | Coding rules, LinkCore API, test/flash how-to |
| [`docs/HARDWARE_SETUP.md`](docs/HARDWARE_SETUP.md) | Pixhawk 2.4.8 + SuperMini wiring, params, checklist |
| [`docs/REPO_STRUCTURE.md`](docs/REPO_STRUCTURE.md) | Repo name, hierarchy, Git workflow, releases |
| [`docs/PIN_DIAGRAM.md`](docs/PIN_DIAGRAM.md) | **Final pin diagrams** (SVG in `hardware/wiring/`), wire lists, continuity checklist |
| [`docs/COMPONENTS.md`](docs/COMPONENTS.md) | What every component does, power/current, signal levels |
| [`docs/HEADER_REFERENCE.md`](docs/HEADER_REFERENCE.md) | Every `.h` file: purpose, checks, tests |
| [`docs/DAY2_GUIDE.md`](docs/DAY2_GUIDE.md) | Build/flash, bench T-02/T-03, Day 2 Git commands |
| [`docs/GCS_CONNECTIVITY.md`](docs/GCS_CONNECTIVITY.md) | Mission Planner / QGC / Android connection guide |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | Week 1 day-by-day + Weeks 2-4 |
| [`docs/TEST_PLAN.md`](docs/TEST_PLAN.md) | Test cases T-01..T-12 + results |
| [`docs/DECISIONS.md`](docs/DECISIONS.md) / [`docs/RISKS.md`](docs/RISKS.md) | Decision log / risk register |
| [`logs/`](logs/) | Daily logs (copy `_TEMPLATE.md`) |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | Branches, commits, releases |

## Zero-data-loss workflow
**Start of session (5 min):** read `STATUS.md` + last log -> do the day's tasks from `docs/ROADMAP.md`.
**End of session (10 min, non-negotiable):**
1. Copy `logs/_TEMPLATE.md` -> `logs/DAY-NN_YYYY-MM-DD.md` and fill it (done / numbers / problems / decisions / next).
2. Update `STATUS.md`; tick boxes in ROADMAP / BOM / TEST_PLAN; add decisions to DECISIONS.
3. `git add -A && git commit -m "chore(log): day-NN <summary>" && git tag day-NN && git push --follow-tags`
4. Re-upload `STATUS.md` + latest log to the Claude Project knowledge.

**Resume in a new chat - paste:**
> Project: pixhawk-nrf24-telemetry (Pixhawk 2.4.8, ESP32-C3 SuperMini, nRF24L01+PA+LNA). Read STATUS.md and the latest log in Project knowledge. Today is Day N. Continue from "Next actions". Keep docs in sync and give me the updated STATUS.md + today's log at the end.

## Safety & legal
Telemetry is **not** a control or failsafe link. Keep RC link and failsafes independent. Antenna on before power. Stay on RF channels <= 80 and verify current Indian 2.4 GHz and drone rules before flying. Props off for all bench command tests.

## License
MIT - see [`LICENSE`](LICENSE). Hardware design files will get a separate open-hardware license in Week 3.
