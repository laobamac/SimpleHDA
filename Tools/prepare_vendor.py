#!/usr/bin/env python3
#
#  prepare_vendor.py
#  SimpleHDA
#
#  Created by laobamac on 2026/8/1.
#  Copyright © 2026 laobamac. All rights reserved.
#
from pathlib import Path
import shutil
import re
import subprocess

root = Path(__file__).resolve().parents[1]
src = root / 'Vendor/xnu/libkern'
dst = root / 'Generated/kxld'
dst.mkdir(parents=True, exist_ok=True)
for p in (src / 'kxld').glob('*'):
    if p.suffix not in ('.c', '.h') or p.name.startswith('WKdm'):
        continue
    text = p.read_text()
    text = text.replace('#include <libkern/kernel_mach_header.h>', '#include <mach-o/loader.h>\n#include <mach/kmod.h>')
    text = text.replace('#include "../../osfmk/mach/machine.h"', '#include <mach/machine.h>')
    if p.name == 'kxld_copyright.c':
        text = text.replace('strnstr(str,', 'strnstr((char *)(uintptr_t)str,')
    if p.name == 'kxld_sym.c':
        text = text.replace('strnstr(name,', 'strnstr((char *)(uintptr_t)name,')
    if p.name == 'kxld_seg.c':
        text = re.sub(r'(kxld_addr_t\s+my_link_addr)\s*;', r'\1 = 0;', text)
    if p.name == 'kxld_reloc.c':
        for typ in ('32', '64'):
            text = text.replace(f'instr{typ} = *instr{typ}p;', f'memcpy(&instr{typ}, instruction, sizeof(instr{typ}));')
            text = text.replace(f'*instr{typ}p = instr{typ};', f'memcpy(instruction, &instr{typ}, sizeof(instr{typ}));')
    if p.name == 'kxld_kext.c':
        text = text.replace('#include <libkern/OSKextLibPrivate.h>', 'extern void shda_weak_reference(void);')
        text = text.replace('&kext_weak_symbol_referenced', '&shda_weak_reference')
    if p.name == 'kxld_util.c':
        text = text.replace('#include <kern/kalloc.h>', '#include "SHDAAlloc.h"')
        text = text.replace('#include <vm/vm_kern.h>', '')
        text = text.replace('kheap_alloc_tag(KHEAP_DATA_PRIVATE, size, Z_WAITOK | Z_ZERO,\n\t    VM_KERN_MEMORY_OSKEXT)', 'shda_alloc(size)')
        text = text.replace('kheap_free(KHEAP_DEFAULT, ptr, size)', 'shda_free(ptr, size)')
        text = text.replace('kheap_free(KHEAP_DEFAULT, ptr, round_page(size))', 'shda_free(ptr, round_page(size))')
    (dst / p.name).write_text(text)
for name in ('kxld.h', 'kxld_types.h', 'prelink.h'):
    shutil.copyfile(src / 'libkern' / name, dst / name)
    target = root / 'Generated/Headers/libkern' / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src / 'libkern' / name, target)

sdk = Path(subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip())
for name in ('stab.h', 'x86_64/reloc.h'):
    target = root / 'Generated/Headers/mach-o' / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(sdk / 'usr/include/mach-o' / name, target)
