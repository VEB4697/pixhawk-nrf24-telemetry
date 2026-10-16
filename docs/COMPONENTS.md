# Components - what each part does and why it is there

> Electrical numbers are typical datasheet/community figures for these module types. Cheap clone boards vary: measure your own units (a USB power meter on the 5 V rail is enough).

## 1. Block overview
```
 Pixhawk 2.4.8 ──UART 57600──> [AIR: ESP32-C3 + nRF24 PA/LNA] ~~~ 2.4 GHz ~~~ [GROUND: ESP32-C3 + nRF24 PA/LNA] ──USB──> PC / Android GCS
   MAVLink v1/v2        (PRX, answers via ACK payloads)                  (PTX, polls 500x/s)        (CDC serial or USB-TTL)
```

## 2. Parts

### 2.1 ESP32-C3 SuperMini (x2) - the brain of each unit
| Item | Detail |
|---|---|
| What | Tiny ESP32-C3 board (single-core 32-bit RISC-V, up to 160 MHz, 4 MB flash), USB-C, ~22.5 x 18 mm |
| Role | Runs the bridge: reads UART/USB bytes into ring buffers, cuts 31-byte chunks, drives the nRF24 over SPI, runs the link state machine, status LED |
| Logic level | 3.3 V on all GPIO (matches Pixhawk TELEM and nRF24 - no level shifters) |
| Used peripherals | SPI (GPIO4/5/6 + CS 7 + CE 10), UART1 (GPIO0/1 on air; GPIO20/21 for `ground_uart`), native USB Serial/JTAG (CDC) on ground, GPIO8 LED |
| Power | 5 V on the `5V` pin (or USB-C) -> on-board 3.3 V regulator. Wi-Fi/Bluetooth are **never started**, so draw is low (tens of mA) and there is no radio interference with the nRF24 |
| Why this board | Native USB means the ground unit needs no USB-serial chip; UART1 can be remapped to any pin, keeping the boot-log pins (GPIO20/21) away from the Pixhawk |
| Watch out | GPIO2/8/9 are strapping pins (kept off the nRF24 bus). GPIO9 = BOOT button (hold while plugging USB to enter download mode). LED polarity varies (`LED_ACTIVE_LOW` in `pins.h`) |
| No atomics | RV32IMC has no atomic read-modify-write; `RingBuffer` uses only load/store atomics on purpose |

