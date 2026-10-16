# Architecture

## 1. System block diagram
```
 AIR UNIT (drone/vehicle)                              GROUND UNIT (GCS side)
+-------------------------------+                    +---------------------------------+
| Pixhawk TELEM1                |                    |   PC: Mission Planner / QGC     |
|  TX -> ESP32-C3 RX (GPIO1)    |                    |         ^ USB-C (CDC serial)    |
|  RX <- ESP32-C3 TX (GPIO0)    |                    |         v                       |
|  5V  -> unit power            |   2.4 GHz nRF24    |  ESP32-C3 (USB CDC <-> nRF24)   |
|  GND                          |<==================>|  nRF24L01+PA+LNA + antenna      |
|  ESP32-C3 <-SPI-> nRF24+PA/LNA|  ShockBurst + ACK  |  Role: PTX (polls)              |
|  Role: PRX (answers w/ ACK    |     payloads       |                                 |
|  payload = downlink data)     |                    |                                 |
+-------------------------------+                    +---------------------------------+
```

## 2. Roles (ADR-001)
- **Ground = PTX.** Sends a packet every ~2-3 ms. Payload = uplink bytes (GCS->Pixhawk) or an empty poll.
- **Air = PRX.** Replies via **ACK payload** carrying downlink bytes (Pixhawk->GCS).
- Result: deterministic polling, no collisions, full-duplex-like throughput, natural link-loss detection (no ACK = link down).

## 3. Wiring (proposed - verify against your exact board pinouts)

### 3.1 Pixhawk 2.4.8 TELEM1 (DF13 6-pin)
| Pin | Signal | Connect to |
|---|---|---|
| 1 | VCC 5V | Air unit 5V in |
| 2 | TX (out of Pixhawk) | ESP32-C3 RX (GPIO1) |
| 3 | RX (into Pixhawk) | ESP32-C3 TX (GPIO0) |
| 4 | CTS | not connected (v1.0) |
| 5 | RTS | not connected (v1.0) |
| 6 | GND | common GND |
TX/RX cross over. Signals are 3.3 V - no level shifter needed. **Check your board's pinout doc before applying power.**

### 3.2 ESP32-C3 <-> nRF24 (both units identical)
| nRF24 pin | ESP32-C3 GPIO (proposed) | Notes |
|---|---|---|
| SCK | GPIO4 | SPI clock, up to 10 MHz |
| MISO | GPIO5 | |
| MOSI | GPIO6 | |
| CSN | GPIO7 | |
| CE | GPIO10 | |
| IRQ | GPIO3 | optional but recommended (interrupt-driven) |
| VCC | adapter 3.3 V (from its regulator) | + 10-100 uF and 0.1 uF at module |
| GND | GND | |
Avoid strapping pins GPIO2, GPIO8, GPIO9 for critical signals (GPIO8 is the onboard LED on many SuperMini boards). Pin map fixed in ADR-007 / `include/pins.h`. UART on GPIO0/1 (not 20/21) avoids ROM boot text on the Pixhawk RX line (ADR-010). See `HARDWARE_SETUP.md` for full tables and the checklist.

### 3.3 Power notes
- Use nRF24 **socket adapter with onboard 3.3 V regulator** for PA+LNA modules, fed from 5 V.
- Keep RF module wires short; keep antenna away from the ESP32 and Pixhawk (>= 3-5 cm), vertical polarization on both ends.

## 4. Radio configuration (defaults)
| Parameter | Value |
|---|---|
| Data rate | 1 Mbps (options: 250 kbps range mode, 2 Mbps) |
| Channel | 40 (2440 MHz) initial; must be <= 80; configurable |
| CRC | 16-bit |
| Auto-retransmit | delay 750 us (1500 us for 250 kbps), count 3 |
| Dynamic payloads + ACK payloads | Enabled |
| Address width | 5 bytes, custom pipe address (not default) |
| PA level | LOW for bench, HIGH/MAX for range tests |
| Wi-Fi/BT on ESP32-C3 | OFF |

