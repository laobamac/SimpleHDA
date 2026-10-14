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
| 依赖 | Lilu ≥ 1.7.1、AppleALC |
| 配置 | 有效的 `alcid` 或 `layout-id`，启动参数 `keepsyms=1` |

## 安装

1. 在 [Releases](https://github.com/laobamac/SimpleHDA/releases) 下载并解压 `RELEASE` 包，将 `SimpleHDA.kext` 放入 `EFI/OC/Kexts`。
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
| `shda-lang=en` | 指定名称语言，优先于自动探测 |
| `-shdadbg` | 开启 Debug 版本日志 |

`alcid` 使用自己声卡的有效值；通过设备属性设置 layout 时，无需重复添加。

<details>
<summary>shda-lang 支持的语言（41 种）</summary>

默认自动读取 NVRAM `prev-lang:kbd` 中的语言。优先级：**有效的 `shda-lang` → NVRAM 语言 → `zh_CN`**。例如 `shda-lang=ja` 强制使用日语，修改后重启生效。

`prev-lang:kbd` 可能由 OpenCore 固定，与登录用户的语言设置不同；此时可通过 `shda-lang` 指定。

| 值 | 语言 |
| :--- | :--- |
| `ar` | 阿拉伯语 |
| `ca` | 加泰罗尼亚语 |
| `cs` | 捷克语 |
| `da` | 丹麦语 |
| `de` | 德语 |
| `el` | 希腊语 |
| `en` | 英语 |
| `en_AU` | 英语（澳大利亚） |
| `en_GB` | 英语（英国） |
| `es` | 西班牙语 |
| `es_419` | 西班牙语（拉丁美洲） |
| `es_US` | 西班牙语（美国） |
| `fi` | 芬兰语 |
| `fr` | 法语 |
| `fr_CA` | 法语（加拿大） |
| `he` | 希伯来语 |
| `hi` | 印地语 |
| `hr` | 克罗地亚语 |
| `hu` | 匈牙利语 |
| `id` | 印度尼西亚语 |
| `it` | 意大利语 |
| `ja` | 日语 |
| `ko` | 韩语 |
| `ms` | 马来语 |
| `nl` | 荷兰语 |
| `no` | 挪威语 |
| `pl` | 波兰语 |
| `pt_BR` | 葡萄牙语（巴西） |
| `pt_PT` | 葡萄牙语（葡萄牙） |
| `ro` | 罗马尼亚语 |
| `ru` | 俄语 |
| `sk` | 斯洛伐克语 |
| `sl` | 斯洛文尼亚语 |
| `sv` | 瑞典语 |
| `th` | 泰语 |
| `tr` | 土耳其语 |
| `uk` | 乌克兰语 |
| `vi` | 越南语 |
| `zh_CN` | 简体中文（探测失败时使用） |
| `zh_HK` | 繁体中文（香港） |
| `zh_TW` | 繁体中文（台湾） |

值不区分大小写，`-` 与 `_` 等价，例如 `zh-CN` 与 `zh_CN`。

中文别名：`zh-Hans`、`zh-Hans-CN` → `zh_CN`；`zh-Hant`、`zh-Hant-TW` → `zh_TW`；`zh-Hant-HK` → `zh_HK`。

区域值未匹配时会尝试基础语言，例如 `en_US` → `en`、`ja_JP` → `ja`。无效的启动参数不覆盖自动探测结果。

</details>

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

产物位于 `Dist`，`DEBUG` 和 `RELEASE` 分别打包，每包均附 HAL 安装包及 SHA-256 校验清单。日常使用选择 `RELEASE`，排查问题选择 `DEBUG`。仅编译 Release：`zsh Tools/build.zsh Release`。

使用 Xcode 时，首次运行 `zsh Tools/prepare.zsh`，再打开 `SimpleHDA.xcodeproj`。按 `⌘B` 构建，构建阶段也会自动准备依赖和生成源码。

依赖固定在 `VendorVersions.json`，原版载荷位于 `Resources/AppleHDA.kext`。版本号以 `Info.plist` 为准，标识为 `cn.laobamac.SimpleHDA`。

推送自动更新 `v1.0.0-alpha` 预发布，只保留最新预发布及附件。发布说明列出最近 5 条提交，文件名包含提交短哈希，例如 `SimpleHDA-Tahoe-v1.0.0-RELEASE-alpha-abcdef0.zip`。

发布正式 Release 后自动上传 `DEBUG`、`RELEASE` 包，例如 `SimpleHDA-Tahoe-v1.0.0-RELEASE.zip`；tag 使用与 `Info.plist` 一致的 `1.0.0` 或 `v1.0.0`。

## 致谢

[Lilu](https://github.com/acidanthera/Lilu) · [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK) · [AppleALC](https://github.com/acidanthera/AppleALC) · [Apple XNU](https://github.com/apple-oss-distributions/xnu)

第三方组件及许可见 [NOTICE](NOTICE.md)。

<div align="center">

Copyright © 2026 **laobamac**. All rights reserved.

</div>
