# Final Pin Diagrams - AIR and GROUND units

Rendered diagrams (open in a browser / VS Code):
- [`hardware/wiring/air_unit_pinout.svg`](../hardware/wiring/air_unit_pinout.svg)
- [`hardware/wiring/ground_unit_pinout.svg`](../hardware/wiring/ground_unit_pinout.svg)

Regenerate after any pin change: `python scripts/gen_pin_diagrams.py` (the GPIO numbers mirror `include/pins.h`, which is the single source of truth and is protected by compile-time checks).

> **Match by silkscreen name, not by position.** The ESP32-C3 SuperMini and the nRF24 socket adapters come from many makers; header order can differ. The GPIO *names* and the nRF24 signal *names* are what matter.

## 1. Master pin table (identical on both units)

| ESP32-C3 SuperMini | Function | Goes to | Colour (diagram) | Unit |
|---|---|---|---|---|
| **GPIO4** | SPI SCK | nRF24 SCK | blue | both |
| **GPIO5** | SPI MISO | nRF24 MISO | blue | both |
| **GPIO6** | SPI MOSI | nRF24 MOSI | blue | both |
| **GPIO7** | SPI CSN (chip select) | nRF24 CSN | purple | both |
| **GPIO10** | CE (chip enable) | nRF24 CE | purple | both |
| **GPIO3** | IRQ (input, optional) | nRF24 IRQ | purple | both (unused by v1.0 firmware) |
| **5V** | 5 V in | Pixhawk TELEM1 pin 1 (air) / USB-C VBUS (ground); also adapter VCC | red | both |
| **GND** | ground | common GND (Pixhawk pin 6, adapter GND, nRF24 GND) | black | both |
| **GPIO1** | UART1 RX | Pixhawk TELEM1 **TX** (pin 2) | green | air |
| **GPIO0** | UART1 TX | Pixhawk TELEM1 **RX** (pin 3) | green | air |
| **GPIO20** | UART RX | USB-TTL adapter **TX** | green (dashed) | ground_uart only |
| **GPIO21** | UART TX | USB-TTL adapter **RX** | green (dashed) | ground_uart only |
| **GPIO8** | status LED (onboard) | - | orange | both |
| GPIO2, GPIO9, 3V3 | not used (GPIO9 = BOOT button, GPIO2/8/9 are strapping pins) | - | grey | - |

Rules enforced at compile time (`include/pins.h`): every pin is broken out on the SuperMini, no two functions share a GPIO, SPI stays on the board's default SPI pins (4/5/6/7), the nRF24 bus never uses strapping pins, the Pixhawk UART never uses the UART0 pins (GPIO20/21).

## 2. AIR unit (vehicle) - env `air`

```
   PIXHAWK 2.4.8                       ESP32-C3 SuperMini                     nRF24L01+PA+LNA
   TELEM1 (DF13 6-pin)                 (USB-C at top)                         on 5 V socket adapter
  +------------------+              +---------------------+                +----------------------+
  | 1  +5V   --------+--------------+ 5V           GPIO5  +----------------+ MISO                 |
  | 2  TX    --------+------------+ | GND          GPIO6  +----------------+ MOSI                 |
  | 3  RX    --------+----------+ | | 3V3          GPIO7  +----------------+ CSN                  |
  | 4  CTS   (n/c)   |          | | | GPIO4        GPIO8  | (LED)          |                      |
  | 5  RTS   (n/c)   |          | | | GPIO3        GPIO9  |                | CE   <- GPIO10       |
  | 6  GND   --------+--------+ | | | GPIO2        GPIO10 +----------------+ SCK  <- GPIO4        |
  +------------------+        | | | | GPIO1  <---- TX(2)  | GPIO20         | IRQ  <- GPIO3        |
                              | | +-+-+ GPIO0  ----> RX(3)| GPIO21         | VCC  <- 5V           |
                              | |       +---------------------+            | GND  <- GND          |
                              +-+-> common GND                              +----------+-----------+
                                                                                       |
                                                                                 SMA antenna (vertical)
```
(The ASCII sketch is schematic; the SVG shows exact routing.)

**Air-unit connection list (12 wires)**

