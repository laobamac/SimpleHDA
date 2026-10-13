#!/usr/bin/env python3
#
#  package.py
#  SimpleHDA
#
#  Created by laobamac on 2026/8/27.
#  Copyright © 2026 laobamac. All rights reserved.
#
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--channel', choices=('pre', 'rel'), default='rel')
args = parser.parse_args()
version = plistlib.loads((root / 'Info.plist').read_bytes())['CFBundleVersion']
dist = root / 'Dist'
dist.mkdir(exist_ok=True)
for configuration in ('Release', 'Debug'):
    subprocess.run(['python3', str(root / 'Tools/verify.py'), '--configuration', configuration], check=True)
suffix = ''
if args.channel == 'pre':
    sha = os.environ.get('GITHUB_SHA') or subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    if not re.fullmatch(r'[0-9a-fA-F]{40}', sha):
        raise SystemExit('Invalid commit SHA')
    suffix = f'-alpha-{sha[:7]}'
package = root / 'Build' / f'SimpleHDA-HAL-{version}.pkg'
if not package.is_file():
    raise SystemExit('HAL installer is missing; run zsh Tools/build.zsh')
entry = {'Arch': 'x86_64', 'BundlePath': 'SimpleHDA.kext', 'Comment': f'SimpleHDA {version}',
         'Enabled': True, 'ExecutablePath': 'Contents/MacOS/SimpleHDA',
         'MaxKernel': '25.99.99', 'MinKernel': '25.0.0', 'PlistPath': 'Contents/Info.plist'}
for configuration in ('Debug', 'Release'):
    name = f'SimpleHDA-Tahoe-v{version}-{configuration.upper()}{suffix}'
    stage = dist / name
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir()
    shutil.copytree(root / 'Build' / configuration / 'SimpleHDA.kext', stage / 'SimpleHDA.kext')
    shutil.copy2(package, stage / package.name)
    for filename in ('README.md', 'README.en.md', 'NOTICE.md'):
        shutil.copy2(root / filename, stage / filename)
    tools = stage / 'Tools'
    tools.mkdir()
    shutil.copy2(root / 'Tools/uninstall_hal.zsh', tools / 'uninstall_hal.zsh')
    (stage / 'OpenCore-Kernel-Add.plist').write_bytes(plistlib.dumps({'Kernel': {'Add': [entry]}}))
    licenses = stage / 'Licenses'
    licenses.mkdir()
    for dependency in ('Lilu', 'xnu'):
        for filename in ('LICENSE.txt', 'LICENSE', 'APPLE_LICENSE'):
            source = root / 'Vendor' / dependency / filename
            if source.is_file():
                shutil.copy2(source, licenses / f'{dependency}-{filename}')
    files = sorted(p for p in stage.rglob('*') if p.is_file())
    hashes = {p.relative_to(stage).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    (stage / 'SHA256.json').write_text(json.dumps(hashes, ensure_ascii=False, indent=2) + '\n')
    archive = dist / f'{name}.zip'
    subprocess.run(['ditto', '-c', '-k', '--keepParent', '--norsrc', '--noextattr', str(stage), str(archive)], check=True)
    checksum = dist / f'{name}.sha256'
    checksum.write_text(f'{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n')
    print(archive)
