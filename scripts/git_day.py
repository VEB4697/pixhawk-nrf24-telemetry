#!/usr/bin/env python3
"""Day-N Git helper: turns a day's work into a clean series of logical commits on a feature branch.

Usage (from the repo root):
  python scripts/git_day.py --day 3 --dry-run         # show the commits that would be made
  python scripts/git_day.py --day 3                   # run host tests (if pio found), branch feat/day3-headers, commit groups
  python scripts/git_day.py --day 3 --merge --push    # also merge into main (--no-ff), tag day-03, push with tags

Options: --day N (2 or 3)  --skip-tests  --branch NAME  --tag NAME
Requires: git. Optional: PlatformIO (`pio`) for the pre-commit test run.
"""
import argparse
import os
import shutil
import subprocess
import sys

# (commit message, paths) - paths that do not exist are skipped
DAYS = {
    2: dict(
        branch="feat/day2-bridge", tag="day-02", merge_msg="merge: day-02 bridge firmware",
        groups=[
            ("feat(core): add transparent bridge (PTX/PRX), io_pump, selftest pattern; dup-aware stats, ring discard", ["lib/LinkCore"]),
            ("test(core): simulated-nRF24 bridge tests (loss, ACK loss, outage, backpressure) + io_pump/selftest tests", ["test"]),
            ("feat(radio): RadioLink flushTx; config, pins (PC UART fallback), bridge tuning", ["src/common", "include"]),
            ("feat(air): Pixhawk UART <-> radio bridge firmware (PRX) with selftest mode", ["src/air"]),
            ("feat(ground): USB-CDC / UART <-> radio bridge firmware (PTX) with selftest mode", ["src/ground"]),
            ("build: platformio envs ground_uart, air_selftest, ground_selftest; CI runs all", ["platformio.ini", ".github"]),
            ("feat(scripts): create_hierarchy and git_day CLI helpers", ["scripts", "tools"]),
            ("docs: day-2 guide, GCS connectivity (Mission Planner/QGC/Android), ADR-013..016, risks, test plan",
             ["docs", "README.md", "CHANGELOG.md", "CONTRIBUTING.md", "hardware", "results"]),
            ("chore(log): day-02 bridge firmware + host-simulated tests, STATUS update", ["STATUS.md", "logs"]),
        ]),
    3: dict(
        branch="feat/day3-headers", tag="day-03", merge_msg="merge: day-03 hardened headers, pin diagrams, firmware tests",
        groups=[
            ("feat(core): timing.h, radio_traits.h, memcpy ring without atomic RMW, constexpr frame helpers", ["lib/LinkCore"]),
            ("feat(board): pins/radio_config/config compile-time validation; air_unit.h and ground_unit.h role headers", ["include"]),
            ("feat(radio): RadioLink read-back verification; StatusLed (writes only on change)", ["src/common"]),
            ("refactor(air,ground): main.cpp wired through role headers; 250 us air radio service period", ["src/air", "src/ground"]),
            ("test(firmware): Arduino/RF24 fakes, RadioLink + header tests, real air/ground main.cpp vs simulated peers", ["test"]),
            ("build: native env flags for stubs; CI checks pin diagrams are in sync", ["platformio.ini", ".github"]),
            ("feat(hw): final AIR/GROUND pin diagrams (SVG) + generator; hierarchy script knows new files", ["hardware", "scripts"]),
            ("docs: PIN_DIAGRAM, COMPONENTS, HEADER_REFERENCE; ADR-017..021, risks R19-R21, roadmap/test plan", ["docs", "README.md", "CHANGELOG.md", "CONTRIBUTING.md", "results"]),
            ("chore(log): day-03 hardened headers + pinout, STATUS update", ["STATUS.md", "logs"]),
        ]),
}


def run(cmd, dry, check=True, capture=False):
    print("$ " + " ".join(cmd))
    if dry:
        return subprocess.CompletedProcess(cmd, 0, "", "")
    return subprocess.run(cmd, check=check, text=True, capture_output=capture)


def out(cmd):
    return subprocess.run(cmd, check=True, text=True, capture_output=True).stdout.strip()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--day", type=int, default=3, choices=sorted(DAYS))
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--skip-tests", action="store_true")
    ap.add_argument("--branch", default=None)
    ap.add_argument("--tag", default=None)
    ap.add_argument("--merge", action="store_true", help="merge branch into main (--no-ff) and tag")
    ap.add_argument("--push", action="store_true", help="push (with tags) after commit/merge")
    a = ap.parse_args()
    plan = DAYS[a.day]
    a.branch = a.branch or plan["branch"]
    a.tag = a.tag or plan["tag"]

    if shutil.which("git") is None:
        print("git not found")
        return 2
    try:
        out(["git", "rev-parse", "--is-inside-work-tree"])
    except subprocess.CalledProcessError:
        print("Not inside a git repository. Run:  git init -b main  (see docs/REPO_STRUCTURE.md)")
        return 2

    # 1) tests before committing
    if not a.skip_tests:
        if shutil.which("pio"):
            r = run(["pio", "test", "-e", "native"], a.dry_run, check=False)
            if r.returncode != 0:
                print("Host tests FAILED - fix them before committing (or use --skip-tests).")
                return 1
        else:
            print("NOTE: PlatformIO (`pio`) not found - skipping host tests. Install with: pip install platformio")

    # 2) branch
    current = out(["git", "branch", "--show-current"]) or "main"
    if current != a.branch:
        exists = subprocess.run(["git", "rev-parse", "--verify", "--quiet", a.branch], capture_output=True).returncode == 0
        run(["git", "switch", a.branch] if exists else ["git", "switch", "-c", a.branch], a.dry_run)

    # 3) commit each group that has changes
    made = 0
    for msg, paths in plan["groups"]:
        existing = [p for p in paths if os.path.exists(p)]
        if not existing:
            continue
        run(["git", "add", "--"] + existing, a.dry_run)
        staged = a.dry_run or subprocess.run(["git", "diff", "--cached", "--quiet"]).returncode != 0
        if staged:
            run(["git", "commit", "-m", msg], a.dry_run)
            made += 1
        else:
            print(f"  (no changes for: {msg.split(':')[0]})")

    # 4) anything left over -> one catch-all commit so nothing is lost
    run(["git", "add", "-A"], a.dry_run)
    if a.dry_run or subprocess.run(["git", "diff", "--cached", "--quiet"]).returncode != 0:
        run(["git", "commit", "-m", f"chore: day-{a.day:02d} remaining files"], a.dry_run)
        made += 1

    # 5) merge + tag + push
    if a.merge:
        run(["git", "switch", "main"], a.dry_run)
        run(["git", "merge", "--no-ff", a.branch, "-m", plan["merge_msg"]], a.dry_run)
        run(["git", "tag", "-f", a.tag], a.dry_run)
    if a.push:
        run(["git", "push", "-u", "origin", "HEAD", "--follow-tags"], a.dry_run)

    print(f"\nDone: {made} commit(s)" + (" (dry run)" if a.dry_run else "") + ".")
    if not a.merge:
        print(f"Next: python scripts/git_day.py --day {a.day} --merge --push   (or: git switch main && git merge --no-ff {a.branch})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
