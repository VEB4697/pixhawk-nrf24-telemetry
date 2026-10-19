#!/usr/bin/env python3
"""Generate hardware/wiring/*.svg pin diagrams (air + ground unit).

Run:  python scripts/gen_pin_diagrams.py
The GPIO numbers below mirror include/pins.h - if you change a pin there, change it here and re-run.
Colours: red = 5 V, black = GND, green = UART, blue = SPI, purple = nRF24 CE/CSN/IRQ, orange = USB / LED.
Board pin ORDER is drawn as commonly published for the ESP32-C3 SuperMini (USB-C at top); the GPIO NAMES are what
matters - always match by silkscreen label.
"""
import os
from xml.sax.saxutils import escape

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "hardware", "wiring")
C = dict(pwr="#d62828", gnd="#222222", uart="#2a9d3f", spi="#1d6fd6", ctl="#8338ec", other="#f77f00", nc="#9aa0a6")

LEFT = ["5V", "GND", "3V3", "GPIO4", "GPIO3", "GPIO2", "GPIO1", "GPIO0"]
RIGHT = ["GPIO5", "GPIO6", "GPIO7", "GPIO8", "GPIO9", "GPIO10", "GPIO20", "GPIO21"]
BX0, BX1, BY0, BY1 = 450, 650, 160, 590
PY0, PDY = 230, 46
LX, RX = BX0 - 28, BX1 + 28
def ly(n): return PY0 + LEFT.index(n) * PDY
def ry(n): return PY0 + RIGHT.index(n) * PDY

# nRF24 adapter pin order is chosen so the right-column SPI wires do not cross each other
NRF = ["VCC", "GND", "MISO", "MOSI", "CSN", "CE", "SCK", "IRQ"]
NX0, NX1, NY0, NY1 = 900, 1170, 160, 590
def ny(n): return PY0 + NRF.index(n) * PDY
NIN = NX0 - 24

def T(x, y, t, size=12, anchor="start", color="#222", weight="400"):
    return f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" fill="{color}" font-weight="{weight}">{escape(t)}</text>'
def W(pts, color, width=3, dash=None):
    d = " ".join(f"{x},{y}" for x, y in pts)
    ex = f' stroke-dasharray="{dash}"' if dash else ""
    return f'<polyline points="{d}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linejoin="round"{ex}/>'
def D(x, y, c): return f'<circle cx="{x}" cy="{y}" r="4.5" fill="{c}"/>'
def box(x, y, w, h, lines, fill, stroke):
    o = [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="10" fill="{fill}" stroke="{stroke}" stroke-width="2"/>']
    for i, ln in enumerate(lines):
        o.append(T(x + w / 2, y + 24 + i * 18, ln, 15 if i == 0 else 12, "middle", "#111" if i == 0 else "#37474f", "700" if i == 0 else "400"))
    return o
def head(w, h, title, sub):
    return [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}" font-family="Segoe UI, Arial, sans-serif">',
            f'<rect width="{w}" height="{h}" fill="#fbfbfd"/>',
            T(w / 2, 32, title, 24, "middle", "#111", "700"), T(w / 2, 54, sub, 13, "middle", "#555")]

def board(active, role):
    o = box(BX0, BY0, BX1 - BX0, BY1 - BY0, ["ESP32-C3 SuperMini", role, "(pin order: verify vs silkscreen)"], "#e8f5e9", "#2e7d32")
    cx = (BX0 + BX1) / 2
    o.append(f'<rect x="{cx-34}" y="{BY0-22}" width="68" height="22" rx="4" fill="#cfd8dc" stroke="#37474f" stroke-width="2"/>')
    o.append(T(cx, BY0 - 7, "USB-C", 11, "middle", "#263238", "700"))
    o.append(T(cx, BY1 - 14, "GPIO8 = onboard status LED", 11, "middle", C["other"], "700"))
    for n in LEFT:
        y, on = ly(n), n in active
        o += [W([(LX, y), (BX0, y)], "#555", 4), T(BX0 + 8, y + 4, n, 12, "start", "#111" if on else "#9aa0a6", "700" if on else "400")]
    for n in RIGHT:
        y, on = ry(n), n in active
        o += [W([(BX1, y), (RX, y)], "#555", 4), T(BX1 - 8, y + 4, n, 12, "end", "#111" if on else "#9aa0a6", "700" if on else "400")]
    return o

