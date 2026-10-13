#!/bin/zsh
#
#  build.zsh
#  SimpleHDA
#
#  Created by laobamac on 2026/8/9.
#  Copyright © 2026 laobamac. All rights reserved.
#
set -euo pipefail
cd "${0:A:h:h}"
export PYTHONDONTWRITEBYTECODE=1
mkdir -p Build
python3 Tools/setup.py
python3 Tools/verify.py --source
python3 Tools/prepare_vendor.py
python3 Tools/prepare_payload.py
python3 Tools/generate_project.py
SIMPLEHDA_CONFIGS=("$@")
(( $# )) || SIMPLEHDA_CONFIGS=(Debug Release)
for SIMPLEHDA_CONFIG in "${SIMPLEHDA_CONFIGS[@]}"; do
    if [[ "$SIMPLEHDA_CONFIG" != Debug && "$SIMPLEHDA_CONFIG" != Release ]]; then
        print -u2 -r -- "Invalid configuration: $SIMPLEHDA_CONFIG"
        exit 2
    fi
    if ! xcodebuild -project SimpleHDA.xcodeproj -scheme SimpleHDA -configuration "$SIMPLEHDA_CONFIG" -derivedDataPath Build/DerivedData build > "Build/xcode-${SIMPLEHDA_CONFIG}.log" 2>&1; then
        tail -n 100 "Build/xcode-${SIMPLEHDA_CONFIG}.log"
        exit 1
    fi
    python3 Tools/verify.py --configuration "$SIMPLEHDA_CONFIG"
    print -r -- "Built $SIMPLEHDA_CONFIG"
done
python3 Tools/prepare_hal.py