| # | From | To | Signal |
|---|---|---|---|
| 1 | Pixhawk TELEM1 pin 1 (+5 V) | SuperMini `5V` | power |
| 2 | Pixhawk TELEM1 pin 6 (GND) | SuperMini `GND` | ground |
| 3 | Pixhawk TELEM1 pin 2 (TX) | SuperMini `GPIO1` (RX) | UART data to ESP |
| 4 | Pixhawk TELEM1 pin 3 (RX) | SuperMini `GPIO0` (TX) | UART data to Pixhawk |
| 5 | SuperMini `5V` | adapter `VCC` (5 V in) | power |
| 6 | SuperMini `GND` | adapter `GND` | ground |
| 7 | SuperMini `GPIO4` | adapter `SCK` | SPI clock |
| 8 | SuperMini `GPIO5` | adapter `MISO` | SPI data to ESP |
| 9 | SuperMini `GPIO6` | adapter `MOSI` | SPI data to radio |
| 10 | SuperMini `GPIO7` | adapter `CSN` | chip select |
| 11 | SuperMini `GPIO10` | adapter `CE` | radio enable |
| 12 | SuperMini `GPIO3` | adapter `IRQ` | interrupt (optional) |
Pixhawk pins 4 and 5 (CTS/RTS) stay unconnected; if the link stalls set `BRD_SER1_RTSCTS = 0` (see `HARDWARE_SETUP.md`).

## 3. GROUND unit (GCS) - env `ground` (default) or `ground_uart`

```
   PC / ANDROID                         ESP32-C3 SuperMini                      nRF24L01+PA+LNA
  +-------------+   USB-C cable        +---------------------+                +----------------------+
  | Mission     +======================+ USB-C   (5 V + data)|                | same 8 wires as the  |
  | Planner/QGC |  (default: COM port) |  5V, GND, GPIO3..10 +----------------+ air unit (SPI/CE/IRQ |
  +-------------+                      |                     |                | + 5V + GND)          |
                                       |  GPIO20 <- adapter TX  (ground_uart) +----------------------+
   OPTIONAL USB-TTL (CP210x)           |  GPIO21 -> adapter RX  (ground_uart) |
  +-------------+ TX ------------------+ GPIO20                              SMA antenna (vertical)
  |             | RX ------------------+ GPIO21
  |             | GND -----------------+ GND
  +-------------+ (leave VCC open)
```

**Ground-unit connection list**
- 8 wires `SuperMini <-> adapter` exactly as air-unit wires 5-12 (5 V comes from USB-C VBUS through the SuperMini `5V` pin).
- `env ground`: **no other wires** - just the USB-C cable to the PC/phone.
- `env ground_uart` (optional USB-TTL): adapter `TX -> GPIO20`, adapter `RX <- GPIO21`, adapter `GND -> GND`; **leave the adapter's VCC unconnected**; USB-C still powers the unit and prints debug stats.

## 4. nRF24L01+PA+LNA adapter pins

| Adapter pin | Connects to | Notes |
|---|---|---|
| VCC | SuperMini `5V` | adapter regulator makes 3.3 V for the module. **Not** the 3V3 pin |
| GND | common GND | |
| CE | GPIO10 | |
| CSN | GPIO7 | |
| SCK | GPIO4 | |
| MOSI | GPIO6 | |
| MISO | GPIO5 | |
| IRQ | GPIO3 | optional |
Bare 8-pin modules (without the adapter) are a 2x4 header: `GND VCC / CE CSN / SCK MOSI / MISO IRQ`; they need a clean **3.3 V** supply, never 5 V.

## 5. Pre-power continuity checklist (multimeter, nothing powered)
- [ ] Pixhawk DF13 pin 1 = +5 V and pin 6 = GND on **your** board (probe with the Pixhawk powered separately, then power it off).
- [ ] No short between SuperMini `5V` and `GND`; none between adapter `VCC` and `GND`.
- [ ] TX/RX crossed (Pixhawk TX -> GPIO1, Pixhawk RX -> GPIO0).
- [ ] Each of the 6 nRF24 signal wires beeps end to end and no two neighbours beep to each other.
- [ ] Capacitors fitted at the module; antenna screwed on.
- [ ] Air unit: USB **unplugged** while the Pixhawk supplies 5 V (and vice versa).
