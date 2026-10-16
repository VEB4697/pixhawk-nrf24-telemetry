# Roadmap

**Day 0 = Tue 2026-09-29 (planning, done). Day 1 = Wed 2026-09-30 ... Day 7 = Tue 2026-10-06.**
If you start later, shift dates but keep the day order. Every day ends with the End-of-Session ritual in `README.md`.

Assumption: parts arrive by end of Day 2. Days 1-2 are structured so software progress does not depend on hardware.

## WEEK 1 - Working v1.0 prototype

### Day 1 - Setup, order, software foundation
- [x] Confirm Pixhawk model (2.4.8, DF13 TELEM1), boards (SuperMini), radio modules (owned) - firmware type/version still to note
- [~] BOM updated for owned parts; **you: confirm adapters/cable/caps and order the missing ones (<= Rs 840)**
- [x] Repo base + folder layout + one PlatformIO workspace (envs air/ground/native); **you: `git init` + push**
- [x] Write `frame.h`, `ring_buffer.h`, `packetizer.h`, `link_stats.h` (LinkCore)
- [x] Host-side unit tests: 17/17 pass (framing, ring, packetizer, stats)
- [ ] **You:** install PlatformIO + Mission Planner/QGC; first `pio run -e air -e ground` build; (optional) ArduPilot SITL
- **Exit:** repo pushed, tests pass, first firmware build compiles, missing parts ordered

### Day 2 - Bridge firmware + bench bring-up (software DONE; hardware steps are yours)
- [x] Bridge logic `PtxBridge`/`PrxBridge`, `io_pump`, selftest pattern in LinkCore
- [x] 31 host tests green, incl. simulated nRF24 (loss/ACK loss/outage/backpressure), 60-seed sweep, ASan/UBSan
- [x] AIR + GROUND firmware, 5 envs (`air`, `ground`, `ground_uart`, `air_selftest`, `ground_selftest`) - syntax-checked
- [x] CLI: `scripts/create_hierarchy.py`, `scripts/git_day2.py`; docs `DAY2_GUIDE.md`, `GCS_CONNECTIVITY.md`
- [ ] **You:** `pio test -e native`, then compile all envs (fix any toolchain/API issue)
- [ ] **You:** wire both units, T-01
- [ ] **You:** flash selftest pair -> T-02 (5 min, gaps=0); then T-03 with `SELFTEST_RATE_BPS=0`
- [ ] **You:** run `python scripts/git_day2.py --merge --push` -> tag `day-02`
- **Exit:** T-02 pass

### Day 3 - Headers hardened, pin diagrams, throughput gate G1
- [x] Every `.h` reviewed against its `main.cpp`, optimised and covered by tests (see `HEADER_REFERENCE.md`): new `radio_config.h`, `air_unit.h`, `ground_unit.h`, `timing.h`, `radio_traits.h`; compile-time pin/RF validation; `StatusLed`; `RadioLink` read-back verification; memcpy ring
- [x] 63 host tests incl. the REAL `main.cpp` files vs a simulated peer; ASan/UBSan clean; zero warnings with `-Wall -Wextra -Wshadow -Wconversion`
- [x] Final pin diagrams (SVG) for AIR and GROUND + `PIN_DIAGRAM.md` + `COMPONENTS.md`
- [ ] **You:** first real build of all 5 envs (`pio run ...`), fix any toolchain issue
- [ ] **You:** wire both units per `PIN_DIAGRAM.md`, continuity checklist, T-01
- [ ] **You:** T-02 (5 min, `gaps=0`) then T-03 with `SELFTEST_RATE_BPS=0` at 2 Mbps / 1 Mbps / 250 kbps (change `kRadioConfig.rate`, keep `retryDelay >= minRetryDelay`) -> fill FEASIBILITY table (Gate G1)
- [ ] **You:** T-13 (power-cycle one unit, watch link recover and no stale replay)
- **Exit / Gate G1:** >= 5.8 KB/s sustained at 1 Mbps, `gaps=0`. If not: lower `kServiceIntervalUs`, check SPI wiring/capacitor, try `kPollIntervalUs` 1500
- **Tag:** `v0.1.0`

