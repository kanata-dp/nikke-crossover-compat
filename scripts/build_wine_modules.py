#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Build experimental Wine modules from pinned official LGPL sources."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE_URL = "https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.1.0.tar.gz"
SOURCE_SHA256 = "e4ec87d5821a009dd1f1d2e36ffe2e24b8fcbae9516375ea42f95a16928ab8fa"
PATCHES = ("crossover-26.1-kernel.patch", "crossover-26.1-mf-software.patch")
MODULES = {"ntoskrnl.exe": "ntoskrnl.exe", "mfreadwrite.dll": "mfreadwrite", "mfplat.dll": "mfplat"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True, type=Path, help=SOURCE_URL)
    parser.add_argument("--output", type=Path, default=ROOT / "local/wine-modules")
    parser.add_argument("--bison", default="/opt/homebrew/opt/bison/bin/bison")
    parser.add_argument("--with-thread-process", action="store_true",
                        help="include the separately tested PsGetThreadProcess candidate (not gameplay verified)")
    args = parser.parse_args()
    archive = args.archive.resolve()
    digest = hashlib.sha256()
    with archive.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""): digest.update(chunk)
    if digest.hexdigest() != SOURCE_SHA256:
        parser.error("archive does not match the pinned CrossOver 26.1 source hash")
    target = args.output.resolve()
    if target.exists(): parser.error("output already exists; choose a new --output")
    target.mkdir(parents=True)
    source = target / "source"
    source.mkdir()
    with tarfile.open(archive) as bundle:
        for item in bundle:
            if not item.name.startswith("sources/wine/"): continue
            relative = Path(item.name[len("sources/wine/"):])
            if relative.is_absolute() or ".." in relative.parts:
                raise ValueError("unsafe archive member")
            path = source / relative
            if item.isdir(): path.mkdir(parents=True, exist_ok=True)
            elif item.isfile():
                path.parent.mkdir(parents=True, exist_ok=True)
                with bundle.extractfile(item) as src, path.open("wb") as dest:
                    shutil.copyfileobj(src, dest)
                path.chmod(item.mode & 0o777)
            else: raise ValueError(f"unexpected archive member type: {item.name}")
    patches = PATCHES + (("crossover-26.1-thread-process-experimental.patch",)
                         if args.with_thread_process else ())
    for patch in patches:
        subprocess.run(["patch", "-p1", "--batch", "--forward", "-i", str(ROOT / "patches" / patch)],
                       cwd=source, check=True)
    build = target / "build"
    build.mkdir()
    env = os.environ.copy()
    env["BISON"] = args.bison
    env.setdefault("CFLAGS", "-O2")
    env.setdefault("OBJCFLAGS", "-O2")
    with (target / "configure.log").open("w") as log:
        subprocess.run([str(source / "configure"), "--enable-archs=x86_64",
                        "--without-x", "--without-freetype", "--disable-tests"],
                       cwd=build, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    targets = [f"dlls/{directory}/x86_64-windows/{name}" for name, directory in MODULES.items()]
    with (target / "build.log").open("w") as log:
        subprocess.run(["make", "-j8", *targets], cwd=build, env=env,
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    for name, directory in MODULES.items():
        result = build / "dlls" / directory / "x86_64-windows" / name
        print(f"{name}: {hashlib.sha256(result.read_bytes()).hexdigest()}")
    print(f"Build complete: {build}. No runtime or bottle was modified.")


if __name__ == "__main__": main()
