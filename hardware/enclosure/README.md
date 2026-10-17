# Enclosures (3D printed - Vaibhav)

Two designs: **Air unit** (light, vibration-tolerant, on the vehicle) and **Ground unit** (desk/field, USB-C powered).
Keep CAD sources (`.step`/`.f3d`/`.FCStd`) here and exports (`.stl`/`.3mf`) in `exports/`. Version names: `air_v1`, `ground_v1`.

## Design inputs (measure with calipers before modelling)
| Item | Note |
|---|---|
| ESP32-C3 SuperMini | roughly 22.5 x 18 mm board, USB-C on one short edge, BOOT/RESET buttons must stay reachable on the ground unit |
| nRF24L01+PA+LNA module | measure body + SMA connector; the PA/LNA metal shield and antenna base need clearance |
| nRF24 socket adapter (3.3 V regulator) | measure with capacitors fitted |
| DF13 6-pin cable (air unit) | strain relief / cable exit |

## Requirements
1. Antenna (SMA) exits the case, mounted **vertical**, >= 3-5 cm away from the ESP32 and any metal/carbon fibre.
2. Never print a fully metalised/carbon-filled case around the antenna; use PETG or ABS/ASA (PETG preferred for heat near the vehicle).
3. Status LED visible (light pipe or window).
4. USB-C access on both units (flashing/debug); BOOT/RESET reachable.
5. Air unit: cable strain relief, foam/velcro mounting, no loose boards, ventilation slots.
6. Wall >= 1.6 mm, screw bosses or snap fit, 0.2-0.3 mm clearance on fits.
7. Label printed/engraved: `AIR` / `GND`, firmware version area.

## Status
- [ ] Measurements recorded
- [ ] air_v1 draft
- [ ] ground_v1 draft
- [ ] Test fit with real boards (Day 6-7)
