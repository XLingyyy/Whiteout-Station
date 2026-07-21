# v0.3 自由文本意图与降级测试报告

验收日期：2026-07-22

## 结论

自由文本仅映射到规则已支持的 `询问 / 质疑 / 承诺 / 安抚`。承诺只接受
`keep_records / reserve_medicine / heat_repair_room`；命令、交易、状态修改与提示词注入均不会产生规则外效果。

## 中文样例集

UE 自动化 `WhiteoutStation.Agents.DialogueBoundary` 固化了 30 条中文样例，结果为 **30 / 30（100%）**：

| 分组 | 数量 | 结果 | 覆盖 |
|---|---:|---:|---|
| 询问 | 5 | 5 / 5 | 原因、人物、设备、保护回路 |
| 质疑 | 6 | 6 / 6 | 撒谎、证据矛盾、隐瞒、事故质疑 |
| 安抚 | 6 | 6 / 6 | 安心、陪伴、冷静；含“安抚+询问”歧义句 |
| 三类承诺 | 9 | 9 / 9 | 不弃站、保留药品、维修间升温，各 3 条 |
| 对抗/规则外/低信息 | 4 | 4 / 4 安全拒绝 | 改 AP、命令、交易、单字输入 |

通过阈值为样例数不少于 20 且准确率不少于 90%。测试源位于
`WhiteoutStation/Source/WhiteoutStation/Private/Tests/WhiteoutRulesTests.cpp`。

## Schema 与越权护栏

| 用例 | 预期 | 结果 |
|---|---|---|
| 精确三字段意图 JSON | 接受 | PASS |
| 额外 `state_changes` 字段 | `unexpected_field` | PASS |
| 模型判为承诺但原文无规则关键词 | `promise_dual_check_failed` | PASS |
| OpenAI-compatible mock 外层 envelope | 解包并校验 | PASS |
| 表达 JSON 追加 `ap_delta` | `model_attempted_rule_change` | PASS |
| 引用未授权事实 | `fact_permission_violation` | PASS |
| 台词暗示未授权事实但未声明引用 | 语义泄漏拒绝 | PASS |

## 降级链实测

| 路径 | 实测结果 |
|---|---|
| 在线模型 | 本地 mock HTTP 代理返回严格 JSON；意图映射为 `Promise / heat_repair_room`，`source=online_model`，调用计数 1 |
| 本地意图词典 | 显式禁止在线提供方后，同一句承诺同步回调为 `local_dictionary`；已加入 UE 自动化断言 |
| 纯轮盘 | 对抗输入和不可可靠识别输入返回 `bMapped=false / wheel_only`，UI 如实提示并返回 4+1 轮盘 |
| 无密钥整局 | `WhiteoutLLMEnabled=false` 下 6/6 UE、17/17 Python、三条 AutoRoute 全绿 |

mock 技术路线产生 1 条意图请求与 6 条表达请求，全部使用固定模型
`deepseek-v4-flash`；路线仍为 `TaskSuccess / 71.90`。运行态完整审计写入
`Saved/Logs/WhiteoutStation_ModelAudit.jsonl`（忽略入库），可入库脱敏样例见
`docs/model_audit_mock_sample_v0.3.jsonl`。
