# pixhawk-nrf24-telemetry

> Open-source MAVLink telemetry link for **Pixhawk** using **nRF24L01+ (PA+LNA)** and **ESP32-C3**: an air unit that plugs into TELEM1 and a ground unit that shows up as a USB serial port for Mission Planner / QGroundControl.

![status](https://img.shields.io/badge/status-Day%201%20%7C%20project%20base-blue) ![license](https://img.shields.io/badge/license-MIT-green) ![platform](https://img.shields.io/badge/platform-ESP32--C3-orange)

**Owner:** Vaibhav ([VEB4697](https://github.com/VEB4697)) | **Target:** working v1.0 prototype in 7 days, market-ready hardware by Week 4.

## How it works
```
Pixhawk 2.4.8 --TELEM1 UART 57600--> [AIR unit: ESP32-C3 + nRF24 (PRX)] ~~2.4 GHz~~ [GROUND unit: ESP32-C3 + nRF24 (PTX)] --USB-C--> PC (Mission Planner / QGC)
```
The ground unit polls ~500x/s; the air unit answers inside the ACK packet, so MAVLink flows both ways with no collisions. v1.0 is a transparent byte bridge (31 bytes per packet). Details: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Hardware
Pixhawk 2.4.8 | 2x ESP32-C3 SuperMini | 2x nRF24L01+PA+LNA with 3.3 V socket adapters | 3D-printed enclosures. Wiring: [`docs/HARDWARE_SETUP.md`](docs/HARDWARE_SETUP.md). Parts and costs: [`docs/BOM.md`](docs/BOM.md), [`docs/BUDGET.md`](docs/BUDGET.md).

## Quick start (after parts are wired)
```bash
pip install platformio
pio test -e native                 # host unit tests (LinkCore)
pio run -e ground -t upload        # flash ground unit  (hold BOOT while plugging USB if no port appears)
pio run -e air    -t upload        # flash air unit
pio device monitor -b 115200
```
Day 2 bring-up firmware prints link stats once per second; the full MAVLink bridge lands Days 3-5.

## Project status
See [`STATUS.md`](STATUS.md). Verified so far: LinkCore logic passes 17 host tests. Radio and bridge firmware are bring-up drafts awaiting first hardware run.

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
