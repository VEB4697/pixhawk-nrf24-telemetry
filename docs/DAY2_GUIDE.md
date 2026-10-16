# Day 2 Guide - Bridge firmware, bench self-test, Git

## What is new on Day 2
| Item | Where | Verified how |
|---|---|---|
| Bridge logic (ground `PtxBridge`, air `PrxBridge`) | `lib/LinkCore/src/bridge.h` | 7 simulated-radio tests, 60-seed loss sweep, ASan/UBSan clean |
| Non-blocking port <-> ring pumps | `lib/LinkCore/src/io_pump.h` | host tests with fake ports (partial writes, full port) |
| Pattern generator/checker for bench tests | `lib/LinkCore/src/selftest.h` | host tests |
| AIR + GROUND firmware (5 build variants) | `src/air`, `src/ground`, `platformio.ini` | syntax-checked for all variants against stubs; **not yet built on the real toolchain** |
| Hierarchy CLI | `scripts/create_hierarchy.py` | run on empty dir + on this repo |
| Day 2 Git CLI | `scripts/git_day2.py` | run end-to-end on a scratch repo |

Total host tests: **31 (24 core + 7 bridge), 0 failures.**

## 1. Hierarchy CLI
```bash
python scripts/create_hierarchy.py --check       # verify the tree (exit 1 if something is missing)
python scripts/create_hierarchy.py               # create missing dirs / stub files (never overwrites)
python scripts/create_hierarchy.py --root D:\work\pixhawk-nrf24-telemetry --git   # new location + git init
```

## 2. Build & flash
```bash
pip install platformio
pio test -e native                         # 31 host tests
pio run -e air -e ground -e ground_uart -e air_selftest -e ground_selftest      # compile everything

pio run -e air_selftest    -t upload       # unit 1  (hold BOOT while plugging USB if no port appears)
pio run -e ground_selftest -t upload       # unit 2
pio device monitor -b 115200               # one terminal per unit (label the cables!)
```
| Env | Use |
|---|---|
| `air` | Vehicle unit on Pixhawk TELEM1 |
| `ground` | PC unit, USB CDC = COM port for Mission Planner / QGC (silent: only MAVLink on USB) |
| `ground_uart` | PC unit via USB-TTL adapter on GPIO20/21; USB CDC prints stats. Fallback / Android |
| `air_selftest` + `ground_selftest` | Bench pair, no Pixhawk/PC; prints continuity stats |

## 3. Bench test T-02 / T-03 (no Pixhawk needed)
Wire both units (antennas ON, capacitors fitted, see `HARDWARE_SETUP.md`). Flash `air_selftest` and `ground_selftest`, open both monitors.

Expected within ~1 s on each unit (numbers approximate):
```
[GND] link=1 txB/s=5760 rxB/s=5760 fail=0 retries=0 loss=0% lost=0 dup=0 | down rx=... gaps=0 lostB=0 dupB=0 | drop=0 reinit=0
[AIR] link=1 rxB/s=5760 txB/s=5760 rx=... lost=0 dup=0 | up rx=... gaps=0 lostB=0 dupB=0 | drop=0 reinit=0
```
`link`: 0 = searching, 1 = linked, 2 = degraded.

**T-02 pass:** 5 minutes at 1 m: `gaps=0`, `dupB=0`, `fail` ~ 0, `link=1`.
**T-03 (throughput):** rebuild both with the unlimited pattern and read `rxB/s`:
```bash
set PLATFORMIO_BUILD_FLAGS=-DSELFTEST_RATE_BPS=0        # Windows cmd   (Linux/mac: export ...)
pio run -e air_selftest -t upload && pio run -e ground_selftest -t upload
```
Pass: sustained >= 5,800 B/s each way (57600 baud saturated) with `gaps=0`. Ceiling in theory is ~15.5 KB/s (500 polls/s x 31 B); anything below 5.8 KB/s -> Gate G1 actions in `FEASIBILITY.md` (try `kPollIntervalUs`, SPI speed, retry delay, IRQ-driven loop).
Record numbers in `results/` and the Day 2/3 log.

## 4. Full Pixhawk chain (can be started as soon as T-02 passes)
1. Wire air unit to TELEM1 (DF13) per `HARDWARE_SETUP.md`; set Pixhawk params; **never USB + Pixhawk 5 V together**.
2. Flash `air` and `ground`. Plug the ground unit into the PC.
3. Connect Mission Planner / QGC: see [`GCS_CONNECTIVITY.md`](GCS_CONNECTIVITY.md).
4. `python tools/mavlink_sniff.py COMx --baud 57600 --seconds 60` -> `bad%` should be ~0 (T-04).

## 5. Git for Day 2
Scripted (recommended):
```bash
python scripts/git_day2.py --dry-run         # preview
python scripts/git_day2.py                   # tests (if pio present) -> branch feat/day2-bridge -> 9 logical commits
python scripts/git_day2.py --merge --push    # merge to main (--no-ff), tag day-02, push with tags
```
Manual equivalent (what the script does):
```bash
git switch -c feat/day2-bridge
git add lib/LinkCore                      && git commit -m "feat(core): add transparent bridge (PTX/PRX), io_pump, selftest pattern; dup-aware stats, ring discard"
git add test                              && git commit -m "test(core): simulated-nRF24 bridge tests + io_pump/selftest tests"
git add src/common include                && git commit -m "feat(radio): RadioLink flushTx; config, pins, bridge tuning"
git add src/air                           && git commit -m "feat(air): Pixhawk UART <-> radio bridge firmware (PRX) with selftest mode"
git add src/ground                        && git commit -m "feat(ground): USB-CDC / UART <-> radio bridge firmware (PTX) with selftest mode"
git add platformio.ini .github            && git commit -m "build: platformio envs ground_uart, air_selftest, ground_selftest; CI runs all"
git add scripts tools                     && git commit -m "feat(scripts): create_hierarchy and git_day2 CLI helpers"
git add docs README.md CHANGELOG.md CONTRIBUTING.md hardware results && git commit -m "docs: day-2 guide, GCS connectivity, ADR-013..016, risks, test plan"
git add STATUS.md logs                    && git commit -m "chore(log): day-02 bridge firmware + host-simulated tests, STATUS update"
git switch main && git merge --no-ff feat/day2-bridge && git tag day-02 && git push --follow-tags
```

## 6. Day 2 exit checklist
- [ ] `pio test -e native` passes on your PC (31 tests)
- [ ] All 5 firmware envs compile (fix small API issues if any)
- [ ] T-01 sane on both modules
- [ ] T-02 passes (5 min, gaps=0)
- [ ] T-03 number recorded (even if below target)
- [ ] Day 2 commits pushed + `day-02` tag; STATUS + log updated

## 7. Troubleshooting
| Symptom | Likely cause | Fix |
|---|---|---|
| `Serial.setTxTimeoutMs` / `setRxBufferSize` compile error | old Arduino-ESP32 core | update platform (`pio pkg update`) or delete those lines (only non-blocking safeguards) |
| `getARC` compile error | old RF24 | update `nrf24/RF24` to >= 1.4.11 |
| `link=0` forever | channel/address mismatch, antenna missing, no 3.3 V | compare `config.h` on both units; add capacitor |
| `gaps` > 0 in selftest at 1 m | supply noise / weak module / interference | cap at module, shorter wires, try other channel (<= 80) |
| Ground USB silent in `ground` env | by design | use `ground_uart` or `*_selftest` for debug text |
