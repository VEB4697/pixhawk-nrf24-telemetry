# Hardware Setup - Pixhawk 2.4.8 + ESP32-C3 SuperMini + nRF24L01+PA+LNA

> Verify every pin against your own boards/photos before applying power. Pixhawk 2.4.8 boards are often clones; connectors and silkscreens can differ.

## 1. Pixhawk 2.4.8 TELEM1 (DF13 6-pin) -> Air unit
| DF13 pin | Signal | Wire to |
|---|---|---|
| 1 (red) | +5 V | Air unit 5 V rail (SuperMini `5V` + nRF24 adapter `5V`) |
| 2 | TX (Pixhawk out) | SuperMini **GPIO1** (RX) |
| 3 | RX (Pixhawk in) | SuperMini **GPIO0** (TX) |
| 4 | CTS | not connected |
| 5 | RTS | not connected |
| 6 | GND | common GND |
TX/RX cross. Logic is 3.3 V so no level shifter is needed. Do not use TELEM2 unless you reconfigure `SERIAL2_*`.

## 2. SuperMini <-> nRF24 adapter (both units)
| nRF24 adapter pin | SuperMini | Notes |
|---|---|---|
| SCK | GPIO4 | |
| MISO | GPIO5 | |
| MOSI | GPIO6 | |
| CSN | GPIO7 | |
| CE | GPIO10 | |
| IRQ | GPIO3 | optional now, used later |
| VCC (adapter 5 V in) | 5V pin | adapter regulator makes the module's 3.3 V |
| GND | GND | |
Add a **10-100 uF electrolytic + 0.1 uF ceramic** across the module supply right at the adapter. Keep SPI wires short (< 10 cm).

## 3. Safety checklist (before first power-up)
- [ ] **Antenna screwed on BEFORE power** (a PA module transmitting into no load can be damaged).
- [ ] Multimeter: no short between 5 V and GND on the air unit; DF13 pin 1 = 5 V, pin 6 = GND confirmed on your Pixhawk.
- [ ] TX/RX crossed, not straight through.
- [ ] **Do not power the SuperMini from USB and from the Pixhawk 5 V at the same time** (two 5 V sources can back-feed). For USB flashing/debug, disconnect DF13 pin 1 (5 V) first, or flash on the bench with no Pixhawk attached.
- [ ] Props OFF, vehicle restrained for every command test.
- [ ] Both units use the same channel/address (see `include/config.h`).

## 4. Pixhawk 2.4.8 parameters
**ArduPilot** (TELEM1 = SERIAL1): `SERIAL1_PROTOCOL = 2`, `SERIAL1_BAUD = 57`. If the link stalls after a few seconds, also set `BRD_SER1_RTSCTS = 0` (CTS/RTS are unconnected) - confirm the parameter name for your version.
**PX4:** `MAV_0_CONFIG = TELEM 1`, `SER_TEL1_BAUD = 57600`.
Keep default stream rates first; reduce `SR1_*` only if throughput tests demand it.

## 5. Bench bring-up order
1. Ground unit alone: flash `ground`, confirm "radio ready" + `printPrettyDetails` output (T-01).
2. Air unit alone: same.
3. Power both: expect `state=1 (Linked)` on ground, counters climbing, `lost` near 0 (T-02).
4. Only after T-02 passes: connect the Pixhawk (Day 4).

## 6. Troubleshooting quick table
| Symptom | Likely cause | Fix |
|---|---|---|
| "radio init FAILED" | wiring / no 3.3 V / dead module | recheck SPI + CE/CSN, add capacitor, try spare module |
| Chip detected but no link | channel/address mismatch, antenna off | compare config, fit antenna |
| Link drops when TX power high | supply sag | bigger cap, shorter wires, adapter regulator |
| `getARC` compile error | old RF24 library | update `nrf24/RF24` >= 1.4.11 or remove retries stat |
| LED inverted | `LED_ACTIVE_LOW` wrong | flip in `include/pins.h` |
| No COM port | SuperMini not in CDC mode | BOOT+plug to enter download mode, re-flash |
