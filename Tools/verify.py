#!/usr/bin/env python3
#
#  verify.py
#  SimpleHDA
#
#  Created by laobamac on 2026/8/17.
#  Copyright © 2026 laobamac. All rights reserved.
#
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--source', action='store_true')
parser.add_argument('--configuration', choices=('Debug', 'Release'))
args = parser.parse_args()

def require(condition, message):
    if not condition:
        raise SystemExit(message)

source_info = plistlib.loads((root / 'Info.plist').read_bytes())
version = source_info['CFBundleVersion']
if args.source:
    manifest = json.loads((root / 'Resources/AppleHDA.json').read_text())
    bundle = root / 'Resources/AppleHDA.kext'
    paths = {p.relative_to(bundle).as_posix(): p for p in bundle.rglob('*') if p.is_file()}
    require(set(paths) == set(manifest['files']), 'AppleHDA payload file list differs')
    for name, expected in manifest['files'].items():
        require(hashlib.sha256(paths[name].read_bytes()).hexdigest() == expected, f'AppleHDA checksum differs: {name}')
    for name in ('AppleHDA.kext', 'AppleHDA.kext/Contents/PlugIns/AppleHDAHALPlugIn.bundle'):
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(root / 'Resources' / name)], check=True)
    print(f'Verified original AppleHDA payload: {len(paths)} files')
if args.configuration:
    bundle = root / 'Build' / args.configuration / 'SimpleHDA.kext'
    info = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
    require(info['CFBundleVersion'] == version == info['CFBundleShortVersionString'], 'Built version differs')
    require(info['OSBundleLibraries'] == source_info['OSBundleLibraries'], 'Built dependency requirements differ')
    require(info['CFBundleIdentifier'] == 'cn.laobamac.SimpleHDA', 'Built identifier differs')
    require(info['IOKitPersonalities']['SimpleHDA']['IOResourceMatch'] == 'IOKit', 'Incorrect startup match')
    require(info['IOKitPersonalities']['SimpleHDA']['IOProviderClass'] == 'IOResources', 'Incorrect startup provider')
    binary = bundle / 'Contents/MacOS/SimpleHDA'
    data = binary.read_bytes()
    require(len(data) >= 32 and struct.unpack_from('<I', data)[0] == 0xfeedfacf, 'Expected thin Mach-O')
    require(struct.unpack_from('<I', data, 4)[0] == 0x1000007, 'Expected x86_64')
    require(struct.unpack_from('<I', data, 12)[0] == 0xb, 'Expected kernel extension')
    require(b'cn.laobamac.SimpleHDA\0' in data and version.encode() + b'\0' in data, 'Missing kmod metadata')
    symbols = subprocess.check_output(['nm', '-g', str(binary)], text=True)
    for name in ('_SimpleHDA_kern_start', '_SimpleHDA_kern_stop', '_kmod_info'):
        require(any(line.split()[-1:] == [name] and ' U ' not in line for line in symbols.splitlines()), f'Missing startup symbol: {name}')
    subprocess.run(['plutil', '-lint', str(bundle / 'Contents/Info.plist')], check=True)
    print(f'Verified {args.configuration}: cn.laobamac.SimpleHDA {version}')
if not args.source and not args.configuration:
    parser.error('Specify --source or --configuration')