### 2.2 nRF24L01+ PA+LNA module with SMA antenna (x2) - the radio
| Item | Detail |
|---|---|
| What | Nordic nRF24L01+ 2.4 GHz transceiver plus external power amplifier and low-noise amplifier, SMA connector, ~2 dBi antenna |
| Role | Carries the bridged bytes. Enhanced ShockBurst gives hardware CRC, auto-ACK, auto-retransmit and **ACK payloads** (the air unit's telemetry rides back inside the acknowledgement of each ground poll) |
| Settings used | 1 Mbps (250 kbps = range mode, 2 Mbps = short range), channel 40 (must be <= 80), 5-byte address, 16-bit CRC, dynamic payload 1-32 B (1 header + 31 data), retries 3 x 750 us |
| Interface | SPI up to 10 MHz + CE + CSN + IRQ |
| Power | 3.3 V. Peak TX current of PA modules is typically ~100-120 mA in short bursts -> needs a **stiff, clean 3.3 V** (see adapter + capacitors) |
| Range | Datasheet-level estimates: 300-800 m LOS with 5 dBi-class antennas, far less near buildings, ground, or other 2.4 GHz traffic. Validated by test T-08, not promised |
| Watch out | Antenna must be fitted before power (a PA transmitting into no load can be damaged). Clone chips (Si24R1) exist; test each module (T-01) |
| Pins | `GND VCC CE CSN SCK MOSI MISO IRQ` |

### 2.3 nRF24 socket adapter with 3.3 V regulator (x2) - the power fix
| Item | Detail |
|---|---|
| What | Small board with an 8-pin socket for the module, a 5 V input, an LDO (commonly AMS1117-3.3) and decoupling capacitors |
| Role | Turns the 5 V rail into a stable 3.3 V for the module. This is the single most important reliability part: PA modules on a weak 3.3 V pin brown out, drop packets or reset their registers |
| Wiring | Adapter `VCC` = 5 V (SuperMini `5V` pin), `GND` common, signals straight to the SuperMini. Logic inputs are 5 V tolerant; the outputs are 3.3 V |
| Dissipation | 5 V -> 3.3 V at ~0.1 A is about 0.2 W in the LDO - fine, but keep it ventilated in the enclosure |

### 2.4 Decoupling capacitors (10-100 uF electrolytic + 0.1 uF ceramic, x2 sets)
Fit across `VCC`/`GND` as close to the nRF24 module as possible (solder to the adapter pins). The electrolytic supplies the TX current bursts; the ceramic kills high-frequency noise. Observe polarity of the electrolytic.

### 2.5 Pixhawk 2.4.8 - TELEM1 (the data source, air unit only)
| Item | Detail |
|---|---|
| Port | TELEM1, DF13 6-pin: 1 +5 V, 2 TX (out), 3 RX (in), 4 CTS, 5 RTS, 6 GND (verify on your board) |
| Protocol | MAVLink v1/v2 at 57600 8N1 (ArduPilot `SERIAL1_PROTOCOL=2`, `SERIAL1_BAUD=57`; PX4 `MAV_0_CONFIG=TELEM 1`, `SER_TEL1_BAUD=57600`) |
| Power | TELEM 5 V is a shared rail; the air unit is designed to stay below ~250 mA |
| Flow control | CTS/RTS not wired; if the link stalls set `BRD_SER1_RTSCTS=0` |
| Role in the project | Produces telemetry (heartbeats, attitude, GPS, params) and accepts commands (modes, params, missions) - the bridge never interprets it |

### 2.6 USB-C cable + PC/Android GCS (ground unit)
Native-USB mode (`ground`): the SuperMini enumerates as a serial port; Mission Planner / QGroundControl open it like any telemetry radio. Powers the unit (5 V). On Android the phone supplies power over OTG.

### 2.7 USB-TTL adapter, CP2102/CP210x (optional, 1)
Fallback PC link for `ground_uart`: adapter TX -> GPIO20, RX <- GPIO21, GND common, **VCC unconnected**. Use it if opening the COM port resets the C3 (test T-14) or if an Android phone does not recognise the C3's native USB (CP210x is widely supported). Also handy as a debug sniffer.

### 2.8 Antennas (SMA, vertical)
Mount both vertically, polarisation-matched, >= 3-5 cm from the ESP32, wiring and any carbon fibre/metal. The ground antenna benefits from height and a clear view of the vehicle.

### 2.9 Status LED (GPIO8, onboard)
| Pattern | Meaning |
|---|---|
| fast blink (100 ms) | Searching - no link |
| solid | Linked |
| slow blink (500 ms) | Linked but >= 10% loss in the last second |
| 3 short blinks, pause, repeat | Fatal: nRF24 init failed (wiring / 3.3 V / capacitor / antenna) |

### 2.10 Enclosures (3D printed, yours)
See `hardware/enclosure/README.md`: SMA exit with antenna clear of electronics, USB-C and BOOT access, LED light-pipe, strain relief, ventilation for the regulator. Prefer PETG/ABS/ASA; no carbon-filled material near the antenna.

## 3. Power architecture
```
AIR:     Pixhawk TELEM1 +5V ──> SuperMini 5V ──(on-board LDO)──> ESP32-C3 3.3 V
                           └──> nRF24 adapter VCC ──(AMS1117)──> nRF24 module 3.3 V  (+10-100 uF +0.1 uF)
GROUND:  USB-C VBUS 5 V ───> SuperMini 5V ──> ESP32-C3 3.3 V    and    adapter VCC ──> nRF24 module 3.3 V
```
Typical current (estimates, measure yours): ESP32-C3 ~30-50 mA, nRF24 PA RX ~50 mA / TX burst ~115 mA, adapter + LED ~10 mA -> ~100-180 mA average-to-peak at 5 V. **Never feed 5 V from USB and from the Pixhawk at the same time.**

## 4. Signal levels and data rates
| Link | Level | Rate | Notes |
|---|---|---|---|
| Pixhawk <-> air ESP32 | 3.3 V UART | 57600 baud = 5.76 KB/s | RX buffer 1 KB, hardware FIFO only 128 B |
| ESP32 <-> nRF24 | 3.3 V SPI | up to 10 MHz | wires < 10 cm |
| Air <-> ground | 2.4 GHz GFSK | 1 Mbps on air; ~15.5 KB/s theoretical per direction (500 polls/s x 31 B) | needs 5.76 KB/s |
| Ground <-> PC | USB 2.0 full speed CDC (or UART 115200 via CP210x) | far above need | |

## 5. Wiring hygiene
Keep SPI wires short and twisted with a ground wire; keep the antenna away from the ESP32 and the Pixhawk; strain-relieve the DF13 cable; never route SPI wires parallel to the antenna cable; solder the capacitors at the module, not at the board edge.
