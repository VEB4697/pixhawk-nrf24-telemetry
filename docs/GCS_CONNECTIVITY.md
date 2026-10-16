# Connecting the ground unit to a Ground Control Station (PC & Android)

The ground unit is a **transparent serial bridge**: whatever the GCS writes goes to the Pixhawk, whatever the Pixhawk sends shows up on the GCS port. No special driver or plugin is needed on the GCS.

| GCS | Platform | How it connects |
|---|---|---|
| Mission Planner | Windows (Linux/macOS via Mono) | COM port from the ground unit |
| QGroundControl | Windows / Linux / macOS / **Android** | Serial link (auto-connect on USB) |
| MAVProxy | any | `mavproxy.py --master=<port> --baud 57600` |

## A. Mission Planner (Windows)
1. Flash `ground` (USB CDC) and plug the ground unit in. Device Manager -> *Ports* shows a new COM port (USB Serial Device).
2. Power the vehicle: Pixhawk + air unit. The ground unit LED goes solid within about a second.
3. Mission Planner: top-right port dropdown -> that COM port, baud **57600** (ignored by USB CDC, but must be a valid number; with `ground_uart` use **115200**) -> **Connect**.
4. It should read parameters and show the HUD. Parameter download over this link takes longer than USB - normal.
5. Link quality: MP's packet-loss figure comes from MAVLink sequence numbers. Real RSSI arrives with `RADIO_STATUS` in Week 2.

## B. QGroundControl on PC
Application Settings -> Comm Links -> Add -> Type Serial -> pick the port, baud 57600 -> Connect (or leave auto-connect on).

## C. Android
Mission Planner does not run on Android. Use **QGroundControl for Android** (or another MAVLink GCS with USB-serial support):
1. Phone with USB-C/OTG cable to the ground unit (the phone powers it; the nRF24 PA+LNA module draws ~115 mA peak - fine for most phones, use a powered OTG hub if the phone complains).
2. Open QGC -> it should offer to connect to the USB device automatically.
3. **If the phone does not recognise the ESP32-C3's native USB** (Android serial support for generic CDC-ACM varies by phone/app version): flash `ground_uart` and plug a **CP2102/CP210x USB-TTL adapter** between phone and GPIO20/21. CP210x adapters are universally supported on Android. Baud 115200.
4. Wireless (Wi-Fi/Bluetooth) phone links are not part of v1.0 - the ESP32-C3 radio is kept off to protect the 2.4 GHz link.

## D. Known issue to test first: COM-port open may reset the ESP32-C3
The C3's native USB-Serial-JTAG can reset the chip on certain DTR/RTS sequences (the same lines esptool uses). Most GCS programs only assert DTR and do not reset it, but this must be checked on your PC (test **T-14**): open/close Mission Planner 5 times and watch whether the LED restarts its pattern.
If it resets: use `ground_uart` + USB-TTL adapter (no DTR/RTS wiring to the C3), which also avoids the problem on Android.

## E. Link acquisition ("sync")
There is nothing to pair: both units share channel/address from `config.h`. Power them in any order - the ground unit searches every 20 ms, the air unit starts answering as soon as it hears a poll. In the simulation the link re-forms within 300 ms after an outage; on hardware expect well under 1 s (measure in T-05).
After 500 ms without link both sides discard queued data, so a reconnect never replays old telemetry or old commands.

## F. Reminders
- Telemetry is not a control link: keep RC and failsafes independent.
- Props off for every bench command test (mode changes, arm checks).
- Do not run debug prints on the `ground` (USB CDC) build - they would corrupt MAVLink. Use `ground_uart` / selftest builds for stats.
