# Test Plan

Record every run in the Results table + raw data in `results/`.

| ID | Test | Setup | Pass criteria | When |
|---|---|---|---|---|
| T-01 | Module sanity | Each nRF24 on adapter, read STATUS/CONFIG registers | Stable reads (not 0x00/0xFF) over 60 s; RF24 `isChipConnected()` true; rejects fake/dead modules | Day 2 |
| T-02 | Two-way exchange | Ground PTX <-> Air PRX, counter payloads | 0 lost / 0 duplicated over 5 min at 1 Mbps, 1 m | Day 2 |
| T-03 | Sustained throughput | Synthetic 57600-baud stream both directions | >= 5.8 KB/s each way, loss 0%, at 1 Mbps; record 2 Mbps, 250 kbps | Day 3 |
| T-04 | MAVLink integrity | Real Pixhawk downlink, capture at ground USB | CRC-valid MAVLink frames >= 99.9% (use pymavlink parse errors) | Day 4 |
| T-05 | Link recovery | Power-cycle air then ground, and reverse | Re-link <= 3 s, data resumes, no reboot loop | Day 5 |
| T-06 | GCS soak | Mission Planner/QGC via ground unit | 10 min: params download OK, live attitude, no disconnect | Day 5 |
| T-07 | Uplink command | Change flight mode / set a param from GCS | Command acknowledged by Pixhawk (with props OFF) | Day 5 |
| T-08 | LOS range | Open field, antennas vertical, distances 10/50/100/200/300 m+ | < 5% loss, retries logged | Day 6 |
| T-09 | Coexistence | Repeat T-08 at 100 m and 300 m with RC TX ON | < 10% loss; note any degradation | Day 6 |
| T-10 | 2 h soak | Bench, streaming, both units | No lockups, no memory growth, loss < 1% | Day 7 |
| T-11 | Brown-out | Momentary power dip on air unit | Radio re-inits, link back <= 3 s | Wk 2 |
| T-12 | Vehicle in motion | Mounted on vehicle, real flight/drive | No disconnects; log vs distance | Wk 4 |
| T-13 | Outage purge (hardware) | Power-cycle/unplug one unit for 2 s while streaming | `link` -> 0 on both within ~0.6 s; after return link=1 <= 1 s; no old data replayed (selftest `dupB=0`) | Day 3 |
| T-14 | COM-port open/close | Open/close Mission Planner 5x on `ground` | Unit does not reset (LED pattern continuous) | Day 5 |
| T-15 | Android connect | QGC on phone via OTG (CDC or CP210x + `ground_uart`) | HUD live + params | Day 5-6 |

**Host tests (hardware-free, run on every push):** `pio test -e native` = **63 tests in 8 suites**: `test_core` (28), `test_bridge` (7), `test_firmware` (14), `test_air_main` (5), `test_ground_main` (5), `test_ground_uart_main` (2), `test_selftest_air` (1), `test_selftest_ground` (1). They cover framing, ring buffer (200k-op randomised comparison), packetizer, stats, pumps, config/pin helpers, `RadioLink` against a fake RF24 (incl. brown-out), the bridge under packet loss / ACK loss / outage / backpressure, and the REAL `air` and `ground` `main.cpp` files running against a simulated peer. Details: `HEADER_REFERENCE.md`.
**T-02 / T-03 on hardware use the `air_selftest` + `ground_selftest` envs** (see `DAY2_GUIDE.md`).

**Safety:** all Pixhawk command tests with props removed / vehicle restrained. Telemetry is never the only failsafe.

## Results log
| Date | Test | Config | Result | Pass? | Data file |
|---|---|---|---|---|---|
| | | | | | |
