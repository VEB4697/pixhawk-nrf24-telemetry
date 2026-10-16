# Risk Register

L = likelihood, I = impact (H/M/L). Review weekly.

| ID | Risk | L | I | Mitigation | Owner/Trigger |
|---|---|---|---|---|---|
| R1 | Parts delivery slips past Day 2 | M | H | Order Day 1; do software-only tasks meanwhile; local market for backup modules | Day 2 no parts -> re-plan |
| R2 | Counterfeit/dead nRF24 modules | M | M | Buy spare (B1); T-01 on arrival | T-01 fail |
| R3 | Supply noise/brown-out on PA+LNA | H | H | Adapter w/ regulator, 10-100 uF + 0.1 uF at module, short wires | Random status 0x00/0xFF |
| R4 | Throughput < 5.8 KB/s | M | H | 1 Mbps, IRQ-driven loop, SPI 8-10 MHz, reduce SRx rates, ESP-NOW fallback | Gate G1 |
| R5 | Range < 300 m | M | M | Better antennas, antenna orientation/spacing, 250 kbps mode, lower stream rates | Gate G3 |
| R6 | Interference from RC/Wi-Fi | M | M | Channel scan, pick clean channel, FHSS in Wk 3 | T-09 loss > 10% |
| R7 | Wrong Pixhawk pinout/voltage damages board | L | H | Verify pinout doc + multimeter before power; use TELEM 3.3 V logic | Before Day 4 wiring |
| R8 | Context/data loss between days | M | H | Daily log + STATUS + Git + upload to Project knowledge | Ritual skipped |
| R9 | Scope creep (FHSS, PCB in Week 1) | H | M | Freeze scope to PRD Must items until v1.0 tagged | Any new idea -> Week 2 backlog |
| R10 | Regulatory/flight legality | L | H | Stay in ISM band, low power, verify Indian rules before flights | Before Day 6 field test |
| R11 | Telemetry mistaken for safety link | L | H | Docs state non-goal; keep RC + failsafes independent | Always |
| R12 | Pixhawk 2.4.8 clone quirks: wrong DF13 pinout, TELEM1 flow-control stalls | M | M | Verify pinout with multimeter; set `BRD_SER1_RTSCTS=0` if stalls; test on bench first | Day 4 |
| R13 | USB 5 V and Pixhawk 5 V back-feed into each other on the SuperMini | M | H | Never connect both; disconnect DF13 pin 1 when USB is plugged | Any time both are connected |
| R14 | Firmware drafts not yet compiled on hardware (RF24 API/version differences, LED polarity) | M | L | First build Day 1-2; small fixes expected; CI catches regressions | Day 2 |
| R15 | Opening the COM port (DTR/RTS) resets the ESP32-C3 USB-CDC ground unit | M | M | Test T-14; fallback env `ground_uart` + CP2102 adapter | GCS connect resets LED pattern |
| R16 | Downlink is best-effort: a chunk is lost when all ACKs of an exchange are lost; nRF24 duplicate suppression needs PID + CRC to match, so an alias only drops a packet that is byte-identical to the previous one 4 writes earlier (the 4-bit SEQ in our header makes that practically impossible) | L | L | MAVLink CRC + retries tolerate it; counted in stats; sim shows >= 99% delivery at 20% packet + 10% ACK loss | seqLost / `gaps` rising |
| R17 | Host simulation is not hardware: real SPI timing, RF24 library behaviour, interference | M | M | Selftest envs measure the real numbers on Day 2-3 (T-02/T-03); gate G1 | Measured < 5.8 KB/s |
| R18 | Android phone/app may not enumerate the ESP32-C3 native USB | M | L | Use `ground_uart` + CP210x adapter | Android connect fails |
| R19 | Fake RF24 / Arduino stubs model my understanding of the libraries, not the real chip | M | M | Real-hardware T-01/T-02/T-03 are the arbiter; keep fakes in `test/stubs` simple and documented; fix tests when hardware disagrees | Any hardware result that contradicts a host test |
| R20 | SuperMini pin ORDER on the header may differ from the diagram (makers vary) | L | M | Diagrams and tables use GPIO names; match by silkscreen; continuity checklist in `PIN_DIAGRAM.md` | Before first power-up |
| R21 | Throttled 250 us air service reduces ACK-queue headroom on real hardware | L | L | `kServiceIntervalUs` in `air_unit.h`; saturation test shows <= 15% cost in simulation; lower it if T-03 is short | T-03 below target |
