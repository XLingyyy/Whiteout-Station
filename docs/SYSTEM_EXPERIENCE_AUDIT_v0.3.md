# v0.3 系统体验审计

审计日期：2026-07-22
对应任务：V3-20 至 V3-24

## 结论

G4 的五项系统体验工作均已实现并通过实机验证：ESC 设置实时生效且跨进程保持；AP 时钟覆盖 HUD、菜单、开场、危机和结算时间线；空格跳跃参数克制且不改变路线可达性；控制室过曝与天线逆光完成定点修正；v0.2 文档口径完成追溯统一。所有改动均位于表现、输入、设置或验证层，权威规则冻结区未改动。

## V3-20 ESC 选项页

设置页由 ESC 暂停菜单的“设置｜视野与音量”进入，提供五个实时滑条并保留可读数值：

| 设置 | 范围 / 默认 | 运行时目标 |
|---|---|---|
| 视野 FOV | 75°—105° / 90° | 第一人称相机 |
| 主音量 | 0%—100% / 100% | `SC_WS_Master`，含子类 |
| 氛围音量 | 0%—100% / 100% | `SC_WS_Ambience` |
| 效果音量 | 0%—100% / 100% | `SC_WS_Foley`、`SC_WS_Cinematic`、`SC_WS_Music` |
| 反馈音量 | 0%—100% / 100% | `SC_WS_UI` |

`UWhiteoutSettingsSubsystem` 将本机设置写入平台 `GameUserSettings.ini` 的 `WhiteoutStation.LocalSettings` 节；不进入仓库、存档或规则状态。滑条变化立即更新相机或运行时 SoundMix，并立即落盘。

独立进程验证先写入 FOV 103°、主音量 0.76、氛围 0.48、效果 0.64、反馈 0.82，再退出并启动第二个进程读取；五项值及相机实际 FOV 全部一致：

- `docs/evidence_v0.3/G4_SettingsAudit_write.json`
- `docs/evidence_v0.3/G4_SettingsAudit_verify.json`
- `docs/baseline_v0.3/UI_settings_{default,adjusted}_{1280x720,1920x1080}.png`

## V3-21 AP 时钟

时钟是纯表现映射：`08:15 + (8 - 剩余 AP) × 75 分钟`，不参与任何规则判断。

| 剩余 AP | 显示时间 |
|---:|---:|
| 8 | 08:15 |
| 7 | 09:30 |
| 6 | 10:45 |
| 5 | 12:00 |
| 4 | 13:15 |
| 3 | 14:30 |
| 2 | 15:45 |
| 1 | 17:00 |
| 0 | 18:15 |

HUD 与 ESC 读取当前 AP；开场显示 08:15；危机演出显示 13:15；结算事件时间线按每条事件的 `APAfter` 显示对应时间。双分辨率证据为 `UI_clock_0815`、`UI_clock_1815`、`UI_opening_controls_time`、`UI_crisis_emergency_time` 和 `UI_results_timeline_time`。

## V3-22 跳跃

角色跳跃参数为 `JumpZVelocity=350`、`GravityScale=1.15`、`AirControl=0.10`。空格在开场时仍可跳过演出，同时使用标准 `Jump/StopJumping` 输入；未引入二段跳、攀爬或新导航能力。

实机探针在低顶区域完成起跳、最高点和落地全序列：

- 实测高度 43.07 cm，滞空记录 0.35 s，7 帧证据；
- 胶囊半高 92 cm，最高胶囊顶约 231.07 cm；低顶高度 365 cm，名义余量约 133.93 cm；
- 全程 `blocking_overlap_detected=false`，正常落地；
- `docs/evidence_v0.3/G4_JumpAudit.json`、`G4_JumpCapture.mp4` 与 `jump_sequence/` 可复查。

AutoRoute 不调用跳跃，medical / technical / quick 三路线仍分别为 TaskSuccess 76.64 / 71.90 / 72.06，证明路线和交互点不依赖新输入。

## V3-23 灯光

- 全局 Bloom 从 0.28 收敛至 0.16。
- 控制室冷色补光调整为冷蓝 `(0.28, 0.50, 0.78)`、强度 820、半径 700，保留顶灯灯条轮廓和周边设备细节。
- 天线检修灯移至镜头侧补光位 `(2140, 400, 245)`，颜色 `(0.48, 0.63, 0.92)`、强度 4800、半径 1150，使天线在雪天空逆光下保持可辨轮廓。

三组同视角对照位于：

- `docs/evidence_v0.3/lighting_comparisons/ControlCeiling_v02-v03.png`
- `docs/evidence_v0.3/lighting_comparisons/AntennaFront_v02-v03.png`
- `docs/evidence_v0.3/lighting_comparisons/AntennaSide_v02-v03.png`

对应 v0.3 单帧另提供 1280×720 与 1920×1080 两档基线。捕获序列在每个视角前恢复默认预览灯光并等待曝光稳定，避免跨帧状态污染。

## V3-24 文档口径

修订详情见 `docs/DOCUMENTATION_RECONCILIATION_v0.3.md`。统一结论为：v0.2 G1 的 223/207 是门禁快照，v0.2 QA 的 236/210 是封版值，v0.3 G4 当前为 255/210。v0.2 唯一清单外的玩家功能 Esc 菜单已由 V3-07 追认、V3-20 扩展；AutoRoute、截图和探针属于 QA 基础设施。

## 回归矩阵

| 检查 | 结果 |
|---|---|
| Editor Development 编译 | PASS |
| Python 规则回归 | 17 / 17 PASS |
| UE Automation（无模型） | 6 / 6 PASS |
| 规则冻结 | 5 / 5 MATCH |
| StringTable | PASS：255 条目、210 个引用键 |
| 无模型 AutoRoute `medical` | TaskSuccess，76.64 |
| 无模型 AutoRoute `technical` | TaskSuccess，71.90 |
| 无模型 AutoRoute `quick` | TaskSuccess，72.06 |
| 设置跨进程持久化 | PASS：5 / 5 值一致 |
| 跳跃低顶碰撞 | PASS：正常落地、无阻挡重叠 |
| G4 双分辨率截图 | PASS：20 / 20，尺寸复核通过 |
| 灯光对照与跳跃视频 | PASS：3 组对照 + 1 个 MP4 |
