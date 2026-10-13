#!/usr/bin/env python3
#
#  setup.py
#  SimpleHDA
#
#  Created by laobamac on 2026/8/14.
#  Copyright © 2026 laobamac. All rights reserved.
#
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
versions = json.loads((root / 'VendorVersions.json').read_text())
patterns = {
    'Lilu': ['/Lilu/Headers/', '/Lilu/Library/plugin_start.cpp', '/capstone/include/',
             '/hde/hde32.h', '/hde/hde64.h', '/LICENSE.txt'],
    'MacKernelSDK': ['/Headers/', '/Library/universal/', '/Library/x86_64/', '/LICENSE'],
    'xnu': ['/libkern/kxld/', '/libkern/libkern/kxld.h', '/libkern/libkern/kxld_types.h',
            '/libkern/libkern/prelink.h', '/APPLE_LICENSE'],
}
for name, spec in versions.items():
    target = root / 'Vendor' / name
    if not (target / '.git').exists():
        target.mkdir(parents=True, exist_ok=True)
        if any(not path.is_dir() or path.is_symlink() for path in target.rglob('*')):
            raise SystemExit(f'Dependency directory is not a Git checkout: {target}')
        subprocess.run(['git', 'init', '-q', str(target)], check=True)
        subprocess.run(['git', '-C', str(target), 'remote', 'add', 'origin', spec['url']], check=True)
        subprocess.run(['git', '-C', str(target), 'sparse-checkout', 'set', '--no-cone', *patterns[name]], check=True)
        subprocess.run(['git', '-C', str(target), 'fetch', '-q', '--depth=1', '--filter=blob:none', 'origin', spec['commit']], check=True)
        subprocess.run(['git', '-C', str(target), 'checkout', '-q', '--detach', 'FETCH_HEAD'], check=True)
    actual = subprocess.check_output(['git', '-C', str(target), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != spec['commit']:
        raise SystemExit(f'{name} revision differs from VendorVersions.json: {actual}')
    changed = subprocess.check_output(['git', '-C', str(target), 'status', '--porcelain', '--untracked-files=no'], text=True)
    if changed.strip():
        raise SystemExit(f'{name} has modified dependency files')
    print(f'{name}: {actual}')
