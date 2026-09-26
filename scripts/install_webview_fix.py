#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Install/restore the INTL WebView rendering wrapper in a stopped bottle.

Only the service executable, its backup and our hash manifest are touched.
Unknown backups, symlinks and files changed by an updater are never overwritten.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

MARKER = b"NIKKE_INTL_WEBVIEW_COMPAT_V1"
ORIGINAL_REF = "intl_service.original.exe".encode("utf-16le")
SERVICE = "intl_service.exe"
BACKUP = "intl_service.original.exe"
MANIFEST = ".nikke-webview-fix.json"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def regular(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"Expected a regular, non-symlink file: {path.name}")
    return path.read_bytes()


def pe64(data):
    if len(data) < 64 or data[:2] != b"MZ":
        return False
    offset = int.from_bytes(data[60:64], "little")
    return data[offset:offset + 6] == b"PE\0\0\x64\x86"


def atomic_write(path, data, mode=0o600):
    fd, temporary = tempfile.mkstemp(prefix=".webview-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
            os.fchmod(stream.fileno(), mode)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def ensure_stopped(prefix):
    # Read process arguments only for matching; do not print or save them.
    listing = subprocess.run(["ps", "-axo", "command="], check=True,
                             text=True, capture_output=True).stdout.lower()
    candidates = ("intl_service.exe", "intl_service.original.exe",
                  "intlwebviewhelper.exe", "tbs_browser.exe", "nikke.exe")
    for line in listing.splitlines():
        if "install_webview_fix.py" in line:
            continue
        if any(name in line for name in candidates) or (
            str(prefix).lower() in line and any(name in line for name in ("wine", "launcher"))
        ):
            raise ValueError("Close NIKKE, its launcher and bottle processes in CrossOver first.")


def service_directory(prefix, relative):
    if not (prefix / "system.reg").is_file():
        raise ValueError("The selected directory is not a Wine/CrossOver bottle.")
    part = Path(relative)
    if part.is_absolute() or ".." in part.parts:
        raise ValueError("--service-dir must be a relative directory inside the bottle.")
    result = prefix
    for name in part.parts:
        result = result / name
        if result.is_symlink():
            raise ValueError("Service directory contains a symlink; refusing external writes.")
    if not result.is_dir():
        raise ValueError("INTL service directory not found; check --prefix / --service-dir.")
    return result


def operate(directory, action, wrapper=None):
    target, backup, manifest = (directory / name for name in (SERVICE, BACKUP, MANIFEST))
    current = regular(target)
    for path in (backup, manifest):
        if path.is_symlink() or (path.exists() and not path.is_file()):
            raise ValueError(f"Refusing unsafe backup/manifest: {path.name}")
    state = json.loads(regular(manifest)) if manifest.exists() else None
    if state is not None and not isinstance(state, dict):
        raise ValueError("Invalid manifest; keep all files for manual review.")
    if state:
        if state.get("schema") != 1:
            raise ValueError("Unknown manifest format; keep all files for manual review.")
        original = regular(backup)
        if digest(original) != state.get("original_sha256"):
            raise ValueError("Original backup changed; refusing to overwrite anything.")
        if digest(current) not in (state.get("wrapper_sha256"), state.get("original_sha256")):
            raise ValueError("Service changed (possibly an update). Do not restore an old backup; see update notes.")
    else:
        if manifest.exists() or backup.exists() or MARKER in current or ORIGINAL_REF in current:
            raise ValueError("Unmanaged backup/wrapper found; keep it and see the migration notes.")
        if not pe64(current):
            raise ValueError("Expected an x86-64 Windows INTL service executable.")
        original = current
    if action == "check":
        if not state:
            return "Not installed. No files changed."
        return "Installed." if digest(current) == state["wrapper_sha256"] else "Install interrupted or restored; rerun install or restore."
    if action == "restore":
        if not state:
            return "Not installed. No files changed."
        atomic_write(target, original, target.stat().st_mode & 0o777)
        # Keep the backup: it also permits recovery after an interrupted cleanup.
        return "Original service restored. Backup and manifest kept for recovery/reinstallation."
    replacement = regular(wrapper)
    if not pe64(replacement) or MARKER not in replacement:
        raise ValueError("Use the wrapper built by 'make webview' in this repository.")
    if state and digest(current) == state["wrapper_sha256"]:
        if digest(replacement) == state["wrapper_sha256"]:
            return "Already installed. No files changed."
        raise ValueError("Restore the previous wrapper before installing a different build.")
    state = {"schema": 1, "original_sha256": digest(original),
             "wrapper_sha256": digest(replacement)}
    if not backup.exists():
        # Exclusive creation: never overwrite a pre-existing official backup.
        with backup.open("xb") as stream:
            stream.write(original)
            stream.flush()
            os.fsync(stream.fileno())
    atomic_write(manifest, (json.dumps(state, indent=2) + "\n").encode())
    atomic_write(target, replacement, target.stat().st_mode & 0o777)
    return "Installed. Restart your usual CrossOver NIKKE entry; complete the CAPTCHA yourself."


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("install", "check", "restore"))
    parser.add_argument("--prefix", type=Path, required=True)
    parser.add_argument("--service-dir", default="drive_c/NIKKE/Launcher/intl_service")
    parser.add_argument("--wrapper", type=Path, default=Path(__file__).resolve().parents[1] / "build/intl_service.wrapper.exe")
    args = parser.parse_args()
    try:
        prefix = args.prefix.expanduser().resolve(strict=True)
        directory = service_directory(prefix, args.service_dir)
        if args.action != "check":
            ensure_stopped(prefix)
        print(operate(directory, args.action, args.wrapper))
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, f"Stopped without guessing: {error}\n")


if __name__ == "__main__":
    main()
