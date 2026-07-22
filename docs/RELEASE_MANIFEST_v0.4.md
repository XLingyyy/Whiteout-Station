# v0.4 发布清单

生成日期：2026-07-22

## 归档

- 路径：`Builds/WhiteoutStation-v0.4-Win64`
- 启动文件：`Windows/WhiteoutStation.exe`
- 项目版本：0.4.0
- 平台/配置：Windows 64-bit / Shipping
- 文件数：42
- 总大小：774,388,637 bytes（约 738.5 MiB）
- 包体预算：2,500,000,000 bytes；实际占用约 31.0%
- 构建工具：Unreal Automation Tool `BuildCookRun` / UE 5.8
- Cook：694 个总包 / 687 个运行时包
- 内容格式：Pak + IoStore

`Builds/` 是本地发布产物并被 Git 忽略；源码、配置、资产许可、测试/验证脚本、QA 报告和可复现构建说明均纳入版本控制。

## 关键文件 SHA-256

| 文件 | 大小（bytes） | SHA-256 |
|---|---:|---|
| `Windows/WhiteoutStation.exe` | 171,520 | `1d2592875b7d9410f9a487e6f4e92c99b08652c475ebc71d02d62037875c1b20` |
| `Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe` | 172,497,920 | `51e9d83b8bed9d5fbe7e006a4ca4b90c807e9c7e97e842e2cdd4a0e50fac20ee` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.pak` | 26,113,011 | `5c10370f7ea74080b825bbaac2acbfcdd9881c37b6f066507ca60a18fd7b0977` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.ucas` | 272,426,240 | `f7a81d739baefc70d2b882087fae1c1afa3a2ebac0215528ff55d7db59712fe9` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.utoc` | 252,343 | `54e9f47cb9caa653881ea119439d9fdc59dc72787d8af9f76d2bc57f43bd634c` |

## 包内随附文件

- `README_v0.4.txt`：启动、操作、目标、证据板、对话、设置和可选表达服务说明。
- `ASSET_LICENSES.md`：第三方、原创程序资产和 AI 生成资产的来源、工具与许可登记。
- `Validation/{medical,technical,quick}_EventLog.json`：三条无密钥 Shipping 路线事务与结算。
- `Validation/{medical,technical,quick}_RuntimeSmoke.png`：三张 1280×720 Shipping 结算截图。
- `Validation/no_key_smoke_summary.json`：子进程环境、分数和 `model_calls=0` 汇总。
- `Validation/performance_1080p.json`：1920×1080 Shipping 性能结果。
- `Windows/WhiteoutStation/Content/Rules/`：规则 v0.1.0 原始 JSON，冻结哈希保持不变。
- `Windows/WhiteoutStation/Content/Agents/AgentRuntime.v0.1.json`：默认离线、固定 `deepseek-v4-flash`、无凭据字段。

## 发布校验

`python -X utf8 Tools/Release/validate_release_v04.py` 校验项目版本 0.4.0、启动器、Shipping EXE、Pak/UCAS/UTOC、随包说明、规则与 Agent 默认配置、三路线事件/分数/AP/截图、零模型调用、1080p 性能和 2.5 GB 包体预算，最终输出：

`RELEASE VALIDATION v0.4: PASS`

构建复现见 `docs/BUILD_AND_PLAY_v0.4.md`，验收详情见 `docs/QA_REPORT_v0.4.md`。整版发布仍需按 `docs/GATE_G3_v0.4.md` 处理 Idle 源动画决定。
