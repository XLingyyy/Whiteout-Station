# v0.6 QA / 发布验收记录

更新日期：2026-07-26

## 结论

v0.6 的阶段化对话、手动黑幕开场、首局教学、状态解释、产品菜单、可访问性、多轮 AI 表达、路线反馈、四结局、真实输入和最终 Shipping 包均通过验收。发布包绑定源码提交 `4326f3ba268de4846f1b7889ca84858daf70984b`，发布清单校验为 PASS。

## 构建与自动化

| 层级 | 结果 | 主要覆盖 |
|---|---:|---|
| UE 5.8 Editor Development | PASS | C++、UHT、运行时模块 |
| UE Automation | 8 / 8 | Agent 边界/预算、AP、资源、对话阶段、知识、路线、事务 |
| Python Agents | 43 passed | 四字段合同、历史消息、取消、重试、探针、凭据脱敏 |
| Python Rules | 30 passed | 阶段意向、承诺条件、四结局、路线与随机不变量 |
| Python Release | 15 passed | v0.6 版本、保护对象、凭据、清单、包绑定与篡改拒绝 |
| 真实输入 Smoke | 2 / 2 | 开场、H/E/Esc、移动、聚焦、F/Q、中文输入、结算 |
| Shipping Smoke | 10 / 10 | 四结局、三成功策略、无 Key、在线、断连、多轮历史 |

最终 UE 报告：`Artifacts/TestResults/v06-checkpoint-2/index.json`

- succeeded：8
- succeededWithWarnings：0
- failed：0
- notRun：0

新增测试 `WhiteoutStation.Rules.DialogueStages` 专门覆盖 NPC/证据/压力/关系/承诺的开放规则；其余 7 项既有规则与 Agent 测试全部保持通过。

## 对话与开场验收

| 场景 | 结果 | 断言 |
|---|---:|---|
| 顾衡初始意向 | PASS | 仅询问、安抚；没有质疑或承诺 |
| 叶澄初始意向 | PASS | 仅询问；所有阶段均无承诺 |
| 顾衡阶段开放 | PASS | 证据开放质疑；交谈与具体需求开放合法承诺 |
| 非法/重复 Promise | PASS | UI 不显示，直接请求由 C++ 拒绝且不扣 AP |
| 黑幕故事 | PASS | 7 句、无自动推进、点击与 Space 各推进一个状态 |
| 最终揭幕 | PASS | 第 7 句淡出后黑幕渐隐，恢复第一人称控制 |
| 减少动态效果 | PASS | 保留逐句确认，显著缩短淡入淡出时间 |

视觉基线位于 `docs/baseline_v0.6`。已目检 1920×1080 开场第 1/4/7 句、顾衡初始/开放、叶澄初始、生存手册，以及 1280×720/1920×1080 的暂停、设置和四类结果页。

## AI 集成

| 场景 | 结果 | 关键断言 |
|---|---:|---|
| 默认离线 | PASS | 完整玩法 0 模型调用 |
| 启用但无 Key | PASS | 本地确定性表达，规则结果不变 |
| loopback 在线 | PASS | HTTP 200、四字段合同通过、Authorization 缺失 |
| 不可达端点 | PASS | 有界尝试后 502 分类并回退，本轮事务不重复 |
| 两轮同会话 | PASS | message count 2 → 4，history turns 0 → 1 |
| 官方 DeepSeek | PASS | `deepseek-v4-flash` HTTP 200，JSON 合同通过，输出未包含凭据 |

在线与离线技术路线均得到 `TaskSuccess / 72.02 / 0 AP`，确认模型表达不改变权威玩法结果。网关只保留同会话最近 4 轮；离开对话、读档、新局和对象销毁会取消活动请求并使旧回调失效。

## Shipping 路线矩阵

| 场景 | 路线 | 结局 | 分数 | 剩余 AP | 模型调用 |
|---|---|---|---:|---:|---:|
| 默认离线 | 医疗协作 | TaskSuccess | 76.76 | 0 | 0 |
| 默认离线 | 证据替代 | TaskSuccess | 72.02 | 0 | 0 |
| 默认离线 | 直接抢修 | CostUncontrolled | 68.31 | 2 | 0 |
| 默认离线 | 等待 | SurvivalWait | 47.28 | 8 | 0 |
| 默认离线 | 失控代价 | CostUncontrolled | 49.11 | 3 | 0 |
| 默认离线 | 全面崩溃 | TotalCollapse | 45.57 | 0 | 0 |
| 启用无 Key | 医疗协作 | TaskSuccess | 76.76 | 0 | 0 |
| loopback 在线 | 证据替代 | TaskSuccess | 72.02 | 0 | 1 |
| 断连回退 | 证据替代 | TaskSuccess | 72.02 | 0 | 1 |
| loopback 历史 | 两轮对话 | 合同通过 | — | — | 2 |

Shipping 汇总位于包内 `Validation/ShippingSmoke/shipping_smoke_summary.json`。每条玩法路线写出 1280×720 截图、事件日志和脱敏模型元数据。

## 真实输入

`survival_controls` 使用实际鼠标/键盘完成：点击与 Space 推进开场、H 手册、Esc 返回、E 证据板、W 移动、F 预览、Q 切换、F 提交、Enter 二次确认。事件日志只包含所提交的 `distribute_food`，结局为 `SurvivalWait`。

`dialogue_free_text` 使用实际鼠标/键盘完成：开场、F 对话、点击“询问”、输入“继电器怎么会烧毁？”，Enter 发送、点击离开、Enter 二次确认。事件日志只包含 `talk_gu_heng`，结局为 `SurvivalWait`。

两条路径共保存 26 张 1920×1080 输入证据和 2 份事件日志，汇总位于包内 `Validation/InputSmoke/input_smoke_summary.json`。

## 发布与范围

- Shipping Build/Cook/Stage/Pak/IoStore/Archive：PASS；
- 包内文件：90，清单覆盖文件：89，总大小：788,753,256 字节；
- `validate_release_v06.py`：PASS；
- 5 个受保护角色/地图/交互对象 Git OID：全部一致；
- API Key 未写入源码、日志、证据或分发包；
- 顾衡与叶澄的模型、骨骼、材质、动作、动画、AnimBP 和 LookAt 未改。
