#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Exercise the experimental Wine APIs in an explicit disposable prefix."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--runtime", type=Path, default=ROOT / "local/runtime")
    parser.add_argument("--with-thread-process", action="store_true")
    args = parser.parse_args()
    names = ("guarded_mutex", "process_image_name", "bugcheck_registration",
             "physical_ranges", "physical_mapping", "thread_process", "privileged_probe")
    subprocess.run(["make", *(f"build/{name}.exe" for name in names)], cwd=ROOT, check=True)
    child = ROOT / "build/image_name_child_abcdef.exe"
    shutil.copyfile(ROOT / "build/process_image_name.exe", child)
    child_windows = "Z:" + str(child).replace("/", "\\")
    cases = [("guarded_mutex", [], False),
             ("process_image_name", [child_windows], False),
             ("bugcheck_registration", [], False),
             ("physical_ranges", [], False),
             ("physical_mapping", [], False),
             ("privileged_probe", [], False),
             ("privileged_probe", ["fixed"], True)]
    if args.with_thread_process: cases.append(("thread_process", [], False))
    for name, extra, privileged in cases:
        env = os.environ.copy()
        env.pop("NOP_BRIDGE_LOG", None)
        env.pop("NOP_BRIDGE_PRIVILEGED", None)
        if privileged: env["NOP_BRIDGE_PRIVILEGED"] = "1"
        command = [sys.executable, str(ROOT / "scripts/run_wine.py"),
                   "--prefix", str(args.prefix.resolve()),
                   "--runtime", str(args.runtime.resolve()),
                   str(ROOT / "build" / (name + ".exe")), *extra]
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=60)
        print(f"{name} privileged={privileged}: exit={result.returncode}", flush=True)
        print(result.stdout.strip(), flush=True)
        if result.returncode: raise RuntimeError(result.stderr or result.stdout)
    print("PASS: tested Wine API contracts and privileged-fault classification")


if __name__ == "__main__": main()
