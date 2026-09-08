#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Windows/Wine integration tests. Pass a dedicated disposable prefix."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--runtime", type=Path, default=ROOT / "local/runtime")
    args = parser.parse_args()
    cases = [
        ("windows_probe.exe", True, []),
        ("windows_probe.exe", False, ["--expect-bridge"]),
        ("windows_early.exe", True, []),
        ("windows_early.exe", False, ["--expect-bridge"]),
        ("windows_probe.exe", False, ["--spawn-child"]),
    ]
    for name, without, extra in cases:
        cmd = [sys.executable, str(ROOT / "scripts/run_wine.py"),
               "--prefix", str(args.prefix.resolve()),
               "--crossover", str(args.runtime.resolve())]
        if without: cmd.append("--without-bridge")
        cmd += [str(ROOT / "build" / name), *extra]
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=45)
        print(f"{name} bridge={not without} {extra}: exit={p.returncode}")
        print(p.stdout.strip())
        if p.returncode: raise AssertionError(p.stderr or p.stdout)
        assert "state_preserved=yes" in p.stdout or "main_reached=yes" in p.stdout
        if not without: assert "fallback_nops=0" in p.stdout
        if extra == ["--spawn-child"]: assert "child_exit=0" in p.stdout
    print("PASS: Windows execution, process-attach initialization, child inheritance")

if __name__ == "__main__": main()
