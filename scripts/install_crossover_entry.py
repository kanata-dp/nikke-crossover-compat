#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Persist a stopped, working test bottle and register a CrossOver menu entry.

Copies local files only. Does not download or redistribute CrossOver or games.
The source runtime and bootstrap must already have been tested together.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shlex
import shutil
import subprocess
import uuid


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def copy_tree(source, target):
    # APFS clone files; fail instead of silently filling the disk with a full copy.
    subprocess.run(['/bin/cp', '-cR', str(source), str(target)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-prefix', required=True, type=Path)
    parser.add_argument('--source-runtime', required=True, type=Path)
    parser.add_argument('--source-app', required=True, type=Path)
    parser.add_argument('--source-bridge', required=True, type=Path)
    parser.add_argument('--bottle-name', default='NIKKE-Compatibility')
    parser.add_argument('--menu-name', default='NIKKE Compatibility')
    parser.add_argument('--support', type=Path, default=Path.home() /
                        'Library/Application Support/NIKKE Compatibility')
    parser.add_argument('--crossover', type=Path, default=Path(
        '/Applications/CrossOver.app/Contents/SharedSupport/CrossOver'))
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]+', args.bottle_name):
        parser.error('bottle-name must contain ASCII letters, digits, _ or -')
    if not args.menu_name or any(c in args.menu_name for c in '/\n\r'):
        parser.error('menu-name must be one menu component')
    source = args.source_prefix.resolve()
    source_runtime = args.source_runtime.resolve()
    source_app = args.source_app.resolve()
    source_bridge = args.source_bridge.resolve()
    support = args.support.expanduser().resolve()
    prefix = Path.home() / 'Library/Application Support/CrossOver/Bottles' / args.bottle_name
    cx = args.crossover.resolve()
    for p in (source / 'system.reg', source / 'cxbottle.conf',
              source_runtime / 'lib/wine/x86_64-unix/ntdll.so',
              source_app / 'Contents/MacOS/wine_bootstrap', source_bridge, cx / 'bin/cxmenu'):
        if not p.is_file(): parser.error(f'required file missing: {p}')
    if prefix.exists() or support.exists():
        parser.error('destination bottle/support already exists; choose new names')
    env = os.environ.copy()
    env['WINEPREFIX'] = str(source)
    try:
        subprocess.run([str(source_runtime / 'bin/wineserver'), '-w'],
                       env=env, check=True, timeout=3)
    except subprocess.TimeoutExpired:
        parser.error('source bottle is running; close it before copying')

    source_info = plistlib.loads((source_app / 'Contents/Info.plist').read_bytes())
    source_env = {
        'NOP_BRIDGE_APP_PROGRAM': r'C:\NIKKE\Launcher\nikke_launcher.exe',
        'NOP_BRIDGE_APP_CWD': str(source / 'drive_c/NIKKE/Launcher'),
        'NOP_BRIDGE_NTDLL': str(source_runtime / 'lib/wine/x86_64-unix/ntdll.so'),
        'WINELOADER': str(source_app / 'Contents/MacOS/wine_bootstrap'),
        'CX_WINELOADER': str(source_app / 'Contents/MacOS/wine_bootstrap'),
        'WINESERVER': str(source_runtime / 'bin/wineserver'),
        'DYLD_INSERT_LIBRARIES': str(source_bridge),
        'WINEDLLPATH': ':'.join(str(source_runtime / p) for p in (
            'lib/wine/x86_64-windows', 'lib/wine/i386-windows', 'lib/wine')),
        'CX_GRAPHICS_BACKEND': 'dxvk', 'WINEDLLOVERRIDES': 'version=n,b',
        'NOP_BRIDGE_PRIVILEGED': '1', 'NOP_BRIDGE_MF_NO_DXGI': '1',
        'WINEDEBUG': '-all,err+all,warn+ntoskrnl,fixme+ntoskrnl,warn+mfplat',
    }
    source_env.update(source_info.get('LSEnvironment', {}))
    if not Path(source_env['NOP_BRIDGE_APP_CWD']).is_dir():
        parser.error('the official NIKKE launcher directory is missing')
    support.mkdir(parents=True, mode=0o700)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    app = support / 'NIKKE Compatibility.app'
    runtime = support / 'runtime'
    bridge = support / 'libnop_bridge.dylib'
    (support / 'logs').mkdir(mode=0o700)
    copy_tree(source, prefix)
    os.chmod(prefix, 0o700)
    copy_tree(source_runtime, runtime)
    copy_tree(source_app, app)
    shutil.copy2(source_bridge, bridge)
    loader = app / 'Contents/MacOS/wine_bootstrap'
    child_loader = runtime / 'lib/wine/x86_64-unix/wine'
    child_loader.unlink()
    child_loader.symlink_to(loader)
    mappings = sorted(((str(source), str(prefix)), (str(source_runtime), str(runtime)),
                       (str(source_app), str(app)), (str(source_bridge), str(bridge))),
                      key=lambda item: -len(item[0]))
    def relocate(value):
        for old, new in mappings: value = value.replace(old, new)
        return value
    new_env = {key: relocate(value) for key, value in source_env.items()}
    new_env.update(CX_BOTTLE=args.bottle_name, CX_BOTTLE_PATH=str(prefix.parent),
                   WINEPREFIX=str(prefix), CX_ROOT=str(cx), WINEARCH='wow64',
                   NOP_BRIDGE_LOG=str(support / 'logs/launcher.log'))
    source_info.update(CFBundleIdentifier='org.nopbridge.nikke.compatibility',
                       CFBundleName='NIKKE Compatibility', LSEnvironment=new_env)
    (app / 'Contents/Info.plist').write_bytes(plistlib.dumps(source_info))
    subprocess.run(['/usr/bin/codesign', '--force', '--sign', '-', str(app)], check=True)

    # The copied menu data still names the source bottle: rebuild it solely in
    # this new copy, with a new ID, so the original bottle's icons are untouched.
    shutil.rmtree(prefix / 'desktopdata', ignore_errors=True)
    (prefix / 'cxmenu.conf').write_text('')
    config = (prefix / 'cxbottle.conf').read_text()
    config = re.sub(r'^"BottleID"\s*=.*$', f'"BottleID" = "{str(uuid.uuid4()).upper()}"',
                    config, flags=re.M)
    config = re.sub(r'^"Description"\s*=.*$', '"Description" = "NIKKE compatibility runtime"',
                    config, flags=re.M)
    (prefix / 'cxbottle.conf').write_text(config)
    menu_env = os.environ.copy()
    for key in list(menu_env):
        if key.startswith(('WINE', 'CX_', 'NOP_BRIDGE_', 'DYLD_')): menu_env.pop(key)
    command = shlex.join(['/usr/bin/open', str(app)])
    icon = prefix / 'windata/cxmenu/icons/hicolor/256x256/apps/A7E5_nikke_launcher.0.png'
    cmd = [str(cx / 'bin/cxmenu'), '--bottle', args.bottle_name, '--create',
           'StartMenu/' + args.menu_name, '--type', 'raw', '--command', command,
           '--description', 'Launch the locally tested NIKKE compatibility runtime', '--install']
    if icon.is_file(): cmd += ['--icon', str(icon)]
    subprocess.run(cmd, env=menu_env, check=True)
    modules = ['ntoskrnl.exe', 'mfplat.dll', 'mfreadwrite.dll']
    hashes = {name: digest(runtime / 'lib/wine/x86_64-windows' / name) for name in modules}
    hashes.update(bootstrap=digest(loader), bridge=digest(bridge))
    manifest = dict(bottle=str(prefix), app=str(app), runtime=str(runtime), sha256=hashes,
                    menu='StartMenu/' + args.menu_name,
                    source_prefix=str(source), high_resolution='enable with CrossOver after installation')
    (support / 'installation.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
