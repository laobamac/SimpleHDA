#!/usr/bin/env python3
#
#  prepare_hal.py
#  SimpleHDA
#
#  Created by laobamac on 2026/8/21.
#  Copyright © 2026 laobamac. All rights reserved.
#
from pathlib import Path
import hashlib
import json
import os
import plistlib
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
version = plistlib.loads((root / 'Info.plist').read_bytes())['CFBundleVersion']
source = Path(os.environ.get('SIMPLEHDA_SOURCE', str(root / 'Resources/AppleHDA.kext'))) / 'Contents/PlugIns/AppleHDAHALPlugIn.bundle'
info = plistlib.loads((source / 'Contents/Info.plist').read_bytes())
if info['CFBundleIdentifier'] != 'com.apple.audio.AppleHDAHALPlugIn':
    raise SystemExit('Unexpected HAL identifier')
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(source)], check=True)
executable = source / 'Contents/MacOS' / info['CFBundleExecutable']
if 'x86_64' not in subprocess.check_output(['lipo', '-archs', str(executable)], text=True).split():
    raise SystemExit('HAL has no x86_64 slice')
stage = root / 'Build/HALPackage'
if stage.exists():
    shutil.rmtree(stage)
package_root = stage / 'Root'
install_path = Path('Library/Audio/Plug-Ins/SimpleHDA/AppleHDAHALPlugIn.bundle')
target = package_root / install_path
shutil.copytree(source, target, symlinks=True, ignore=shutil.ignore_patterns('.DS_Store'))
for path in package_root.rglob('*'):
    if not path.is_symlink():
        path.chmod(0o755 if path.is_dir() or path == target / 'Contents/MacOS' / info['CFBundleExecutable'] else 0o644)
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(target)], check=True)
component = stage / 'Components.plist'
subprocess.run(['pkgbuild', '--analyze', '--root', str(package_root), str(component)], check=True, stdout=subprocess.DEVNULL)
entries = plistlib.loads(component.read_bytes())
if len(entries) != 1 or entries[0]['RootRelativeBundlePath'] != str(install_path):
    raise SystemExit('Unexpected installer components')
entries[0].update(BundleIsRelocatable=False, BundleIsVersionChecked=False, BundleHasStrictIdentifier=True, BundleOverwriteAction='upgrade')
component.write_bytes(plistlib.dumps(entries))
package = root / 'Build' / f'SimpleHDA-HAL-{version}.pkg'
subprocess.run(['pkgbuild', '--root', str(package_root), '--component-plist', str(component), '--install-location', '/',
    '--identifier', 'cn.laobamac.SimpleHDA.HAL', '--version', version, '--ownership', 'recommended', str(package)], check=True)
files = {str(p.relative_to(target)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(target.rglob('*')) if p.is_file()}
manifest = {'install_path': '/' + str(install_path), 'identifier': info['CFBundleIdentifier'], 'version': info['CFBundleVersion'],
    'package_sha256': hashlib.sha256(package.read_bytes()).hexdigest(), 'files': files, 'relocatable': False, 'system_volume_modified': False}
(root / 'Build/hal-package.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(package)
