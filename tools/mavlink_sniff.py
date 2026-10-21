#!/usr/bin/env python3
"""Count MAVLink traffic on a serial port (TEST_PLAN T-04 integrity check).

Usage:  python tools/mavlink_sniff.py COM5 --baud 57600 --seconds 60
Prints per-message-type counts, bad-frame count and the observed message rate.
Untested draft: run it first against a plain Pixhawk USB connection to confirm the tool itself works.
"""
import argparse
import collections
import time

from pymavlink import mavutil


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="serial port, e.g. COM5 or /dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=57600)
    ap.add_argument("--seconds", type=int, default=60)
    a = ap.parse_args()

    conn = mavutil.mavlink_connection(a.port, baud=a.baud)
    counts = collections.Counter()
    good = bad = 0
    t0 = time.time()
    while time.time() - t0 < a.seconds:
        msg = conn.recv_match(blocking=True, timeout=1)
        if msg is None:
            continue
        kind = msg.get_type()
        if kind == "BAD_DATA":
            bad += 1
        else:
            good += 1
            counts[kind] += 1

    dur = time.time() - t0
    total = good + bad
    print(f"duration {dur:.1f}s  good={good}  bad={bad}  "
          f"bad%={100.0 * bad / total if total else 0:.3f}  rate={good / dur:.1f} msg/s")
    for kind, n in counts.most_common():
        print(f"  {kind:28s} {n}")


if __name__ == "__main__":
    main()
