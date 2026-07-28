# Whiteout Station v0.7 - QA 报告

日期：2026-07-28

结论：PASS（限本机评审）

## 验证对象

- 引擎：Unreal Engine 5.8.0
- 平台：Windows Win64 Shipping
- 源码提交：`0e96ad1b22c4ab894e5c5f5e7dc9f1c103f38bda`
- 源码树：`0678c323d73213f034944c16bb30c686aca7886d`
- 候选包：
  `Builds/WhiteoutStation-v0.7-Win64-20260728T150103Z-0e96ad1b-release`
- 包内门禁清单：
  `Validation/gate_manifest.json`

## 自动化结果

| 层级 | 命令或证据 | 结果 |
|---|---|---:|
| Agent 契约 | `python -X utf8 -m pytest Tools/Agents -q` | 48 passed / 3.87s |
| 规则与闭环 | `python -X utf8 -m pytest Tools/Rules -q` | 30 passed / 0.36s |
| 发布门禁 | `python -X utf8 -m pytest Tools/Release/test_v07_release_gates.py -q` | 18 passed / 60.82s |
| UE 自动化 | `Saved/Logs/v07_automation_final.log` | 8 passed |
| 真实输入烟测 | `Validation/InputSmoke/input_smoke_summary.json` | 2/2 passed |
| Shipping 烟测 | `Validation/ShippingSmoke/shipping_smoke_summary.json` | 12/12 passed |
| 源码门禁 | `Tools/Release/validate_source_v07.py` | PASS |
| 发布包门禁 | `Tools/Release/validate_release_v07.py` | PASS |
| Git 历史敏感信息扫描 | `Tools/Release/scan_secrets.py --history` | PASS |

UE 自动化覆盖 `DialogueBoundary`、`ModelBudget`、`APFlow`、
`DialogueAndResourceChoices`、`DialogueStages`、
`KnowledgeAndAgentValidation`、`Routes`、`Transactions`。

## UI 与真实输入

### 结果

- 左侧面板文本在 1280×720 和 1920×1080 视口内完整收纳；
- 空心圆与手形准心均以视口中心布局；
- 交互提示文字与墨刷背景共用同一布局容器；
- 720p、1080p 像素审计中心误差均不超过 1.5 px；
- 可使用 `Tab`、`Enter`、Unicode 文本输入和 `Esc` 完成完整对话；
- 对话回复出现时焦点转移到继续按钮，`Esc` 可可靠关闭对话。

### 证据

- `Validation/InputSmoke/survival_controls_focus.png`
- `Validation/InputSmoke/survival_controls_preview.png`
- `Validation/InputSmoke/dialogue_free_text_intent.png`
- `Validation/InputSmoke/dialogue_free_text_text_entry.png`
- `Validation/InputSmoke/dialogue_free_text_closed.png`

`survival_controls` 通过真实键鼠输入执行 `distribute_food`，最终进入
`SurvivalWait`，得分 43.64，剩余 AP 7。`dialogue_free_text` 执行
`talk_gu_heng` 并提交中文自由文本，退出对话后进入 `SurvivalWait`，得分
46.92，剩余 AP 7。

## AI 契约与降级

- 响应必须精确包含六个字段；缺字段、增字段、非法枚举和越权事实引用均被拒绝；
- 在线和离线技术路线的权威游戏结果一致；
- 缺少密钥时不发起请求，显式离线时模型调用数为 0；
- 不可达端点触发确定性降级，技术路线仍完成 `TaskSuccess`；
- 连续两轮对话请求的消息数为 `[2, 4]`，历史轮数为 `[0, 1]`；
- Loopback 记录确认请求中没有 `Authorization`；
- API key 未写入证据、日志、包或 Git 历史。

两名 NPC 的表演探针均通过：

| NPC | 模型移动 | 本地实际距离 | 反应 | 结果 |
|---|---|---:|---|---:|
| 顾衡 | `step_closer` | 85.0 cm | `acknowledge` | PASS |
| 叶澄 | `step_closer` | 85.0 cm | `acknowledge` | PASS |

对应截图位于 `Validation/ShippingSmoke/*_Walk.png` 和
`Validation/ShippingSmoke/*_Acknowledge.png`。

## 路线和结局

| 场景 | 预期/实际结局 | 模型调用 | 结果 |
|---|---|---:|---:|
| missing_key_medical | TaskSuccess | 0 | PASS |
| missing_key_technical | TaskSuccess | 0 | PASS |
| missing_key_quick | CostUncontrolled | 0 | PASS |
| missing_key_wait | SurvivalWait | 0 | PASS |
| missing_key_cost | CostUncontrolled | 0 | PASS |
| missing_key_collapse | TotalCollapse | 0 | PASS |
| explicit_offline_medical | TaskSuccess | 0 | PASS |
| loopback_online_technical | TaskSuccess | 1 | PASS |
| unreachable_endpoint_technical | TaskSuccess | 1 | PASS |

## 内容保护和安全

- 用户关卡 Git 对象保持开工基线；
- 顾衡、叶澄模型 Git 对象保持开工基线；
- 保护目录仅增加 12 个允许清单中的动画资产；
- 650 个已跟踪文件的敏感信息扫描通过；
- Git 索引和完整历史扫描通过；
- Git LFS 指针和对象完整；
- 包内规则、Agent 配置、输入证据、Shipping 证据和二进制均有 SHA-256。

## 已知限制

- QA 使用本机自动化与开发者视觉复核，尚无独立玩家样本；
- DeepSeek 服务端质量和可用性超出本项目控制范围；
- 可执行文件未签名，Windows 可能显示未知发布者提示；
- Noanoa 发型许可仍限制当前包为 `local_review_only`。
