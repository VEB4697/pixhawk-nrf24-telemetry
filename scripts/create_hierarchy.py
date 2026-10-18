#!/usr/bin/env python3
"""Create / verify the pixhawk-nrf24-telemetry folder hierarchy.

Idempotent and non-destructive: it only creates what is missing and NEVER overwrites an existing file.

Usage (run from anywhere):
  python scripts/create_hierarchy.py                 # create missing dirs/stub files in the repo root
  python scripts/create_hierarchy.py --root D:\\work\\pixhawk-nrf24-telemetry
  python scripts/create_hierarchy.py --check         # only report what is missing (exit code 1 if anything is)
  python scripts/create_hierarchy.py --dry-run       # show what would be created
  python scripts/create_hierarchy.py --git           # also `git init -b main` if the folder is not a repo yet
"""
import argparse
import subprocess
import sys
from pathlib import Path

DIRS = [
    ".github/workflows", ".github/ISSUE_TEMPLATE",
    "docs", "logs", "scripts", "tools",
    "include", "lib/LinkCore/src",
    "src/common", "src/air", "src/ground",
    "test/test_core", "test/test_bridge", "test/test_firmware", "test/stubs",
    "test/test_air_main", "test/test_ground_main", "test/test_ground_uart_main",
    "test/test_selftest_air", "test/test_selftest_ground",
    "hardware/enclosure/exports", "hardware/wiring",
]

# local-only (git-ignored) folders: created by the script, never reported as missing
LOCAL_DIRS = ["results/raw"]

# path -> stub content (written only when the file does not exist yet)
STUBS = {
    "README.md": "# pixhawk-nrf24-telemetry\n\nSee docs/ for the full documentation set.\n",
    "STATUS.md": "# STATUS\n\n(update at the end of every session)\n",
    "CHANGELOG.md": "# Changelog\n\n## [Unreleased]\n",
    "docs/PRD.md": "# PRD\n",
    "docs/FEASIBILITY.md": "# Feasibility\n",
    "docs/ARCHITECTURE.md": "# Architecture\n",
    "docs/BOM.md": "# BOM\n",
    "docs/BUDGET.md": "# Budget\n",
    "docs/ROADMAP.md": "# Roadmap\n",
    "docs/TEST_PLAN.md": "# Test plan\n",
    "docs/DECISIONS.md": "# Decisions\n",
    "docs/RISKS.md": "# Risks\n",
    "docs/FIRMWARE_PRINCIPLES.md": "# Firmware principles\n",
    "docs/HARDWARE_SETUP.md": "# Hardware setup\n",
    "docs/REPO_STRUCTURE.md": "# Repo structure\n",
    "logs/_TEMPLATE.md": "# DAY-NN - YYYY-MM-DD\n\n## Done\n\n## Measurements\n\n## Problems\n\n## Decisions\n\n## Next actions\n",
    "hardware/wiring/.gitkeep": "",
    "hardware/enclosure/exports/.gitkeep": "",
    "tools/requirements.txt": "pymavlink>=2.4.41\npyserial>=3.5\n",
}

# files that must exist for a healthy repo (reported by --check, never auto-generated: real content lives in git)
REQUIRED = [
    "platformio.ini", "include/config.h", "include/pins.h", "include/version.h",
    "lib/LinkCore/library.json", "lib/LinkCore/src/LinkCore.h", "lib/LinkCore/src/bridge.h",
    "lib/LinkCore/src/frame.h", "lib/LinkCore/src/ring_buffer.h", "lib/LinkCore/src/packetizer.h",
    "lib/LinkCore/src/link_stats.h", "lib/LinkCore/src/io_pump.h", "lib/LinkCore/src/selftest.h",
    "src/common/radio_link.h", "src/common/radio_link.cpp", "src/common/status_led.h",
    "src/air/main.cpp", "src/ground/main.cpp",
    "include/radio_config.h", "include/air_unit.h", "include/ground_unit.h",
    "lib/LinkCore/src/timing.h", "lib/LinkCore/src/radio_traits.h",
    "test/test_core/test_main.cpp", "test/test_bridge/test_main.cpp", "test/test_firmware/test_main.cpp",
    "test/stubs/Arduino.h", "test/stubs/RF24.h", "test/stubs/peer_sim.h",
    "hardware/wiring/air_unit_pinout.svg", "hardware/wiring/ground_unit_pinout.svg",
    ".gitignore", "LICENSE",
]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    ap.add_argument("--check", action="store_true", help="report missing items only")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--git", action="store_true", help="git init -b main if not a repository")
    a = ap.parse_args()
    root: Path = a.root.resolve()

    missing_dirs = [d for d in DIRS if not (root / d).is_dir()]
    missing_stubs = [f for f in STUBS if not (root / f).exists()]
    missing_required = [f for f in REQUIRED if not (root / f).exists()]

    if a.check:
        for d in missing_dirs:
            print(f"MISSING dir  {d}")
        for f in missing_stubs:
            print(f"MISSING file {f}  (stub can be generated)")
        for f in missing_required:
            print(f"MISSING file {f}  (restore from git / the project zip)")
        bad = missing_dirs or missing_stubs or missing_required
        print("hierarchy OK" if not bad else f"{len(missing_dirs)+len(missing_stubs)+len(missing_required)} item(s) missing")
        return 1 if bad else 0

    tag = "[dry-run] " if a.dry_run else ""
    root.mkdir(parents=True, exist_ok=True)
    for d in LOCAL_DIRS:
        if not (root / d).is_dir():
            print(f"{tag}mkdir  {d}  (local, git-ignored)")
            if not a.dry_run:
                (root / d).mkdir(parents=True, exist_ok=True)
    for d in missing_dirs:
        print(f"{tag}mkdir  {d}")
        if not a.dry_run:
            (root / d).mkdir(parents=True, exist_ok=True)
    for f in missing_stubs:
        print(f"{tag}create {f}")
        if not a.dry_run:
            p = root / f
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(STUBS[f], encoding="utf-8", newline="\n")
    for f in missing_required:
        print(f"NOTE: required file missing (not auto-generated): {f}")

    if a.git and not (root / ".git").exists():
        print(f"{tag}git init -b main")
        if not a.dry_run:
            subprocess.run(["git", "init", "-b", "main"], cwd=root, check=True)

    if not (missing_dirs or missing_stubs):
        print("nothing to create - hierarchy already complete")
    return 0


if __name__ == "__main__":
    sys.exit(main())
