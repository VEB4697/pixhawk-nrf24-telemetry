# Feasibility Study

**Verdict: FEASIBLE** for a working v1.0 prototype in 7 days *if parts arrive by Day 2-3*. "Market-ready" (PCB, enclosure, validated range) is a Weeks 2-4 goal, not Week 1.

> All numbers below are **estimates from datasheet-level reasoning**. They must be replaced with measurements (see `TEST_PLAN.md`). Log results in `STATUS.md`.

## 1. Data-rate budget
- Pixhawk TELEM at 57600 baud 8N1 = 5,760 B/s max (~46 kbps). Real ArduPilot default streams typically use less than this.
- nRF24 Enhanced ShockBurst with dynamic payload: max 32 B per packet -> 1 header byte + **31 B user data**.
- One exchange = uplink packet + ACK packet carrying downlink payload. Each exchange moves up to 31 B in *both* directions.
- Needed: 5,760 / 31 = **~186 exchanges/s** to saturate 57600 baud.

| RF rate | Est. airtime per exchange (incl. turnaround) | Theoretical exchanges/s | Realistic (SPI + firmware overhead, ~40-60%) | Verdict for 57600 |
|---|---|---|---|---|
| 2 Mbps | ~0.7 ms | ~1400 | 500-800 | Easy, shortest range |
| **1 Mbps** | ~1.1 ms | ~900 | 350-500 | **Comfortable - default** |
| 250 kbps | ~2.5 ms | ~400 | 150-250 | Marginal - needs reduced stream rates; best range |

Conclusion: **1 Mbps is the v1.0 default.** 250 kbps is a range-mode option tested on Day 6.

## 2. Range (LOS) estimate
- PA+LNA modules: ~+20 dBm TX (about 100 mW) and roughly -94 dBm RX sensitivity at 1 Mbps (-104 dBm at 250 kbps) per nRF24L01+ datasheet.
- Cheap modules, clone chips and poor antennas erode this. Plan on **300-800 m** LOS with ~5 dBi antennas, less near buildings, ground, or 2.4 GHz RC links.
- Target 300 m for acceptance; anything beyond is a bonus, not a promise.

## 3. Power
- ESP32-C3 (radios off): roughly 40-80 mA. nRF24 PA+LNA: ~115 mA peak TX. Total < 250 mA @ 5 V, fine for the Pixhawk TELEM 5 V pin (verify current limit in your board's docs).
- **Do not power the PA+LNA module from the ESP32 3.3 V pin.** Use the socket adapter with its own regulator + 10-100 uF capacitor at the module. Brown-outs on this supply are the #1 cause of flaky nRF24 links.

## 4. Interference / coexistence
- Same 2.4 GHz band as Wi-Fi, ELRS/FrSky/DSM RC links, and BT. Use channels <= 80.
- Keep ESP32-C3 Wi-Fi/BLE **off** (never call `WiFi.begin`, disable BT).
- Test with the RC transmitter ON during range tests (T-09). Telemetry must never be a safety-critical path.

## 5. Regulatory (verify before flying)
- nRF24 channels above 83 (>= 2483.5 MHz) fall outside the ISM band. Keep <= 80.
- Check current WPC rules for 2.4 GHz devices and the Indian Drone Rules for any flight operations. Not legal advice - confirm with official sources.

## 6. Technical feasibility checklist
| Item | Status |
|---|---|
| ESP32-C3 supports Arduino/PlatformIO + RF24 library over SPI | Expected OK (validate Day 2) |
| Two hardware UARTs on C3 + native USB-CDC for ground unit | OK (UART for Pixhawk, USB for PC/debug) |
| Pixhawk TELEM port speaks MAVLink 57600 by default | Yes (ArduPilot/PX4) |
| MAVLink v2 tolerates 30 ms latency & occasional loss | Yes (has CRC, retries at GCS level) |
| Half-duplex handled by PTX polling + ACK payload | Standard nRF24 pattern |

## 7. Alternatives considered
| Option | Approx cost (pair) | Pros | Cons |
|---|---|---|---|
| **This project (nRF24 + ESP32-C3)** | ~Rs 2-3.5k | Cheap, hackable, great learning | DIY effort, range/interference limits |
| ESP-NOW between two ESP32-C3 | ~Rs 700 | No nRF needed, simpler | Shorter range on stock antennas, Wi-Fi-style interference |
| SiK 433/915 MHz radio pair | ~Rs 3.5-6k (verify) | Proven, long range | 915/433 legality/availability, closed |
| ELRS with MAVLink backpack | ~Rs 5-8k (verify) | Long range, robust | Needs ELRS gear |

**Fallback plan:** if nRF24 throughput at range fails NFR-1, reduce SRx stream rates on the Pixhawk (bandwidth need drops 50%+), else pivot to ESP-NOW for v1.0 and keep nRF24 as a research branch.

## 8. Go / No-Go gates
| Gate | When | Criteria | If fail |
|---|---|---|---|
| G1 | End Day 3 | Bench throughput >= 5.8 KB/s at 1 Mbps | Optimize SPI/IRQ, reduce stream rates, try ESP-NOW fallback |
| G2 | End Day 5 | Mission Planner stable 10 min | Debug framing/flush timing |
| G3 | End Day 6 | >= 300 m LOS < 5% loss | Fix antenna/power; try 250 kbps + lower stream rates |

## 9. Honest assessment
- **Week 1 will deliver:** a working, tested prototype on perfboard with docs.
- **Week 1 will not deliver:** PCB, weatherproofing, validated long range, FHSS. Those are Weeks 2-4.
- Biggest schedule risks: parts delivery time, counterfeit/weak nRF24 modules, power-supply noise.
