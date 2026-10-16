# PRD - Pixhawk nRF24 Telemetry Link

**Version:** 0.1 | **Date:** 2026-09-29 | **Owner:** Vaibhav

## 1. Problem
Commercial telemetry radios (SiK 433/915 MHz, ELRS-MAVLink) cost more and are closed for customization. Build a low-cost, hackable, open telemetry link between a Pixhawk and a ground station, and learn the full stack (RF + embedded + MAVLink) while producing a portfolio-grade hardware product.

## 2. Goals
- G1: Transparent, bidirectional MAVLink (v1/v2) bridge Pixhawk <-> GCS over 2.4 GHz nRF24L01.
- G2: Works with Mission Planner / QGroundControl with **no GCS-side software**; the ground unit appears as a USB serial port.
- G3: Total hardware cost for a pair under ~Rs 3,500.
- G4: Documented, reproducible build (repo, wiring, tests).
- G5: (Weeks 2-4) Custom PCB, enclosure, link-quality reporting, frequency hopping.

## 3. Non-goals (v1.0)
- Not a control/RC link. Never the only failsafe path.
- No encryption/authentication (Week 4+ optional).
- No Wi-Fi/BLE use of the ESP32-C3 radio (kept off to avoid self-interference).
- No mesh / multi-vehicle.

## 4. Users
- Primary: Vaibhav (builder/tester of UAV, ROV and rover projects).
- Secondary: makers who reproduce the build from the repo.

## 5. Functional requirements
| ID | Requirement | Priority |
|---|---|---|
| FR-1 | Air unit connects to Pixhawk TELEM port UART at 57600 8N1 (configurable up to 115200) | Must |
| FR-2 | Ground unit exposes USB CDC serial to PC | Must |
| FR-3 | Bidirectional byte-transparent bridge (no MAVLink parsing in v1.0) | Must |
| FR-4 | Auto link acquisition after power-up in any order | Must |
| FR-5 | Link status LED (searching / linked / degraded) | Must |
| FR-6 | Serial debug/stats over USB (packets/s, retries, loss %) | Must |
| FR-7 | Configurable RF channel, data rate, address, PA level (USB CLI + NVS) | Should |
| FR-8 | Inject MAVLink `RADIO_STATUS` so GCS shows link quality | Should (Wk 2) |
| FR-9 | Frequency hopping across a channel list | Could (Wk 2-3) |
| FR-10 | Frame-aware packetization (flush on MAVLink frame boundary) | Should (Wk 2) |
| FR-11 | Watchdog + brown-out safe recovery | Should |

## 6. Non-functional requirements
| ID | Requirement | Target |
|---|---|---|
| NFR-1 | Sustained throughput each direction | >= 5.8 KB/s (57600 baud saturated) at 1 Mbps RF |
| NFR-2 | Typical added latency | < 30 ms one-way |
| NFR-3 | LOS range (PA+LNA, ~5 dBi antennas) | >= 300 m, stretch 800 m |
| NFR-4 | Packet loss at 300 m | < 5% (app level after retries) |
| NFR-5 | Air unit mass | < 25 g without antenna |
| NFR-6 | Air unit power draw | < 250 mA @ 5 V |
| NFR-7 | Legal | RF channel <= 80 (2480 MHz, inside ISM band); low TX power for bench tests; verify current Indian rules for 2.4 GHz and drone operation |
| NFR-8 | Continuous run | 2 h with no lockups |

## 7. Interfaces
- **Air unit <-> Pixhawk:** TELEM1 (JST-GH or DF13 6-pin), 3.3 V UART, 5 V power from Pixhawk.
- **Air <-> Ground:** nRF24L01+ Enhanced ShockBurst, dynamic payloads + ACK payloads.
- **Ground <-> PC:** USB-C, CDC serial.

## 8. Acceptance criteria (v1.0 "done")
1. Mission Planner or QGC connects via the ground unit, downloads params, shows live attitude for 10 min with no disconnects (T-06).
2. Sustained throughput >= 5.8 KB/s both ways on bench (T-03).
3. >= 300 m LOS with < 5% loss (T-08).
4. Link recovers automatically within 3 s after power-cycling either unit (T-05).
5. All docs + firmware in Git, wiring photos in `results/`.

## 9. Milestones
- **v0.1 (Day 3):** two ESP32-C3 exchanging data over nRF24 with measured throughput.
- **v0.5 (Day 5):** full Pixhawk <-> Mission Planner on the bench.
- **v1.0 (Day 7):** range tested, enclosed prototype, docs complete.
- **v1.1 (Wk 2):** RADIO_STATUS, CLI config, frame-aware packetization.
- **v2.0 (Wk 3-4):** custom PCB, FHSS, field-test report, GitHub release + portfolio page.
