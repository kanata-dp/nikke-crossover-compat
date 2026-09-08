#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
import os
from pathlib import Path
import resource
import signal
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def run(args, bridge=False, expected=0):
    env = os.environ.copy()
    env.pop("DYLD_INSERT_LIBRARIES", None)
    env.pop("NOP_BRIDGE_TRACE", None)
    if bridge: env["DYLD_INSERT_LIBRARIES"] = str(ROOT / "build/libnop_bridge.dylib")
    p = subprocess.run([str(ROOT / "build/native_probe"), *args], env=env,
                       text=True, capture_output=True, timeout=30)
    print(f"native {args or ['basic']} bridge={bridge}: exit={p.returncode}")
    if p.stdout: print(p.stdout.strip())
    if p.returncode != expected:
        raise AssertionError(p.stderr or f"expected exit {expected}, got {p.returncode}")
    return p.stdout

def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    subprocess.run([str(ROOT / "build/decoder_test")], check=True)
    baseline = run([])
    fixed = run([], bridge=True)
    assert "fallback_nops=0" in fixed and "state_preserved=yes" in fixed
    if "fallback_nops=0" in baseline:
        print("Host already supports tested NOPs; no compatibility benefit demonstrated here.")
    run(["--threads"], bridge=True)
    run(["--exhaustion"], bridge=True)
    run(["--simple-handler"], bridge=True)
    for flag in ("--unhandled", "--lock-nop", "--guard-ud2", "--reset-hand"):
        run([flag], bridge=True, expected=-signal.SIGILL)
    print("PASS: native compatibility and rejection controls")

if __name__ == "__main__": main()
