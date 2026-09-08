#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Run a program with the experimental bridge in an explicit existing prefix."""
import argparse
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def environment(prefix, crossover, bridge=True, trace=False):
    env = os.environ.copy()
    # Remove inherited process-specific Wine state before a new top-level run.
    for key in ("WINESERVERSOCKET", "WINELOADERNOEXEC", "WINEPRELOADRESERVE",
                "CX_INITIALIZED", "CX_ALT_LOADER_SOCKET"):
        env.pop(key, None)
    loader = ROOT / "build/wine_bootstrap"
    env.update(
        WINEPREFIX=str(prefix), CX_BOTTLE=prefix.name,
        CX_BOTTLE_PATH=str(prefix.parent), CX_ROOT=str(crossover),
        WINESERVER=str(crossover / "bin/wineserver"),
        WINELOADER=str(loader), CX_WINELOADER=str(loader),
        WINEDLLPATH=":".join(str(crossover / p) for p in (
            "lib/wine/x86_64-windows", "lib/wine/i386-windows", "lib/wine")),
        NOP_BRIDGE_NTDLL=str(crossover / "lib/wine/x86_64-unix/ntdll.so"),
        WINEDEBUG="-all",
    )
    # Keep each run explicit. Never inherit another injected library by accident.
    env.pop("DYLD_INSERT_LIBRARIES", None)
    env.pop("NOP_BRIDGE_TRACE", None)
    if bridge:
        env["DYLD_INSERT_LIBRARIES"] = str(ROOT / "build/libnop_bridge.dylib")
    if trace:
        env["NOP_BRIDGE_TRACE"] = "1"
    return env

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--runtime", "--crossover", dest="crossover", type=Path,
                        default=ROOT / "local/runtime")
    parser.add_argument("--without-bridge", action="store_true")
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--debug", default="-all")
    parser.add_argument("--cwd", type=Path)
    parser.add_argument("program")
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    prefix = args.prefix.resolve()
    crossover = args.crossover.resolve()
    if not (prefix / "system.reg").is_file() or not (prefix / "drive_c").is_dir():
        parser.error("--prefix must name an existing Wine prefix; this tool does not create one")
    for file in (ROOT / "build/wine_bootstrap", ROOT / "build/libnop_bridge.dylib",
                 crossover / "lib/wine/x86_64-unix/ntdll.so", crossover / "bin/wineserver"):
        if not file.is_file(): parser.error(f"missing build/runtime file: {file}")
    env = environment(prefix, crossover, not args.without_bridge, args.trace)
    env["WINEDEBUG"] = args.debug
    program = args.program
    if Path(program).is_file(): program = str(Path(program).resolve())
    if args.cwd: os.chdir(args.cwd.resolve())
    os.execve(ROOT / "build/wine_bootstrap",
              [str(ROOT / "build/wine_bootstrap"), program, *args.arguments], env)

if __name__ == "__main__":
    main()
