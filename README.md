<!--
  README.md
  SimpleHDA

  Created by laobamac on 2026/8/22.
  Copyright © 2026 laobamac. All rights reserved.
-->
<div align="center">

# SimpleHDA

**在 macOS Tahoe 上恢复原版 AppleHDA**

![Version](https://img.shields.io/badge/version-1.0.0-2563eb)
![macOS](https://img.shields.io/badge/macOS-Tahoe%2026-111827)
![Architecture](https://img.shields.io/badge/arch-x86__64-6b7280)
![OpenCore](https://img.shields.io/badge/boot-OpenCore-16a34a)

**简体中文** · [English](README.en.md)

OpenCore 注入 · 原版音频栈 · HAL 自动检测 · 设备名称本地化

</div>

---

SimpleHDA 是一个 Lilu 插件，通过 OpenCore 注入恢复原版 AppleHDA，让 AppleALC 继续使用现有的声卡配置。驱动不修改系统卷，不重建内核集合。

## 要求

| 项目 | 要求 |
| :--- | :--- |
| 系统 | macOS Tahoe 26 / Darwin 25.x |
| 架构 | Intel / AMD x86_64 |
| 引导 | OpenCore |
| 依赖 | Lilu ≥ 1.7.3、AppleALC |
| 配置 | 有效的 `alcid` 或 `layout-id`，启动参数 `keepsyms=1` |

## 安装

1. 解压 `SimpleHDA-1.0.0-REL.zip`，将 `SimpleHDA.kext` 放入 `EFI/OC/Kexts`。
2. 在 `Kernel → Add` 添加驱动，顺序为 **Lilu → AppleALC → SimpleHDA**。包内附有 `OpenCore-Kernel-Add.plist` 条目示例。
3. 加入 `keepsyms=1`，保留这台机器原先有效的 AppleALC、layout 和 ACPI 配置。
4. 可选：安装 `SimpleHDA-HAL-1.0.0.pkg`。
5. 重启。

使用前卸载其他 AppleHDA 恢复方案或根补丁，避免重复加载。

## 启动参数

| 参数 | 用途 |
| :--- | :--- |
| `keepsyms=1` | 必需 |
| `-shdaoff` | 停用驱动 |
| `-shdahaloff` | 手动停用 HAL，供故障对照 |
| `shda-lang=en` | 切换名称语言，默认 `zh_CN` |
| `-shdadbg` | 开启 Debug 版本日志 |

`alcid` 使用自己声卡的有效值；通过设备属性设置 layout 时，无需重复添加。

## HAL

HAL 为 CoreAudio 提供 AppleHDA 专用属性与控制接口。驱动自动检测：**已安装就启用，未安装就使用普通 IOAudio 路径**。两种情况都无需设置开关。

安装包将原版签名插件放在：

```text
/Library/Audio/Plug-Ins/SimpleHDA/AppleHDAHALPlugIn.bundle
```

安装或卸载后重启即可，无需挂载 EFI。卸载命令：

```zsh
sudo zsh Tools/uninstall_hal.zsh
```

## 构建

需要完整 Xcode、Git 和 Python 3。

```zsh
zsh Tools/build.zsh
python3 Tools/package.py
```

产物位于 `Dist`，默认驱动为 Release，Debug 驱动放在 `Debug` 目录，另附 HAL 安装包及 SHA-256 校验清单。仅编译 Release：`zsh Tools/build.zsh Release`。

依赖固定在 `VendorVersions.json`，原版载荷位于 `Resources/AppleHDA.kext`。准备依赖后可用 `SimpleHDA.xcodeproj` 构建；版本号以 `Info.plist` 为准，标识为 `cn.laobamac.SimpleHDA`。

推送自动更新固定的 `pre` 预发布，仅保留最新附件。发布正式 Release 后自动构建并上传 REL 包；tag 使用与 `Info.plist` 一致的 `1.0.0` 或 `v1.0.0`。

## 致谢

[Lilu](https://github.com/acidanthera/Lilu) · [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK) · [AppleALC](https://github.com/acidanthera/AppleALC) · [Apple XNU](https://github.com/apple-oss-distributions/xnu)

第三方组件及许可见 [NOTICE](NOTICE.md)。

<div align="center">

Copyright © 2026 **laobamac**. All rights reserved.

</div>
