#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Use CrossOver's normal wrapper and per-bottle settings with the bridge."""
import argparse
import os
from pathlib import Path
import shlex
from run_wine import ROOT

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--crossover", type=Path, default=Path(
        "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver"))
    parser.add_argument("--runtime", type=Path, default=ROOT / "local/runtime")
    parser.add_argument("--workdir")
    parser.add_argument("--dll")
    parser.add_argument("--graphics", choices=("dxvk", "d3dmetal", "wined3d"))
    parser.add_argument("--log", type=Path)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--debug", default="-all")
    parser.add_argument("--software-video", action="store_true")
    parser.add_argument("--privileged-faults", action="store_true")
    parser.add_argument("--disable-dxgi-video", action="store_true")
    parser.add_argument("program")
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    prefix = args.prefix.resolve()
    if not (prefix / "system.reg").is_file(): parser.error("existing prefix required")
    runner = args.crossover.resolve() / "bin/wine"
    ntdll = args.runtime.resolve() / "lib/wine/x86_64-unix/ntdll.so"
    for p in (runner, ntdll, ROOT / "build/wine_bootstrap", ROOT / "build/libnop_bridge.dylib"):
        if not p.is_file(): parser.error(f"required file missing: {p}")
    env = os.environ.copy()
    for key in ("CX_INITIALIZED", "WINESERVERSOCKET", "WINELOADERNOEXEC",
                "WINEPRELOADRESERVE", "CX_ALT_LOADER_SOCKET"):
        env.pop(key, None)
    env["CX_BOTTLE_PATH"] = str(prefix.parent)
    settings = {
        "CX_WINELOADER": str((ROOT / "build/wine_bootstrap").resolve()),
        "WINELOADER": str((ROOT / "build/wine_bootstrap").resolve()),
        "NOP_BRIDGE_NTDLL": str(ntdll),
        "WINEDLLPATH": ":".join(str(args.runtime.resolve() / p) for p in (
            "lib/wine/x86_64-windows", "lib/wine/i386-windows", "lib/wine")),
        "DYLD_INSERT_LIBRARIES": str(ROOT / "build/libnop_bridge.dylib"),
        "WINEDEBUG": args.debug,
    }
    if args.trace: settings["NOP_BRIDGE_TRACE"] = "1"
    else: env.pop("NOP_BRIDGE_TRACE", None)
    if args.graphics: settings["CX_GRAPHICS_BACKEND"] = args.graphics
    env.pop("NOP_BRIDGE_MF_SOFTWARE", None)
    if args.software_video: settings["NOP_BRIDGE_MF_SOFTWARE"] = "1"
    env.pop("NOP_BRIDGE_PRIVILEGED", None)
    if args.privileged_faults: settings["NOP_BRIDGE_PRIVILEGED"] = "1"
    env.pop("NOP_BRIDGE_MF_NO_DXGI", None)
    if args.disable_dxgi_video: settings["NOP_BRIDGE_MF_NO_DXGI"] = "1"
    cmd = [str(runner), "--bottle", prefix.name, "--no-update", "--no-gui",
           "--debugmsg", args.debug, "--env", shlex.join(f"{k}={v}" for k,v in settings.items())]
    if args.workdir: cmd += ["--workdir", args.workdir]
    if args.dll: cmd += ["--dll", args.dll]
    if args.log: cmd += ["--cx-log", str(args.log.resolve())]
    cmd += [args.program, *args.arguments]
    os.execve(runner, cmd, env)

if __name__ == "__main__": main()
