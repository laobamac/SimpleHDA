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
| Dependencies | Lilu ≥ 1.7.1 and AppleALC |
| Configuration | A working `alcid` or `layout-id`, plus `keepsyms=1` |

## Installation

1. Download the `RELEASE` package from [Releases](https://github.com/laobamac/SimpleHDA/releases), extract it and copy `SimpleHDA.kext` to `EFI/OC/Kexts`.
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
| `shda-lang=en` | Override the automatically detected language of device names |
| `-shdadbg` | Enable logging in the Debug build |

Use the appropriate `alcid` for your codec. If the layout is configured through device properties, an additional alcid argument is unnecessary.

<details>
<summary>Languages supported by shda-lang (41 locales)</summary>

The language is detected from NVRAM `prev-lang:kbd` by default. Priority: **valid `shda-lang` → NVRAM language → `zh_CN`**. For example, `shda-lang=ja` forces Japanese. Reboot after changing it.

OpenCore may fix `prev-lang:kbd` to a value that differs from the logged-in user's language. Use `shda-lang` to override it when needed.

| Value | Language |
| :--- | :--- |
| `ar` | Arabic |
| `ca` | Catalan |
| `cs` | Czech |
| `da` | Danish |
| `de` | German |
| `el` | Greek |
| `en` | English |
| `en_AU` | English (Australia) |
| `en_GB` | English (United Kingdom) |
| `es` | Spanish |
| `es_419` | Spanish (Latin America) |
| `es_US` | Spanish (United States) |
| `fi` | Finnish |
| `fr` | French |
| `fr_CA` | French (Canada) |
| `he` | Hebrew |
| `hi` | Hindi |
| `hr` | Croatian |
| `hu` | Hungarian |
| `id` | Indonesian |
| `it` | Italian |
| `ja` | Japanese |
| `ko` | Korean |
| `ms` | Malay |
| `nl` | Dutch |
| `no` | Norwegian |
| `pl` | Polish |
| `pt_BR` | Portuguese (Brazil) |
| `pt_PT` | Portuguese (Portugal) |
| `ro` | Romanian |
| `ru` | Russian |
| `sk` | Slovak |
| `sl` | Slovenian |
| `sv` | Swedish |
| `th` | Thai |
| `tr` | Turkish |
| `uk` | Ukrainian |
| `vi` | Vietnamese |
| `zh_CN` | Simplified Chinese (fallback) |
| `zh_HK` | Traditional Chinese (Hong Kong) |
| `zh_TW` | Traditional Chinese (Taiwan) |

Values are case-insensitive, and `-` and `_` are interchangeable, such as `zh-CN` and `zh_CN`.

Chinese aliases: `zh-Hans` and `zh-Hans-CN` → `zh_CN`; `zh-Hant` and `zh-Hant-TW` → `zh_TW`; `zh-Hant-HK` → `zh_HK`.

If a regional value is unavailable, the base language is tried, such as `en_US` → `en` or `ja_JP` → `ja`. Invalid boot arguments do not override automatic detection.

</details>

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

Separate `DEBUG` and `RELEASE` packages are written to `Dist`. Each includes the HAL installer and SHA-256 manifest. Use `RELEASE` for daily use and `DEBUG` for troubleshooting. To build only Release, run `zsh Tools/build.zsh Release`.

For Xcode, run `zsh Tools/prepare.zsh` once before opening `SimpleHDA.xcodeproj`. Press `⌘B` to build. The build phase also prepares dependencies and generated sources automatically.

Dependencies are pinned in `VendorVersions.json`; the original payload is in `Resources/AppleHDA.kext`. `Info.plist` supplies the version, and the bundle identifier is `cn.laobamac.SimpleHDA`.

Each push updates the `v1.0.0-alpha` prerelease, keeping only the latest prerelease and assets. Release notes list the five most recent commits. Filenames include the short commit hash, such as `SimpleHDA-Tahoe-v1.0.0-RELEASE-alpha-abcdef0.zip`.

Publishing a formal Release automatically uploads `DEBUG` and `RELEASE` packages, such as `SimpleHDA-Tahoe-v1.0.0-RELEASE.zip`. Use `1.0.0` or `v1.0.0` as the tag, matching `Info.plist`.

## Acknowledgments

[Lilu](https://github.com/acidanthera/Lilu) · [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK) · [AppleALC](https://github.com/acidanthera/AppleALC) · [Apple XNU](https://github.com/apple-oss-distributions/xnu)

See [NOTICE](NOTICE.md) for third-party components and licenses.

<div align="center">

Copyright © 2026 **laobamac**. All rights reserved.

</div>
