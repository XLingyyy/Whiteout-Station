# G5 回归与发布验收记录

验收日期：2026-07-22

## 结论

G5 通过，v0.3 Demo 达到任务清单完成标准。全量规则回归、UE 自动化、三路线 Shipping 无密钥烟测、双分辨率截图基线、1080p 性能、包体预算、凭据扫描和规则冻结全部通过；最终归档可直接从 `Builds/WhiteoutStation-v0.3-Win64/Windows/WhiteoutStation.exe` 启动。

## V3-25 全量回归与截图基线

| 检查 | 最终结果 |
|---|---|
| Python 规则回归 | 17 / 17 PASS |
| UE Automation `WhiteoutStation` | 6 / 6 PASS，0 warning，0 error |
| 规则冻结 | 5 / 5 MATCH |
| StringTable | 255 条目、210 个引用键，PASS |
| 截图基线 | 46 个视角 × 2 分辨率 = 92 / 92 PASS |
| 反馈参考视角 | 5 / 5 均有 1280×720 与 1920×1080 对应帧 |
| 当前索引密钥扫描 | PASS |
| 分支/远端/标签历史密钥扫描 | PASS |

92 张基线按视角分类为：26 个 UI、10 个场景、4 个角色、3 个 LookAt、3 个灯光视角；1280×720 与 1920×1080 各 46 张，无错配、错尺寸或缺对。

| 反馈参考 | v0.3 对应视角 |
|---|---|
| `UI_01_ESC_Menu` | `UI_pause` |
| `UI_02_DialogueWheel` | `UI_dialogue_gu_wheel` |
| `UI_03_EvidenceBoard` | `UI_evidence` |
| `UI_04_HUD` | `UI_hud` |
| `UI_05_FocusCard` | `UI_focus_near` |

基线结构由 `Tools/Release/validate_v03_baseline.py` 自动验证；最终 UE 报告副本位于 `docs/evidence_v0.3/g5_shipping/automation_index.json`。

## V3-26 Shipping 与无密钥烟测

最终包由 UE 5.8 `BuildCookRun` 生成：Editor Development 与 Win64 Shipping 编译成功，694 个 Cook 包完成，IoStore 收录 687 个运行时包，Pak/IoStore、Stage 与 Archive 全部 ExitCode 0。

`Tools/Release/run_v03_no_key_smoke.py` 对每个 Shipping 子进程移除 `WHITEOUT_LLM_API_KEY` 与 `WHITEOUT_LLM_ENABLED`，并传入 `-WhiteoutLLMEnabled=false`。三个进程分别生成全新的事件日志和 1280×720 结算截图：

| 路线 | 结局 | 事件 | 剩余 AP | 分数 | 模型调用 | 结果 |
|---|---|---:|---:|---:|---:|---|
| medical | TaskSuccess | 8 | 0 | 76.64 | 0 | PASS |
| technical | TaskSuccess | 8 | 0 | 71.90 | 0 | PASS |
| quick | TaskSuccess | 6 | 2 | 72.06 | 0 | PASS |

三张 Shipping 结算截图已人工复核：中文、评分、AP、状态条与具象事件时间线完整可读，无开场/结局层回跳或裁切。结构化摘要、事件日志和截图位于 `docs/evidence_v0.3/g5_shipping/`。

## 性能与包体

Shipping 室外暴雪视角在 1920×1080 预热 5 秒、采样 15 秒：1522 帧，平均 101.40 FPS，1% Low 84.42 FPS，P95 10.508 ms，P99 11.000 ms，最大 16.336 ms；平均和 1% Low 均高于 60 FPS 门槛。

发布归档共 42 个文件、774,272,463 bytes（约 738.4 MiB），低于 2.5 GB 预算。`Tools/Release/validate_release_v03.py` 最终输出 `RELEASE VALIDATION v0.3: PASS`。

## 规则与安全边界

- `WhiteoutStation/Content/Rules`、`Tools/Rules`、`WhiteoutStation/Source/*/State` 未修改；规则版本仍为 0.1.0。
- 发布包的 Agent 配置固定 `deepseek-v4-flash` 与官方兼容端点，但 `llm_enabled=false`，不含凭据字段。
- 在线模型仍是明确 opt-in 表达层；无密钥离线路径已由三个独立 Shipping 进程证明可完成整局。

发布明细见 `docs/RELEASE_MANIFEST_v0.3.md`，完整 QA 说明见 `docs/QA_REPORT_v0.3.md`，游玩与复现命令见 `docs/BUILD_AND_PLAY_v0.3.md`。
