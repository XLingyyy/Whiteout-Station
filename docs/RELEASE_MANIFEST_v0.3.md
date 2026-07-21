# v0.3 发布清单

生成日期：2026-07-22

## 归档

- 路径：`Builds/WhiteoutStation-v0.3-Win64`
- 启动文件：`Windows/WhiteoutStation.exe`
- 项目版本：0.3.0
- 平台/配置：Windows 64-bit / Shipping
- 文件数：42
- 总大小：774,272,463 bytes（约 738.4 MiB）
- 包体预算：2,500,000,000 bytes；实际占用约 31.0%
- 构建工具：Unreal Automation Tool `BuildCookRun` / UE 5.8
- Cook：694 个总包 / 687 个运行时包
- 内容格式：Pak + IoStore

`Builds/` 是本地发布产物并被 Git 忽略；源码、配置、资产许可、测试/验证脚本、QA 报告和可复现构建说明均纳入版本控制。

## 关键文件 SHA-256

| 文件 | 大小（bytes） | SHA-256 |
|---|---:|---|
| `Windows/WhiteoutStation.exe` | 171,520 | `1d2592875b7d9410f9a487e6f4e92c99b08652c475ebc71d02d62037875c1b20` |
| `Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe` | 172,462,592 | `afcd21b04d1533cf857161e5ca45da7e21aa492bad69c8f46296db0919f0d8a8` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.pak` | 26,113,011 | `2db4afc5b839f0b7e762392d07f8a843c30c8f492b01779e5ba60bbb198568b6` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.ucas` | 272,485,632 | `a622c649cafeebdfecf7660bfb01469027557a30c3d632f2a598009f215ab57f` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.utoc` | 252,343 | `fdf764d4cfd6690159834d366afb789bba535b6df583969c17a9e4ae17a3a642` |

## 包内随附文件

- `README_v0.3.txt`：启动、操作、目标、设置、离线对话与可选模型说明。
- `ASSET_LICENSES.md`：第三方、原创程序资产与 AI 生成资产的来源/工具/许可登记。
- `Validation/{medical,technical,quick}_EventLog.json`：三条无密钥 Shipping 路线事务与结算。
- `Validation/{medical,technical,quick}_RuntimeSmoke.png`：三张 1280×720 Shipping 结算截图。
- `Validation/no_key_smoke_summary.json`：子进程环境、分数与 `model_calls=0` 汇总。
- `Validation/performance_1080p.json`：1920×1080 Shipping 性能结构化结果。
- `Windows/WhiteoutStation/Content/Rules/`：规则 v0.1.0 原始 JSON，冻结哈希保持不变。
- `Windows/WhiteoutStation/Content/Agents/AgentRuntime.v0.1.json`：默认离线、固定 `deepseek-v4-flash`、无凭据字段。

## 发布校验

`python -X utf8 Tools/Release/validate_release_v03.py` 校验启动器、Shipping EXE、Pak/UCAS/UTOC、规则与 Agent 默认配置、三路线事件/分数/AP/截图、零模型调用、1080p 性能和 2.5 GB 包体预算，最终输出：

`RELEASE VALIDATION v0.3: PASS`

构建复现见 `docs/BUILD_AND_PLAY_v0.3.md`，验收详情见 `docs/QA_REPORT_v0.3.md`。