### Day 4 - Pixhawk integration (UART side)
- [ ] Wire air unit to Pixhawk TELEM1 (double-check pinout, 5 V, TX/RX cross)
- [ ] Set Pixhawk params (ARCHITECTURE sec. 7); verify with USB-TTL sniffing that heartbeats appear
- [ ] Bridge UART -> radio -> ground USB (downlink first), then uplink
- [ ] Verify with a serial monitor / `mavproxy.py` that MAVLink frames arrive intact (T-04)
- **Exit:** heartbeats + attitude flowing Pixhawk -> PC over radio

### Day 5 - GCS end-to-end + Gate G2
- [ ] Connect Mission Planner/QGC to ground unit COM port at 57600
- [ ] Param download, mode change, arm-check message, 10 min soak (T-06)
- [ ] Power-cycle tests in both orders (T-05)
- [ ] LED state machine + 1 s stats print
- **Exit / Gate G2:** stable 10 min session, no disconnects
- **Tag:** `v0.5`

### Day 6 - Range + Gate G3
- [ ] Set PA level HIGH, antennas vertical; test at 10 m, 50 m, 100 m, 200 m, 300 m LOS (T-08)
- [ ] Repeat with RC transmitter ON (T-09)
- [ ] Compare 1 Mbps vs 250 kbps; note stream-rate tweaks needed
- [ ] Record loss %, retries, packets/s per distance in `results/range_test.csv`
- **Exit / Gate G3:** >= 300 m LOS with < 5% loss (or documented gap + plan)

### Day 7 - Package v1.0
- [ ] Mount in enclosures/perfboard cleanup, strain relief on cables
- [ ] 2-hour soak test (T-10) running in background while writing docs
- [ ] Update all docs with measured numbers, wiring photos, final pin map
- [ ] README quick-start, tag `v1.0`, push, backup zip to Drive
- [ ] Retrospective in `logs/DAY-07` + Week 2 backlog
- **Exit:** all PRD acceptance criteria checked (or gaps listed in STATUS)

### Buffer rule
If Day N slips, do **not** silently continue. Write the slip in the log, move unfinished tasks to Day N+1, and drop the lowest-priority item (order: enclosure polish > LED polish > CLI > range-mode tests).

## WEEK 2 - v1.1 (quality of life + robustness)
- [ ] Frame-aware packetization (flush at MAVLink frame boundary; parse STX 0xFD/0xFE + length)
- [ ] `RADIO_STATUS` injection (link quality, txbuf)
- [ ] USB CLI: set channel / rate / PA / address, store in NVS
- [ ] Watchdog + brown-out re-init hardening
- [ ] Automated stats logger in `tools/`
- [ ] Field test #1 (open ground, 500 m+), report in `results/`

## WEEK 3 - Hardware v2
- [ ] KiCad schematic: ESP32-C3 module + nRF24 (module carrier or discrete) + LDO + TELEM connector + USB-C + LED
- [ ] PCB layout w/ antenna keep-out, order boards
- [ ] Frequency hopping design (channel list, sync via ACK) + prototype on perfboard
- [ ] Draft enclosure CAD (CAD skills)

## WEEK 4 - Release
- [ ] Assemble PCB units, regression run of full TEST_PLAN
- [ ] Field test #2 with vehicle in motion (drone/ROV/rover), range + loss report
- [ ] GitHub release v2.0, BOM+Gerbers, assembly guide
- [ ] Portfolio page/card + short demo video
- [ ] Optional: AES-based payload obfuscation/auth, mesh/multi-vehicle exploration

## Definition of Done (each day)
1. Tasks ticked or moved with reason. 2. Code committed. 3. Log + STATUS updated. 4. Measurements saved in `results/`.
