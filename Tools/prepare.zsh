#!/bin/zsh
#
#  prepare.zsh
#  SimpleHDA
#
#  Created by laobamac on 2026/8/16.
#  Copyright © 2026 laobamac. All rights reserved.
#
set -euo pipefail
cd "${0:A:h:h}"
export PYTHONDONTWRITEBYTECODE=1
python3 Tools/setup.py
python3 Tools/verify.py --source
python3 Tools/prepare_vendor.py
python3 Tools/prepare_payload.py
