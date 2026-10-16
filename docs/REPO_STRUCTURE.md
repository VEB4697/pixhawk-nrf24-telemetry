# Repository Structure, Naming & Git Workflow

## Names
| Thing | Name |
|---|---|
| GitHub repo | **`pixhawk-nrf24-telemetry`** (monorepo: firmware + docs + hardware + tools) |
| Description | "Open-source MAVLink telemetry link for Pixhawk using nRF24L01+ and ESP32-C3" |
| Topics | `pixhawk` `mavlink` `nrf24l01` `esp32-c3` `telemetry` `drone` `ardupilot` `platformio` `embedded` |
| Firmware roles | `air` (drone side, PRX) and `ground` (GCS side, PTX) |
| Shared library | `LinkCore` (hardware-free C++) |
| Remote URL | `https://github.com/VEB4697/pixhawk-nrf24-telemetry` |

## Hierarchy
```
pixhawk-nrf24-telemetry/
|-- README.md  STATUS.md  CHANGELOG.md  CONTRIBUTING.md  LICENSE
|-- platformio.ini            # envs: air, ground, native (tests)
|-- .gitignore  .gitattributes  .editorconfig
|-- .github/
|   |-- workflows/ci.yml      # native tests + firmware build
|   |-- ISSUE_TEMPLATE/       # task.md, bug.md
|   `-- pull_request_template.md
|-- include/                  # config.h, pins.h, version.h (shared by both roles)
|-- lib/
|   `-- LinkCore/             # portable core (no Arduino): frame, ring_buffer, packetizer, link_stats
|-- src/
|   |-- common/               # radio_link (RF24 wrapper), status_led
|   |-- air/main.cpp          # PRX firmware
|   `-- ground/main.cpp       # PTX firmware
|-- test/test_core/           # host unit tests (Unity) for LinkCore
|-- test/test_bridge/         # simulated-nRF24 bridge tests (loss, outage, backpressure)
|-- scripts/                  # create_hierarchy.py, git_day2.py (cross-platform CLI helpers)
|-- tools/                    # Python helpers (mavlink_sniff.py, later: throughput logger)
|-- hardware/
|   |-- enclosure/            # CAD + STL exports (3D printed)
|   `-- wiring/               # photos, diagrams, later KiCad
|-- results/                  # curated CSVs + photos (raw/ is ignored)
|-- logs/                     # DAY-NN_date.md daily logs + template
`-- docs/                     # PRD, FEASIBILITY, ARCHITECTURE, BOM, BUDGET, ROADMAP,
                              # TEST_PLAN, DECISIONS, RISKS, FIRMWARE_PRINCIPLES,
                              # HARDWARE_SETUP, REPO_STRUCTURE, DAY2_GUIDE, GCS_CONNECTIVITY
```
Why one repo, one PlatformIO project: both roles share protocol code; one commit keeps air and ground in sync; one CI run covers everything. Roles are separated by `build_src_filter` per environment (ADR-008).

## First-time Git setup (copy-paste)
```bash
cd pixhawk-nrf24-telemetry
python scripts/create_hierarchy.py --check      # verify the tree
git init -b main
git add -A
git commit -m "chore: initial project base (docs, LinkCore, PlatformIO, CI)"
git tag day-01

# create empty repo on GitHub (web) named pixhawk-nrf24-telemetry, then:
git remote add origin https://github.com/VEB4697/pixhawk-nrf24-telemetry.git
git push -u origin main --tags
```
(With GitHub CLI: `gh repo create VEB4697/pixhawk-nrf24-telemetry --public --source=. --push`.)

## Daily Git routine
```bash
git switch -c feat/radio-bringup          # start of work item
# ... code, test ...
pio test -e native && pio run -e air -e ground
git add -A && git commit -m "feat(radio): PTX/PRX counter exchange"
git switch main && git merge --no-ff feat/radio-bringup
# end of day: update STATUS + logs/DAY-NN, then
git add -A && git commit -m "chore(log): day-02 bench link up"
git tag day-02 && git push --follow-tags
```

## Release map
| Tag | Meaning | Planned |
|---|---|---|
| `day-NN` | End-of-day snapshot (restore point) | daily |
| `v0.1.0` | Radio link + throughput measured | Day 3 |
| `v0.5.0` | Pixhawk <-> Mission Planner end-to-end | Day 5 |
| `v1.0.0` | PRD acceptance criteria met | Day 7 |
| `v1.1.0` / `v2.0.0` | Week 2 features / PCB release | Wk 2 / Wk 4 |

## Data-loss safeguards
1. Push every day (`git push --follow-tags`). 2. `day-NN` tags are restore points. 3. Upload `STATUS.md` + latest log to Claude Project knowledge. 4. Weekly zip backup to Drive. 5. Large/raw captures stay local in `results/raw/` (ignored) - back them up separately.
