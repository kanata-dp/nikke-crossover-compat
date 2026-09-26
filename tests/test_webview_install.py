# SPDX-License-Identifier: LGPL-2.1-or-later
"""Backup/rollback and refusal tests using authored files, never a real bottle."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("webview", ROOT / "scripts/install_webview_fix.py")
webview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(webview)


def fixture(payload):
    header = bytearray(64)
    header[:2] = b"MZ"
    header[60:64] = (64).to_bytes(4, "little")
    return bytes(header) + b"PE\0\0\x64\x86" + payload


class WebViewInstall(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.service = self.root / "service"
        self.service.mkdir()
        self.target = self.service / webview.SERVICE
        self.backup = self.service / webview.BACKUP
        self.manifest = self.service / webview.MANIFEST
        self.original = fixture(b"authored service fixture")
        self.target.write_bytes(self.original)
        self.wrapper = self.root / "wrapper.exe"
        self.wrapper.write_bytes(fixture(webview.MARKER))

    def install(self):
        return webview.operate(self.service, "install", self.wrapper)

    def test_install_repeat_restore_repeat_and_reinstall(self):
        self.install()
        self.assertEqual(self.backup.read_bytes(), self.original)
        self.assertEqual(self.target.read_bytes(), self.wrapper.read_bytes())
        self.assertIn("Already", self.install())
        self.assertEqual(webview.operate(self.service, "check"), "Installed.")
        for _ in range(2):
            webview.operate(self.service, "restore")
            self.assertEqual(self.target.read_bytes(), self.original)
        self.install()
        self.assertEqual(self.target.read_bytes(), self.wrapper.read_bytes())

    def test_official_update_is_never_overwritten(self):
        self.install()
        updated = fixture(b"new official service fixture")
        self.target.write_bytes(updated)
        for action in ("install", "restore", "check"):
            with self.subTest(action=action), self.assertRaises(ValueError):
                webview.operate(self.service, action, self.wrapper)
            self.assertEqual(self.target.read_bytes(), updated)
            self.assertEqual(self.backup.read_bytes(), self.original)

    def test_tampered_backup_is_preserved_and_refused(self):
        self.install()
        self.backup.write_bytes(b"changed")
        with self.assertRaises(ValueError):
            webview.operate(self.service, "restore")
        self.assertEqual(self.target.read_bytes(), self.wrapper.read_bytes())

    def test_unmanaged_backup_is_preserved(self):
        self.backup.write_bytes(b"existing manual backup")
        with self.assertRaises(ValueError):
            self.install()
        self.assertEqual(self.target.read_bytes(), self.original)
        self.assertEqual(self.backup.read_bytes(), b"existing manual backup")

    def test_manual_wrapper_without_marker_is_refused(self):
        data = fixture(webview.ORIGINAL_REF)
        self.target.write_bytes(data)
        with self.assertRaises(ValueError):
            self.install()
        self.assertEqual(self.target.read_bytes(), data)

    def test_invalid_wrapper_does_not_create_backup(self):
        self.wrapper.write_bytes(b"not a PE executable")
        with self.assertRaises(ValueError):
            self.install()
        self.assertFalse(self.backup.exists())
        self.assertFalse(self.manifest.exists())

    def test_symlinks_do_not_change_external_files(self):
        outside = self.root / "outside"
        outside.write_bytes(self.original)
        for name in (webview.SERVICE, webview.BACKUP, webview.MANIFEST):
            with self.subTest(name=name):
                path = self.service / name
                if path.exists(): path.unlink()
                path.symlink_to(outside)
                with self.assertRaises(ValueError): self.install()
                self.assertEqual(outside.read_bytes(), self.original)
                path.unlink()
                if name == webview.SERVICE: path.write_bytes(self.original)

    def test_interrupted_target_replace_can_resume_or_restore(self):
        real_write = webview.atomic_write
        def fail_target(path, data, mode=0o600):
            if path == self.target: raise OSError("simulated replace failure")
            real_write(path, data, mode)
        with patch.object(webview, "atomic_write", side_effect=fail_target):
            with self.assertRaises(OSError): self.install()
        self.assertEqual(self.target.read_bytes(), self.original)
        self.assertEqual(self.backup.read_bytes(), self.original)
        self.install()
        webview.operate(self.service, "restore")
        self.assertEqual(self.target.read_bytes(), self.original)

    def test_different_wrapper_requires_restore(self):
        self.install()
        self.wrapper.write_bytes(fixture(webview.MARKER + b"another build"))
        with self.assertRaises(ValueError): self.install()
        webview.operate(self.service, "restore")
        self.install()
        self.assertEqual(self.backup.read_bytes(), self.original)

    def test_readonly_check_does_not_write(self):
        self.assertIn("Not installed", webview.operate(self.service, "check"))
        self.assertEqual(list(self.service.iterdir()), [self.target])

    def test_service_directory_rejects_symlink_and_escape(self):
        (self.root / "system.reg").write_text("fixture")
        (self.root / "link").symlink_to(self.service, target_is_directory=True)
        for path in ("link", "../outside", str(self.service)):
            with self.subTest(path=path), self.assertRaises(ValueError):
                webview.service_directory(self.root, path)

    def test_running_service_and_process_query_failure_block(self):
        for line in ("wine intl_service.exe --private-session", "wine NIKKE.exe"):
            result = subprocess.CompletedProcess([], 0, line)
            with patch.object(webview.subprocess, "run", return_value=result):
                with self.assertRaises(ValueError): webview.ensure_stopped(self.root)
        with patch.object(webview.subprocess, "run", side_effect=PermissionError):
            with self.assertRaises(PermissionError): webview.ensure_stopped(self.root)


if __name__ == "__main__":
    unittest.main()