## 5. Over-the-air frame (max 32 B)
```
byte 0        : [7:4] SEQ (4-bit, wraps) | [3:0] FLAGS (bit0 = has RADIO_STATUS/ctrl, rest reserved)
bytes 1..31   : payload (0-31 bytes of raw serial stream); length = dynamic payload width - 1
```
- Empty poll/ACK = 1 byte (header only).
- SEQ is for statistics/loss detection; delivery is guaranteed by ShockBurst ACK + retries.
- Packetization rule (v1.0): flush TX buffer when **31 bytes accumulated OR 3 ms idle**. Frame-aware flush (Wk 2).

## 6. Firmware design (PlatformIO, Arduino framework, RF24 library)
One PlatformIO workspace, one env per role (ADR-008). Principles and API: `FIRMWARE_PRINCIPLES.md`.
```
lib/LinkCore/src/   frame.h  ring_buffer.h  packetizer.h  link_stats.h   # hardware-free, host-tested
src/common/         radio_link.*  status_led.h                            # RF24 + GPIO adapters
src/air/main.cpp    PRX: UART <-> rings <-> ACK payload, LED, health check
src/ground/main.cpp PTX: USB CDC <-> rings <-> poll loop, LED, health check
include/            config.h  pins.h  version.h
```
Loop responsibilities:
1. UART/USB RX -> TX ring buffer.
2. Radio task: on IRQ or poll timer, pack <=31 B from ring into packet (PTX) / ACK payload (PRX).
3. Received payload -> RX ring -> UART/USB TX.
4. Every 1 s: print stats to debug port; update LED state.
5. Watchdog reset if radio task stalls > 2 s.

LED states: fast blink = searching, solid = linked, slow blink = linked but loss > 10%.

Known nRF24 quirks handled in firmware: keep ACK payload FIFO topped up; flush FIFOs on link re-acquire; re-run `begin()` config if status reads 0x00/0xFF (module brown-out).

### 6.1 Bridge algorithm (implemented in `lib/LinkCore/src/bridge.h`)
**Ground (PTX)** every 2 ms (20 ms while searching): take <= 31 bytes of PC data (or none) -> 1-byte header + data -> `exchange()` -> if ACKed, mark chunk sent, advance SEQ and push the ACK-payload bytes to the PC ring; if not ACKed keep the *same* chunk pending and retry next poll (uplink is at-least-once). After 500 ms without ACK, purge the uplink ring.
**Air (PRX)** every loop: drain received polls into the Pixhawk ring (each consumes one queued ACK payload) -> keep **2** ACK payloads queued, each cut from the Pixhawk ring at 31 bytes or after 2 ms (`Packetizer`) -> if no poll for 500 ms: flush the radio TX FIFO and drop Pixhawk bytes (never hoard stale telemetry).
**Ports:** `pumpIn/pumpOut` move bytes between UART/USB and the rings without blocking (`availableForWrite()`), and remove bytes from a ring only after the port accepted them.
**Capacity:** 500 polls/s x 31 B = 15.5 KB/s per direction in theory vs 5.76 KB/s needed for 57600 baud.

## 7. Pixhawk configuration
**ArduPilot (TELEM1 = SERIAL1):** `SERIAL1_PROTOCOL = 2` (MAVLink2), `SERIAL1_BAUD = 57`. Tune `SR1_*` stream rates if bandwidth is tight.
**PX4:** `MAV_0_CONFIG = TELEM 1`, `SER_TEL1_BAUD = 57600`.
(Confirm parameter names for your firmware version.)

## 8. Link quality reporting (Week 2)
Pixhawk/GCS expect `RADIO_STATUS` (MAVLink msg 109): RSSI proxy from retry count (`ARC_CNT`) and loss %, `txbuf` = free ring buffer %. nRF24 has no true RSSI (only carrier detect), so this is a **derived quality metric**, documented as such.

## 9. Failure modes
| Failure | Behavior |
|---|---|
| Ground unit off | Air unit keeps buffering then drops oldest data; Pixhawk unaffected |
| Link lost | LEDs indicate; air side drops stale telemetry after 500 ms to avoid bursts of old data |
| nRF24 brown-out | Detect bad status register -> re-init radio |
| UART overflow | Drop newest bytes, count in stats; consumer purges stale data on link loss (ADR-011) |
