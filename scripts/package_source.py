#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Package an explicit source allowlist; never walk a bottle or runtime."""
import hashlib
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
FILES = [
    "README.md", "README.en.md", "LICENSE", "THIRD_PARTY.md", "Makefile", ".gitignore",
    "src/bridge.c", "src/nop_decode.h", "src/wine_bootstrap.c", "src/Info.plist",
    "src/priv_decode.h",
    "tests/native_probe.c", "tests/decoder_test.c", "tests/windows_probe.c",
    "tests/windows_early.c", "tests/shared_texture.cpp",
    "tests/guarded_mutex.c", "tests/process_image_name.c", "tests/bugcheck_registration.c",
    "tests/privileged_probe.c", "tests/video_reader.cpp", "tests/physical_ranges.c", "tests/physical_mapping.c", "tests/thread_process.c",
    "scripts/run_wine.py", "scripts/prepare_runtime.py", "scripts/launch_crossover.py",
    "scripts/test_native.py", "scripts/test_windows.py", "scripts/package_source.py",
    "scripts/build_wine_modules.py",
    "scripts/test_wine_modules.py",
    "scripts/install_crossover_entry.py",
    "patches/crossover-26.1-kernel.patch", "patches/crossover-26.1-mf-software.patch",
    "patches/crossover-26.1-thread-process-experimental.patch",
    "docs/VALIDATION.md", "docs/ARCHITECTURE.md",
]

def main():
    output = ROOT / "build/nikke-crossover-compat-0.1.0-source.zip"
    output.parent.mkdir(exist_ok=True)
    for name in FILES:
        path = ROOT / name
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"missing or non-regular source file: {name}")
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name in FILES:
            info = zipfile.ZipInfo(f"nikke-crossover-compat/{name}", (2026, 9, 8, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, (ROOT / name).read_bytes())
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None and len(archive.namelist()) == len(FILES)
    checksum = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix(output.suffix + ".sha256").write_text(f"{checksum}  {output.name}\n")
    print(f"{len(FILES)} source files: {output.name} ({output.stat().st_size} bytes)")
    print(f"sha256 {checksum}")

if __name__ == "__main__": main()