def nrf():
    o = box(NX0, NY0, NX1 - NX0, NY1 - NY0, ["nRF24L01+ PA+LNA", "on 5 V socket adapter", "adapter regulator: 5 V -> 3.3 V"], "#fff8e1", "#f9a825")
    for n in NRF:
        y = ny(n)
        o += [W([(NIN, y), (NX0, y)], "#555", 4), T(NX0 + 10, y + 4, n, 12, "start", "#111", "700")]
    cx = (NX0 + NX1) / 2
    o += [T(cx, NY1 - 40, "10-100 uF + 0.1 uF across", 11, "middle", "#444"), T(cx, NY1 - 24, "VCC/GND at the module", 11, "middle", "#444"),
          W([(cx, NY1), (cx, NY1 + 52)], "#444", 6), T(cx, NY1 + 72, "SMA antenna - fit BEFORE power", 12, "middle", "#b71c1c", "700")]
    return o

def nrf_wires():
    o = []
    # right-column SPI/control wires: channels a > b > c so nothing crosses; GPIO10 -> CE is a straight run
    for src, dst, col, chan in (("GPIO5", "MISO", C["spi"], 770), ("GPIO6", "MOSI", C["spi"], 745),
                                ("GPIO7", "CSN", C["ctl"], 720), ("GPIO10", "CE", C["ctl"], 700)):
        y0, y1 = ry(src), ny(dst)
        o.append(W([(RX, y0), (chan, y0), (chan, y1), (NIN, y1)], col))
        o += [D(RX, y0, col), D(NIN, y1, col)]
    # left-column wires routed over the top of the board
    for src, dst, col, xl, ytop, xr in (("GPIO4", "SCK", C["spi"], 395, 112, 858), ("GPIO3", "IRQ", C["ctl"], 372, 130, 840)):
        y0, y1 = ly(src), ny(dst)
        o.append(W([(LX, y0), (xl, y0), (xl, ytop), (xr, ytop), (xr, y1), (NIN, y1)], col))
        o += [D(LX, y0, col), D(NIN, y1, col)]
    # power: board 5V pin -> adapter VCC over the top; common GND along the bottom
    y0, y1 = ly("5V"), ny("VCC")
    o += [W([(LX, y0), (345, y0), (345, 84), (880, 84), (880, y1), (NIN, y1)], C["pwr"]), D(LX, y0, C["pwr"]), D(NIN, y1, C["pwr"])]
    y0, y1 = ly("GND"), ny("GND")
    o += [W([(LX, y0), (330, y0), (330, 640), (872, 640), (872, y1), (NIN, y1)], C["gnd"]), D(LX, y0, C["gnd"]), D(NIN, y1, C["gnd"])]
    # labels in clear space (the right-column signals are identified by colour and by the pin names at both ends)
    o += [T(400, 106, "GPIO4 = SCK", 11, "start", C["spi"], "700"),
          T(352, 78, "5V -> adapter VCC", 11, "start", C["pwr"], "700"),
          T(340, 656, "common GND", 11, "start", C["gnd"], "700")]
    return o

def legend(x, y):
    o = []
    for i, (t, c) in enumerate([("5 V power", C["pwr"]), ("GND", C["gnd"]), ("UART", C["uart"]), ("SPI", C["spi"]), ("nRF24 CE / CSN / IRQ", C["ctl"]), ("USB cable", C["other"])]):
        o += [W([(x, y + i * 20), (x + 28, y + i * 20)], c, 4), T(x + 36, y + i * 20 + 4, t, 11)]
    return o

# ------------------------------------------------------------------------------------------------ AIR
def air():
    o = head(1200, 760, "AIR UNIT - wiring (vehicle side, firmware env: air)",
             "Pixhawk 2.4.8 TELEM1 (DF13 6-pin)  <->  ESP32-C3 SuperMini  <->  nRF24L01+PA+LNA   |   UART 57600 8N1, 3.3 V logic")
    o += box(20, 170, 250, 420, ["Pixhawk 2.4.8 - TELEM1", "DF13 6-pin connector"], "#e3f2fd", "#1565c0")
    rows = [("1", "+5V  (out)", C["pwr"]), ("2", "TX (out) -> GPIO1", C["uart"]), ("3", "RX (in) <- GPIO0", C["uart"]),
            ("4", "CTS (not used)", C["nc"]), ("5", "RTS (not used)", C["nc"]), ("6", "GND", C["gnd"])]
    py = {}
    for i, (n, t, col) in enumerate(rows):
        y = 245 + i * 56; py[n] = y
        o += [W([(244, y), (270, y)], "#555", 4), T(34, y + 4, f"pin {n}", 12, "start", "#111", "700"), T(92, y + 4, t, 12, "start", col, "700")]
    o += board({"5V", "GND", "GPIO4", "GPIO3", "GPIO1", "GPIO0", "GPIO5", "GPIO6", "GPIO7", "GPIO10", "GPIO8"}, "AIR  (PRX)")
    # Pixhawk wires: nested channels so the fewest wires cross
    o += [W([(270, py["1"]), (322, py["1"]), (322, ly("5V")), (LX, ly("5V"))], C["pwr"]), D(LX, ly("5V"), C["pwr"]),
          W([(270, py["6"]), (300, py["6"]), (300, ly("GND")), (LX, ly("GND"))], C["gnd"]), D(LX, ly("GND"), C["gnd"]),
          W([(270, py["2"]), (350, py["2"]), (350, ly("GPIO1")), (LX, ly("GPIO1"))], C["uart"]), D(LX, ly("GPIO1"), C["uart"]),
          W([(270, py["3"]), (366, py["3"]), (366, ly("GPIO0")), (LX, ly("GPIO0"))], C["uart"]), D(LX, ly("GPIO0"), C["uart"])]
    o += nrf() + nrf_wires() + legend(20, 625)
    o += [T(300, 735, "Never power the SuperMini from USB and from the Pixhawk 5 V at the same time. Props OFF for all bench command tests.", 12, "start", "#b71c1c", "700")]
    o.append("</svg>")
    return "\n".join(o)

