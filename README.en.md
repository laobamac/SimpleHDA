<!--
  README.en.md
  SimpleHDA

  Created by laobamac on 2026/8/18.
  Copyright © 2026 laobamac. All rights reserved.
-->
<div align="center">

# SimpleHDA

**Restore native AppleHDA on macOS Tahoe**

![Version](https://img.shields.io/badge/version-1.0.0-2563eb)
![macOS](https://img.shields.io/badge/macOS-Tahoe%2026-111827)
![Architecture](https://img.shields.io/badge/arch-x86__64-6b7280)
![OpenCore](https://img.shields.io/badge/boot-OpenCore-16a34a)

[简体中文](README.md) · **English**

OpenCore injection · Native audio stack · Automatic HAL detection · Localized device names

</div>

---

SimpleHDA is a Lilu plug-in that restores the original AppleHDA stack through OpenCore injection, allowing AppleALC to use your existing audio configuration. It does not modify the system volume or rebuild kernel collections.

## Requirements

| Component | Requirement |
| :--- | :--- |
| System | macOS Tahoe 26 / Darwin 25.x |
| Architecture | Intel / AMD x86_64 |
| Bootloader | OpenCore |
| Dependencies | Lilu ≥ 1.7.3 and AppleALC |
| Configuration | A working `alcid` or `layout-id`, plus `keepsyms=1` |

## Installation

1. Extract `SimpleHDA-1.0.0-REL.zip` and copy `SimpleHDA.kext` to `EFI/OC/Kexts`.
2. Add it to `Kernel → Add` in this order: **Lilu → AppleALC → SimpleHDA**. The package includes an example entry in `OpenCore-Kernel-Add.plist`.
3. Add `keepsyms=1` and retain the working AppleALC, layout and ACPI configuration for your machine.
4. Optionally install `SimpleHDA-HAL-1.0.0.pkg`.
5. Reboot.

Remove other AppleHDA restoration solutions or root patches before installation to avoid loading the stack twice.

## Boot arguments

| Argument | Purpose |
| :--- | :--- |
| `keepsyms=1` | Required |
| `-shdaoff` | Disable the driver |
| `-shdahaloff` | Manually disable HAL for troubleshooting |
| `shda-lang=en` | Change the language of device names; default: `zh_CN` |
| `-shdadbg` | Enable logging in the Debug build |

Use the appropriate `alcid` for your codec. If the layout is configured through device properties, an additional alcid argument is unnecessary.

## HAL

HAL provides AppleHDA-specific properties and controls to CoreAudio. SimpleHDA detects it automatically: **installed HAL is enabled; without HAL, the ordinary IOAudio path is used**. Neither configuration requires a switch.

The installer places the original signed plug-in at:

```text
/Library/Audio/Plug-Ins/SimpleHDA/AppleHDAHALPlugIn.bundle
```

Reboot after installation or removal. Mounting the EFI partition is unnecessary. To uninstall:

```zsh
sudo zsh Tools/uninstall_hal.zsh
```

## Building

Requires full Xcode, Git and Python 3.

```zsh
zsh Tools/build.zsh
python3 Tools/package.py
```

Packages are written to `Dist`, with the Release driver at the top level, a Debug driver in `Debug`, the HAL installer and SHA-256 manifests. To build only Release, run `zsh Tools/build.zsh Release`.

Dependencies are pinned in `VendorVersions.json`; the original payload is in `Resources/AppleHDA.kext`. Once dependencies are prepared, you can build with `SimpleHDA.xcodeproj`. `Info.plist` supplies the version, and the bundle identifier is `cn.laobamac.SimpleHDA`.

Each push updates the fixed `pre` prerelease with the latest assets. Publishing a formal Release automatically builds and uploads the REL package. Use `1.0.0` or `v1.0.0` as the tag, matching `Info.plist`.

## Acknowledgments

[Lilu](https://github.com/acidanthera/Lilu) · [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK) · [AppleALC](https://github.com/acidanthera/AppleALC) · [Apple XNU](https://github.com/apple-oss-distributions/xnu)

See [NOTICE](NOTICE.md) for third-party components and licenses.

<div align="center">

Copyright © 2026 **laobamac**. All rights reserved.

</div>
