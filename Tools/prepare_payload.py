#!/usr/bin/env python3
#
#  prepare_payload.py
#  SimpleHDA
#
#  Created by laobamac on 2026/8/5.
#  Copyright © 2026 laobamac. All rights reserved.
#
import hashlib
import json
import os
from pathlib import Path
import plistlib
import subprocess

root = Path(__file__).resolve().parents[1]
source = Path(os.environ.get('SIMPLEHDA_SOURCE', str(root / 'Resources/AppleHDA.kext'))).resolve()
if not (source / 'Contents/Info.plist').is_file():
    raise SystemExit('Set SIMPLEHDA_SOURCE to a genuine macOS 15 AppleHDA.kext (full bundle).')
gen = root / 'Generated'
blobdir = gen / 'Payload'
blobdir.mkdir(parents=True, exist_ok=True)
header = '//\n//  Payload.cpp\n//  SimpleHDA\n//\n//  Created by laobamac on 2026/8/6.\n//  Copyright © 2026 laobamac. All rights reserved.\n//\n'
cpp = [header, '#include "SHDAPayload.hpp"']
asm = ['//\n//  Payload.S\n//  SimpleHDA\n//\n//  Created by laobamac on 2026/8/26.\n//  Copyright © 2026 laobamac. All rights reserved.\n//', '.section __DATA_CONST,__const']
manifest = {'source': 'AppleHDA.kext', 'modules': [], 'resources': [], 'localizations': [], 'hal': {
    'status': 'automatic detection at engine initialization; optional data-volume HAL installer',
    'install_path': '/Library/Audio/Plug-Ins/SimpleHDA/AppleHDAHALPlugIn.bundle'}}
index = 0

def blob(data):
    global index
    name = f'shda_blob_{index}'
    p = blobdir / f'{index:03d}.bin'
    p.write_bytes(data)
    asm.extend(['.p2align 3', f'.globl _{name}', f'_{name}:', f'.incbin "{p.relative_to(root).as_posix()}"'])
    cpp.append(f'extern "C" const unsigned char {name}[];')
    index += 1
    return name

def sha(data):
    return hashlib.sha256(data).hexdigest()

entries = []
order = ['IOHDAFamily', 'AppleHDAController', 'DspFuncLib', 'AppleHDAHardwareConfigDriver']
bundles = [source / 'Contents/PlugIns' / (name + '.kext') for name in order] + [source]
for bundle in bundles:
    info = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
    original = bundle / 'Contents/MacOS' / info['CFBundleExecutable']
    raw = original.read_bytes()
    archs = subprocess.check_output(['lipo', '-archs', str(original)], text=True).split()
    if 'x86_64' not in archs:
        raise SystemExit(f'No x86_64 slice: {original}')
    if len(archs) > 1:
        thin = blobdir / 'thin.tmp'
        subprocess.run(['lipo', str(original), '-thin', 'x86_64', '-output', str(thin)], check=True)
        raw = thin.read_bytes()
        thin.unlink()
    if raw[:4] != b'\xcf\xfa\xed\xfe':
        raise SystemExit(f'Unsupported Mach-O: {original}')
    rel = 'AppleHDA.kext' + ('/' + str(bundle.relative_to(source)) if bundle != source else '')
    info['_PrelinkBundlePath'] = '/EFI/OC/Kexts/SimpleHDA.kext/Contents/Resources/Payload/' + rel
    info['_PrelinkExecutableRelativePath'] = 'Contents/MacOS/' + info['CFBundleExecutable']
    plistdata = plistlib.dumps(info, fmt=plistlib.FMT_XML) + b'\0'
    ps, es = blob(plistdata), blob(raw)
    libraries = info.get('OSBundleLibraries_x86_64', info.get('OSBundleLibraries', {}))
    libname = f'shda_libraries_{len(entries)}'
    cpp.append(f'static const char * const {libname}[] = {{' + ','.join(json.dumps(k) for k in libraries) + '};')
    entries.append('{' + ', '.join([json.dumps(info['CFBundleIdentifier']), json.dumps(rel), f'reinterpret_cast<const char *>({ps})', es, str(len(raw)), libname, str(len(libraries))]) + '}')
    manifest['modules'].append({'identifier': info['CFBundleIdentifier'], 'version': info['CFBundleVersion'], 'source': str(original.relative_to(source)), 'x86_64_sha256': sha(raw), 'size': len(raw), 'libraries': info.get('OSBundleLibraries_x86_64', info.get('OSBundleLibraries', {}))})
res = []
for p in sorted((source / 'Contents/Resources').glob('*.xml.zlib')):
    raw = p.read_bytes()
    symbol = blob(raw)
    res.append('{' + f'{json.dumps(p.name)}, {symbol}, {len(raw)}' + '}')
    manifest['resources'].append({'name': p.name, 'size': len(raw), 'sha256': sha(raw)})
if not any(r['name'] == 'Platforms.xml.zlib' for r in manifest['resources']):
    raise SystemExit('Original Platforms.xml.zlib is missing.')
localized = []
for p in sorted((source / 'Contents/Resources').glob('*.lproj/Localizable.strings')):
    strings = json.loads(subprocess.check_output(['plutil', '-convert', 'json', '-o', '-', str(p)]))
    if not all(isinstance(k, str) and isinstance(v, str) and '\0' not in k + v for k, v in strings.items()):
        raise SystemExit(f'Invalid localization table: {p}')
    name = f'shda_strings_{len(localized)}'
    cpp.append(f'static const shda::Translation {name}[] = {{' + ','.join('{' + json.dumps(k, ensure_ascii=False) + ',' + json.dumps(v, ensure_ascii=False) + '}' for k, v in sorted(strings.items())) + '};')
    locale = p.parent.stem
    localized.append('{' + f'{json.dumps(locale)}, {name}, {len(strings)}' + '}')
    manifest['localizations'].append({'locale': locale, 'source': str(p.relative_to(source)), 'sha256': sha(p.read_bytes()), 'entries': len(strings)})
locales = [item['locale'] for item in manifest['localizations']]
if 'en' not in locales or 'zh_CN' not in locales:
    raise SystemExit('Original English and Simplified Chinese localization tables are required.')
default_locale = os.environ.get('SIMPLEHDA_LOCALE', '')
if default_locale and default_locale not in locales:
    raise SystemExit(f'Unsupported SIMPLEHDA_LOCALE: {default_locale}; available: {", ".join(locales)}')
manifest['default_locale'] = default_locale
cpp.extend(['namespace shda {', 'const Module modules[] = {\n' + ',\n'.join(entries) + '\n};', 'const size_t moduleCount = sizeof(modules) / sizeof(modules[0]);', 'const Resource resources[] = {\n' + ',\n'.join(res) + '\n};', 'const size_t resourceCount = sizeof(resources) / sizeof(resources[0]);', 'const Localization localizations[] = {\n' + ',\n'.join(localized) + '\n};', 'const size_t localizationCount = sizeof(localizations) / sizeof(localizations[0]);', f'const char defaultLocale[] = {json.dumps(default_locale)};', '}'])
(gen / 'Payload.cpp').write_text('\n'.join(cpp) + '\n')
(gen / 'Payload.S').write_text('\n'.join(asm) + '\n')
(gen / 'PayloadManifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
print(f'Embedded {len(entries)} original modules, {len(res)} resources and {len(locales)} localizations; default={default_locale}')
