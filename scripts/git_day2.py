#!/usr/bin/env python3
"""Backward-compatible wrapper: `python scripts/git_day2.py ...` == `python scripts/git_day.py --day 2 ...`."""
import os
import runpy
import sys

sys.argv = [sys.argv[0], "--day", "2"] + sys.argv[1:]
runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), "git_day.py"), run_name="__main__")
