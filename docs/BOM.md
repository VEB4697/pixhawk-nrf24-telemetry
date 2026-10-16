# Bill of Materials (2 units: 1 air + 1 ground)

Prices are **estimates in INR (India, 2026) - verify at order time**. Updated Day 1 after you confirmed: Pixhawk 2.4.8, ESP32-C3 SuperMini, and nRF24L01+PA+LNA modules already owned; enclosures 3D-printed by you.

## Tier A - Core parts
| # | Part | Qty | Spec / note | Est. total | Status |
|---|---|---|---|---|---|
| A1 | ESP32-C3 SuperMini | 2 | USB-C, native USB CDC | 600 | **OWNED** |
| A2 | nRF24L01+ PA+LNA module w/ SMA antenna | 2 | ~100 mW | 600 | **OWNED** (confirm qty >= 2) |
| A3 | nRF24 socket adapter w/ 3.3 V regulator (5 V in) | 2 | needed to power PA+LNA cleanly | 240 | [ ] owned? [ ] to order |
| A4 | Capacitors 10-100 uF electrolytic + 0.1 uF ceramic | 4 + 4 | at each module supply | 50 | [ ] owned? [ ] to order |
| A5 | DF13 6-pin cable (Pixhawk TELEM1 -> DuPont) | 1 | the 3DR radio cable from the Pixhawk kit works | 150 | [ ] owned? [ ] to order |
| A6 | USB-TTL adapter (CP2102/FT232, 3.3 V) | 1 | debug / loopback / sniffing | 150 | [ ] owned? [ ] to order |
| A7 | Perfboard/breadboard, headers, jumper wires, solder | 1 set | | 250 | [ ] owned? [ ] to order |
| A8 | USB-C data cables | 2 | data-capable | 0 | [ ] owned |
| | **Still to buy if you own none of A3-A7** | | | **840** | |

## Tier B - Recommended
| # | Part | Note | Est. | Status |
|---|---|---|---|---|
| B1 | Spare nRF24L01+PA+LNA | clones vary; spare saves days (skip if you own extras) | 300 | [ ] |
| B2 | 3D-print filament for 2 enclosures (PETG) | you print them | 100 | [ ] |
| B3 | Contingency 15% of remaining buys | wrong/burnt parts | 141 | - |

## Tier C - Optional / later
| # | Part | Est. |
|---|---|---|
| C1 | 18650 + holder + charger/boost (portable ground unit) | 300 |
| C2 | 8-ch USB logic analyzer (SPI/UART debugging) | 600 |
| C3 | Custom PCB fab, 5 pcs 2-layer incl. shipping (Week 3) | 1,500 |
| C4 | 5 dBi 2.4 GHz antennas / SMA extension pigtails | 300 |

## Owned / assumed
Pixhawk 2.4.8, laptop with Mission Planner or QGC, soldering iron, multimeter, 3D printer, 5 V USB supply/power bank. Not budgeted: RC gear, vehicle.

## Buying notes
- Use the adapter boards; do not power PA+LNA modules from the SuperMini 3.3 V pin.
- Test every nRF24 on arrival with T-01.
- Order Day 1; each day of delay shifts the whole plan.
