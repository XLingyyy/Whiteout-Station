# v0.4 QA / 验收记录

更新日期：2026-07-22

## 最终状态

v0.4 的输入导航、证据板、准心、高亮、毛玻璃 UI、HUD、设置、服装、肖像移除、NPC 开场白、意图输入流程、表达管线、回归、基线与 Shipping 归档均已完成。用户已接受 V4-10 的源 Idle 动画姿势限制，G0—G5 全部门禁通过，v0.4 Demo 完成发布。

## 自动化与构建

| 层级 | 结果 | 覆盖 |
|---|---:|---|
| Python 规则 | 17 / 17 | AP/危机、事务、三路线、结局、评分、承诺、知识/Agent 边界 |
| UE Automation | 6 / 6 | DialogueBoundary、ModelBudget、APFlow、KnowledgeAndAgentValidation、Routes、Transactions |
| 规则冻结 | 5 / 5 | 权威 JSON 与四个规则工具文件未改 |
| Editor Development | PASS | C++ / UHT / 运行时表现层 |
| Win64 Shipping BuildCookRun | PASS | Build、Cook、Stage、Pak/IoStore、Archive |
| 基线校验 | 140 / 140 | 70 个视角，双分辨率，9 张反馈后对照 |
| 凭据扫描 | PASS | 当前索引与可推送 Git 历史 |

最终 Automation 报告为 6 succeeded、0 warnings、0 failed、0 not run。

## Shipping 无密钥路线

| 路线 | 结局 | 事件 | AP | 分数 | 模型调用 |
|---|---|---:|---:|---:|---:|
| medical | TaskSuccess | 8 | 0 | 76.64 | 0 |
| technical | TaskSuccess | 8 | 0 | 71.90 | 0 |
| quick | TaskSuccess | 6 | 2 | 72.06 | 0 |

每次运行都先清除旧 QA 输出，再校验新文件时间戳；三张 RuntimeSmoke 均为 1280×720。规则分数、危机次数、AP 与 v0.3 一致。

## 输入、视觉与对话

- 12 条 ESC/E 路径全部判定 PASS；其中 8 条由最终 Shipping 包自动输入实测，4 条由 Dev 实机基线和同一状态机代码审计补证，来源见 `GATE_G0_v0.4.md`。
- 证据板五类过滤与详情、全屏 HUD 隐藏、固定十字准心和细白描边均通过。
- 最终基线分类：UI 42、角色 12、场景 10、LookAt 3、灯光 3。
- 对话完成 NPC 开场白 → 四意图 → 可选 280 字输入 → 发送 → 回应；无模型、mock 接受、端点失联和对抗拦截均通过。
- `player_said` 进入表达请求审计，但规则 act、AP、状态、资源和结算不由模型决定。

## 1080p 性能

测试场景为 Shipping 室外暴雪视角，1920×1080，预热 5 秒后采样 15 秒。

| 采样帧 | 平均 FPS | 1% Low | P95 帧时 | P99 帧时 | 最大帧时 |
|---:|---:|---:|---:|---:|---:|
| 1641 | 109.34 | 98.96 | 9.592 ms | 9.854 ms | 12.057 ms |

相对 v0.3 的 101.40 / 84.42 FPS 无回退，60 FPS 门槛通过。

## 已知限制

顾衡与叶澄当前均只播放指定 Idle，但这两套源动画本身带有前臂前伸、手掌上翻和手指张开姿势。服装、贴地、肖像移除与动画调用固化已完成；按任务清单未擅自换骨架/动画。用户已于 2026-07-22 接受该姿势为 v0.4 已知限制，不构成发布阻塞。除此之外未发现已知发布问题。
