#!/bin/zsh
#
#  uninstall_hal.zsh
#  SimpleHDA
#
#  Created by laobamac on 2026/8/12.
#  Copyright © 2026 laobamac. All rights reserved.
#
set -euo pipefail
if (( EUID != 0 )); then
    print -u2 -r -- 'Run: sudo zsh Tools/uninstall_hal.zsh'
    exit 1
fi
SIMPLEHDA_HAL_DIR=/Library/Audio/Plug-Ins/SimpleHDA
if [[ -L "$SIMPLEHDA_HAL_DIR" ]]; then
    print -u2 -r -- 'Refusing to remove a symbolic link.'
    exit 1
fi
rm -rf "$SIMPLEHDA_HAL_DIR/AppleHDAHALPlugIn.bundle"
if [[ -d "$SIMPLEHDA_HAL_DIR" ]]; then
    rmdir "$SIMPLEHDA_HAL_DIR"
fi
if pkgutil --pkg-info cn.laobamac.SimpleHDA.HAL >/dev/null 2>&1; then
    pkgutil --forget cn.laobamac.SimpleHDA.HAL
fi
print -r -- 'SimpleHDA HAL removed. Reboot to use the ordinary IOAudio path; no boot argument is required.'