# ------------------------------------------------------------------------------------------------ GROUND
def ground():
    o = head(1200, 760, "GROUND UNIT - wiring (GCS side, firmware env: ground / ground_uart)",
             "PC or Android  <->  ESP32-C3 SuperMini  <->  nRF24L01+PA+LNA   |   default: native USB-C (COM port), fallback: USB-TTL adapter on GPIO20/21")
    o += box(20, 100, 250, 130, ["PC / Android phone", "Mission Planner / QGroundControl", "USB-C data cable (OTG on Android)"], "#e3f2fd", "#1565c0")
    o += [T(145, 188, "env: ground  (COM port)", 12, "middle", "#1b5e20", "700"), T(145, 206, "powers the unit over USB 5 V", 11, "middle", "#555")]
    cx = (BX0 + BX1) / 2
    o += [W([(270, 149), (cx - 34, 149)], C["other"], 5, "10 6"), T(285, 141, "USB-C cable", 11, "start", C["other"], "700")]
    o += box(20, 300, 250, 250, ["USB-TTL adapter", "OPTIONAL: env ground_uart", "CP2102 / CP210x, 3.3 V logic", "115200 baud"], "#fce4ec", "#ad1457")
    o += [T(145, 406, "use if opening the COM port resets", 11, "middle", "#555"), T(145, 422, "the C3, or Android lacks CDC", 11, "middle", "#555"),
          T(145, 438, "(leave adapter VCC unconnected)", 11, "middle", "#555")]
    ar = {"TX": 470, "RX": 506, "GND": 542}
    for t, y in ar.items():
        o += [W([(244, y), (270, y)], "#555", 4), T(34, y + 4, t, 12, "start", C["gnd"] if t == "GND" else C["uart"], "700")]
    o += board({"5V", "GND", "GPIO4", "GPIO3", "GPIO5", "GPIO6", "GPIO7", "GPIO10", "GPIO8", "GPIO20", "GPIO21"}, "GROUND  (PTX)")
    # USB-TTL wires (dashed = only in the ground_uart build), routed below the board
    o += [W([(270, ar["TX"]), (292, ar["TX"]), (292, 690), (688, 690), (688, ry("GPIO20")), (RX, ry("GPIO20"))], C["uart"], 3, "8 5"),
          W([(270, ar["RX"]), (310, ar["RX"]), (310, 712), (706, 712), (706, ry("GPIO21")), (RX, ry("GPIO21"))], C["uart"], 3, "8 5"),
          D(RX, ry("GPIO20"), C["uart"]), D(RX, ry("GPIO21"), C["uart"]),
          W([(270, ar["GND"]), (318, ar["GND"]), (318, 640), (330, 640)], C["gnd"], 3, "8 5"), D(330, 640, C["gnd"]),
          T(340, 684, "adapter TX -> GPIO20 (RX)   [ground_uart only]", 11, "start", C["uart"], "700"),
          T(340, 706, "adapter RX <- GPIO21 (TX)   [ground_uart only]", 11, "start", C["uart"], "700")]
    o += nrf() + nrf_wires() + legend(20, 585)
    o += [T(20, 745, "USB-C 5 V -> SuperMini 5V pin -> adapter VCC. Antenna ON before power. GPIO3 (IRQ) is wired but unused by v1.0 firmware. Debug text only on ground_uart / selftest.", 12, "start", "#b71c1c", "700")]
    o.append("</svg>")
    return "\n".join(o)

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name, fn in (("air_unit_pinout.svg", air), ("ground_unit_pinout.svg", ground)):
        p = os.path.normpath(os.path.join(OUT, name))
        open(p, "w", encoding="utf-8").write(fn())
        print("wrote", p)
