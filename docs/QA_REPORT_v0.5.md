# v0.5 QA / 发布验收记录

更新日期：2026-07-25

## 结论

v0.5 的 AI 表达边界、对话与资源选择、规则一致性、端点安全、重试、离线降级和最终 Shipping 包均已通过验收。发布包绑定提交 `9bd94fab63f446290fbb5ababf809529a91c1b7c`，源码门禁与发布门禁均为 PASS。

## 构建与自动化

| 层级 | 结果 | 覆盖 |
|---|---:|---|
| UE 5.8 Editor Development | PASS | C++、UHT、运行时模块 |
| UE Automation | 7 / 7 | Agent 边界、预算、AP、对话/资源选择、知识校验、路线、事务 |
| Python Agents | 39 passed | 四字段合同、provider envelope、错误分类、重试、mock、探针脱敏 |
| Python Rules | 28 passed | 意图差异、承诺、资源组合、天线安全温度配置边界、连续评级、路线与随机不变量 |
| Python Release | 14 passed | v0.5 源码门禁、版本资源、保护范围、凭据规则与包内配置绑定 |

最终 UE 报告：`Artifacts/TestResults/v05-final-20260725-212000/index.json`

- succeeded：7
- succeededWithWarnings：0
- failed：0
- notRun：0

通过的 UE 测试：

1. `WhiteoutStation.Agents.DialogueBoundary`
2. `WhiteoutStation.Agents.ModelBudget`
3. `WhiteoutStation.Rules.APFlow`
4. `WhiteoutStation.Rules.DialogueAndResourceChoices`
5. `WhiteoutStation.Rules.KnowledgeAndAgentValidation`
6. `WhiteoutStation.Rules.Routes`
7. `WhiteoutStation.Rules.Transactions`

## AI 集成矩阵

| 场景 | 结果 | 关键断言 |
|---|---|---|
| 本地 mock / 顾衡 | PASS | HTTP 200，`validation=ok`，请求合同完整 |
| 本地 mock / 叶澄 | PASS | HTTP 200，`validation=ok`，请求合同完整 |
| loopback 凭据隔离 | PASS | `authorization_present=false` |
| mock 持续 429 | PASS | 共 2 次请求；最终 `provider_rate_limited` 并返回本地台词 |
| mock 429 → 200 | PASS | 共 2 次请求；恰好一次重试后 `fallback=false`、`validation=ok` |
| 无 Key | PASS | `provider=preset`、`fallback=true`、`validation=deterministic_decision` |
| 官方 DeepSeek 探针 | PASS | HTTP 成功，finish 与四字段 schema 通过，`secret_present=false` |
| 官方 UE / 顾衡 | PASS | `provider=deepseek`、`fallback=false`、`validation=ok` |
| 官方 UE / 叶澄 | PASS | `provider=deepseek`、`fallback=false`、`validation=ok` |

本地集成证据：

- `Artifacts/Integration/v05-mock-20260725-204058`
- `Artifacts/Integration/v05-mock-429`
- `Artifacts/Integration/v05-mock-retry-success`
- `Artifacts/Integration/v05-no-key`
- `Artifacts/Integration/v05-live-final-20260725-210313`

mock 审计同时确认 `thinking_disabled=true`、`stream_disabled=true`、`response_format_json_object=true`，顾衡与叶澄请求都没有向 loopback 发送 Authorization。

## Shipping 验收

最终包：`Builds/WhiteoutStation-v0.5-Win64-20260725T134938Z-9bd94fab-release`

| 场景 | 结果 | 关键断言 |
|---|---:|---|
| 默认离线 / medical | PASS | TaskSuccess，76.76，0 AP，0 模型调用 |
| 默认离线 / technical | PASS | TaskSuccess，72.02，0 AP，0 模型调用 |
| 默认离线 / quick | PASS | TaskSuccess，72.06，2 AP，0 模型调用 |
| 启用但无 Key / medical | PASS | TaskSuccess，本地确定性表达，0 模型调用 |
| loopback 断连 / technical | PASS | HTTP 502 安全降级，模型调用 1，总尝试上限 2 |

五个场景均写出 1280×720 截图和事件日志。脱敏汇总位于 `Validation/ShippingSmoke/shipping_smoke_summary.json`。

首次并行 Shipping 链接因系统提交内存达到 UBA 阈值而失败；失败归档未进入发布。关闭 UBA 并限制两路并行后，Build、Cook 694 / 694、Stage、Pak 和 Archive 全部成功。最终清单含 45 个文件 SHA-256，发布校验确认版本文件、提交、源树、时间、新鲜度和包内容一致。

## 表达与规则边界

- 在线模型只接收 C++ 投影出的只读上下文，包括 speaker、action、既定 response type / emotion / preset、显式 dialogue act、承诺条件、玩家补充说法、允许事实和只读剩余 AP。
- 模型只可返回 `npc_line`、`emotion`、`used_action_id`、`referenced_fact_ids`。额外字段、字段缺失、动作不匹配、非法事实、空正文或超限正文均拒绝。
- 模型接受或回退前后，规则状态只由已经提交的 C++ 事务产生变化；网络重试不重复扣 AP，也不增加规则层模型调用预算。
- 正常游戏直接使用玩家选择的 Ask / Challenge / Reassure / Promise，不让模型重解释规则意图。
- 整局模型调用硬上限为 10；非对话行动不发送表达请求。

## 玩法回归

| 路线 | 结局 | 分数 | 评级 | 剩余 AP |
|---|---|---:|---:|---:|
| medical | TaskSuccess | 76.76 | B | 0 |
| technical | TaskSuccess | 72.02 | B | 0 |
| quick | TaskSuccess | 72.06 | B | 2 |

额外覆盖：

- 六种非空且总量不超过两份的口粮分配方案均可预览并按所选参数提交。
- 治疗可在药品与已披露的保温包之间切换；不可用资源由规则层拒绝。
- 叶澄承诺、非法承诺条件和重复承诺均不提交事务、不扣 AP。
- 质疑、安抚、承诺的信任 / 压力差异与 JSON、Python、C++ 一致。
- 59.x、69.x、79.x、89.x 等连续分数边界按降序下限正确评级。
- 提前失败结算要求同一状态下连续两次 Enter，期间发生行动会使确认失效。

## 凭据与范围检查

- 官方 API 探针和两次 UE 在线表达的输出均报告 `secret_present=false`；证据不包含 API Key。
- 模型审计记录元数据，不记录 Authorization、完整请求、完整回复或玩家原文，并在 2 MiB 处轮转。
- 角色保护树基线记录于 `docs/PROTECTED_CHARACTER_ASSETS_v0.5.json`；顾衡与叶澄的人物模型、骨骼、材质、动作、动画、AnimBP 与 LookAt 表现没有变更。
- 用户的 `WhiteoutStation/Content/WindStation/World/MVP_StationMap.umap` 工作树改动来自本轮实现开始前；本轮未编辑、覆盖、暂存或清理该地图。
