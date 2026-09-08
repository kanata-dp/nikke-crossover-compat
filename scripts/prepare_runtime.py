#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Create a local-only runtime view; the installed CrossOver stays untouched."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def prepare(crossover, target, modules=None):
    if target.exists():
        raise ValueError(f"runtime already exists: {target}; choose a new --output")
    source = crossover / "lib/wine/x86_64-unix/ntdll.so"
    bootstrap = ROOT / "build/wine_bootstrap"
    if not source.is_file() or not bootstrap.is_file():
        raise ValueError("CrossOver ntdll or compiled bootstrap is missing")
    target.mkdir(parents=True)
    for entry in crossover.iterdir():
        if entry.name != "lib": (target / entry.name).symlink_to(entry)
    lib = target / "lib"
    lib.mkdir()
    for entry in (crossover / "lib").iterdir():
        if entry.name != "wine": (lib / entry.name).symlink_to(entry)
    wine = lib / "wine"
    wine.mkdir()
    for entry in (crossover / "lib/wine").iterdir():
        if entry.name != "x86_64-unix": (wine / entry.name).symlink_to(entry)
    unix = wine / "x86_64-unix"
    unix.mkdir()
    for entry in source.parent.iterdir():
        if entry.name not in ("ntdll.so", "wine"): (unix / entry.name).symlink_to(entry)
    # ntdll derives its child loader path from its own resolved location.
    # A symlink to ntdll would resolve back into the original application.
    copied = unix / "ntdll.so"
    result = subprocess.run(["/bin/cp", "-c", str(source), str(copied)],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if result.returncode: shutil.copy2(source, copied)
    (unix / "wine").symlink_to(bootstrap)
    if modules:
        replacements = {name: modules / "dlls" / directory / "x86_64-windows" / name
                        for name, directory in (("ntoskrnl.exe", "ntoskrnl.exe"),
                                                ("mfreadwrite.dll", "mfreadwrite"),
                                                ("mfplat.dll", "mfplat"))}
        for module in replacements.values():
            if not module.is_file(): raise ValueError(f"built module missing: {module}")
        windows = wine / "x86_64-windows"
        original = windows.resolve()
        windows.unlink()
        windows.mkdir()
        for entry in original.iterdir():
            if entry.name not in replacements: (windows / entry.name).symlink_to(entry)
        for name, module in replacements.items(): shutil.copy2(module, windows / name)
    (target / "LOCAL_ONLY.txt").write_text(
        "Local runtime view containing third-party binaries and absolute symlinks.\n"
        "Do not publish this directory. Recreate it on each machine.\n")
    return target

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--crossover", type=Path, default=Path(
        "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver"))
    parser.add_argument("--output", type=Path, default=ROOT / "local/runtime")
    parser.add_argument("--modules", type=Path, help="optional output build directory from build_wine_modules.py")
    args = parser.parse_args()
    try: print(prepare(args.crossover.resolve(), args.output.resolve(),
                      args.modules.resolve() if args.modules else None))
    except ValueError as error: parser.error(str(error))

if __name__ == "__main__": main()
