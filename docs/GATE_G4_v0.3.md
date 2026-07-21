# G4 系统体验验收记录

验收日期：2026-07-22

## 结论

G4 通过。ESC 选项页、AP 时钟、跳跃、灯光打磨和文档口径五项全部闭环；设置与跳跃均由实际游戏进程生成结构化审计证据，三路线分数与 v0.2 保持一致，规则冻结区零改动。

## 交付摘要

| 任务 | 交付与结果 |
|---|---|
| V3-20 ESC 选项页 | FOV 75°—105°；主/氛围/效果/反馈四类音量映射既有 SoundClass；实时生效；独立进程重启后 5/5 值保持 |
| V3-21 AP 时钟 | 08:15 起每 AP 75 分钟，0 AP 为 18:15；HUD、ESC、开场、危机、结算时间线全部接入 |
| V3-22 跳跃 | 空格标准跳跃；实测 43.07 cm；低顶区正常落地且无阻挡重叠；AutoRoute 不依赖跳跃 |
| V3-23 灯光 | Bloom 收敛；控制室灯条不过曝；天线逆光补光；3 组 v0.2/v0.3 同视角对照 |
| V3-24 文档 | 223/207、236/210、255/210 三个时点口径统一；Esc 清单外功能完成追认 |

实现、参数、时钟映射和证据索引详见 `docs/SYSTEM_EXPERIENCE_AUDIT_v0.3.md`；文档修订追溯见 `docs/DOCUMENTATION_RECONCILIATION_v0.3.md`。

## 验收证据

- 设置：`docs/evidence_v0.3/G4_SettingsAudit_{write,verify}.json`，两个独立 Unreal 进程均 `passed=true`。
- 跳跃：`docs/evidence_v0.3/G4_JumpAudit.json`、`G4_JumpCapture.mp4`、7 帧 `jump_sequence/`。
- 灯光：`docs/evidence_v0.3/lighting_comparisons/` 三组 v0.2/v0.3 合成图。
- 双分辨率基线：设置、08:15/18:15 时钟、开场、危机、结算时间线与三处灯光共 10 个视角、20 张 PNG，全部为 1280×720 或 1920×1080。

## 门禁结果

| 检查 | 结果 |
|---|---|
| Editor Development 编译 | PASS |
| Python 规则回归 | 17 / 17 PASS |
| UE Automation（无模型） | 6 / 6 PASS |
| 无模型 AutoRoute `medical` | TaskSuccess，76.64，与 v0.2 一致 |
| 无模型 AutoRoute `technical` | TaskSuccess，71.90，与 v0.2 一致 |
| 无模型 AutoRoute `quick` | TaskSuccess，72.06，与 v0.2 一致 |
| 规则冻结 | 5 / 5 MATCH |
| StringTable | PASS：255 条目、210 个引用键 |
| 密钥扫描（索引） | PASS |
| 设置跨进程持久化 | PASS |
| 跳跃低顶审计 | PASS |
| 双分辨率截图 | 20 / 20 PASS |

本门禁未修改 `WhiteoutStation/Content/Rules`、`Tools/Rules` 或 `WhiteoutStation/Source/*/State`。
