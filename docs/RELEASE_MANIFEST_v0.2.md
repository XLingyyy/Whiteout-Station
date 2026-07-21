# v0.2 发布清单

生成日期：2026-07-21

## 归档

- 路径：`Builds/WhiteoutStation-v0.2-Win64`
- 启动文件：`Windows/WhiteoutStation.exe`
- 平台/配置：Windows 64-bit / Shipping
- 文件数：40
- 总大小：764,172,929 bytes（约 728.8 MiB）
- 构建工具：Unreal Automation Tool `BuildCookRun`
- Cook：645 个总包 / 638 个运行时包
- 内容格式：Pak + IoStore

`Builds/` 是本地发布产物并被 Git 忽略；源码、配置、许可、测试、验证脚本与可复现构建说明均纳入版本控制。

## 关键文件 SHA-256

| 文件 | 大小（bytes） | SHA-256 |
|---|---:|---|
| `Windows/WhiteoutStation.exe` | 171,520 | `1d2592875b7d9410f9a487e6f4e92c99b08652c475ebc71d02d62037875c1b20` |
| `Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe` | 172,328,960 | `8355bae723cbd81215d9517531de6eb9237b86358a7431f21dc35bc177f832a5` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.pak` | 26,110,963 | `8a9f213f171da5004b80262991395c252926a4a78d680bbc3e62324dc1303736` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.ucas` | 263,688,272 | `4483b7ac78bb2e041369e48a3d388e429d66eb99ee2051f65844c412ec57dade` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.utoc` | 240,646 | `eee2f5203213f78bc3ed22a8a82b55d52371507607715acf55c840d1da8138f2` |

## 发布内随附文件

- `README_v0.2.txt`：启动、操作、目标、存档、退出和可选模型参数。
- `ASSET_LICENSES.md`：第三方资产来源、作者、许可和校验记录。
- `Validation/`：medical、technical、quick 三条 Shipping 路线的事件日志与结果截图。
- `Windows/WhiteoutStation/Content/Paks/`：已 cook 的游戏内容、权威规则和默认离线 Agent 配置。

构建复现见 `docs/BUILD_AND_PLAY_v0.2.md`，验收详情见 `docs/QA_REPORT_v0.2.md`。
