# GATE G4 v0.4 — 对话流程重构

日期：2026-07-22
结论：**PASS**

## 功能结果

- 对话流程已改为“NPC 开场白 → 显式选意图 → 可选输入 → 发送 → NPC 回应”。
- 询问、质疑、安抚、承诺四种意图仍是唯一规则输入；承诺仍只有原三种条件。
- 自由文本不再调用 `RequestDialogueIntent`猜测意图；最多 280 字的 `player_said` 仅透传给表达层。
- 空文本按原确定性意图提交；对抗文本在本地拦截，不提交、不消耗模型。
- 每轮发送仍是一次 talk 事务；AP、`max_uses=2`、状态和结算未改。
- 对话条为透明全屏容器上的底部黑色毛玻璃；右上 NPC 卡已移除肖像。

## 回归结果

| 项目 | 结果 |
|---|---|
| Python 规则 | 17 / 17 PASS |
| `Tools/Rules/run_routes.py` | 3 路线成功 |
| UE `WhiteoutStation` 自动化 | 6 / 6 PASS |
| 冻结哈希 | 5 / 5 PASS |
| Runtime AutoRoute medical | TaskSuccess / 76.64 |
| Runtime AutoRoute technical | TaskSuccess / 71.90 |
| Runtime AutoRoute quick | TaskSuccess / 72.06 |
| 无密钥表达 | 确定性预设，UI 不空白 |
| mock 承诺文本 | `expression / accepted`，`player_said` 完整 |
| mock 询问文本 | `expression / accepted`，`player_said` 完整 |
| 端点失联降级 | `provider_unavailable` 后回退预设 |
| 对抗文本 | `adversarial_input_blocked`，模型调用增量 0 |

## 证据

- 开场白 7 态、承诺、输入、回应：`docs/baseline_v0.4/UI_dialogue_*_1280x720.png`
- Runtime 截图与日志：`docs/evidence_v0.4/g4_dialogue/`
- 脱敏审计样例：`docs/evidence_v0.4/g4_dialogue/model_audit_samples.jsonl`
- 引擎原审计：`WhiteoutStation/Saved/Logs/WhiteoutStation_ModelAudit.jsonl`（未入库）

`ValidateModelPayload`、事实白名单、受保护短语、模型名、温度、token 上限均未改。
