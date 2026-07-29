# Whiteout Station v0.8 - Playtest 记录

日期：2026-07-29

构建：`20260728T194955Z-6f131d00-release`

## 测试目标

确认玩家能够在 Shipping 包中理解危机背景、顾衡和叶澄的职责及合作原因，
完成探索、状态查看、行动交互、阶段意向、中文自由输入、AI 表演和结算闭环。

## 基础流程

1. 启动 `Windows/WhiteoutStation.exe`。
2. 使用 Space 或鼠标左键逐句推进十句黑幕开场。
3. 检查左侧警报、首要目标、右侧队伍状态和屏幕中央准心。
4. 使用 `H` 查看生存手册，理解全部 0—10 人物读数及改善方式。
5. 使用 `E` 查看证据板。
6. 瞄准行动点，确认空心圆切换为手形；使用 `F` 预览、`Q` 切换方案，
   再次使用 `F` 确认。
7. 与顾衡或叶澄对话，先选择当前阶段开放的意向，再输入自由文本。
8. 观察 NPC 回复、受约束的移动和对应反应。
9. 使用 Enter 进入结算并查看结局动画和结果页。

## 自动真实输入记录

### survival_controls

- 输入：鼠标左键、Space、H、Esc、E、W、F、Q、F、Enter、Enter；
- 已执行行动：`distribute_food`；
- 结局：`SurvivalWait`；
- 得分：43.64；
- 剩余 AP：7；
- 模型调用：0；
- 结果：PASS。

该路径验证十句开场、手册、证据板、移动、目标聚焦、行动预览、方案切换、
确认、结算和结果页。手册打开/关闭、证据板打开/关闭共四次鼠标回中检查，
水平和垂直误差均为 0 px。

### dialogue_free_text

- 输入：鼠标左键、Space、F、Tab、Enter、Unicode 中文文本、Enter、Esc、
  Enter、Enter；
- 已执行行动：`talk_gu_heng`；
- 结局：`SurvivalWait`；
- 得分：46.92；
- 剩余 AP：7；
- 模型调用：0；
- 结果：PASS。

该路径验证开局只显示“询问”和“安抚”两个合理意向、中文自由文本输入、
NPC 回复、键盘焦点、退出对话和结算。对话打开/关闭两次鼠标回中检查的
水平和垂直误差均为 0 px。

## 路线与 AI 覆盖

Shipping 验收覆盖：

- `TaskSuccess`：医疗路线、技术路线；
- `CostUncontrolled`：快速路线、代价路线；
- `SurvivalWait`：等待路线；
- `TotalCollapse`：崩溃路线；
- 缺少密钥、显式离线、loopback 在线和端点不可达；
- 同一 NPC 的两轮连续对话历史；
- 顾衡和叶澄各自的移动、行走阶段和反应阶段。

在线、离线和网络故障均保持本地权威结果一致。

## NPC 表演与渲染

Loopback 模型分别让顾衡、叶澄执行 `step_closer` 和 `acknowledge`。两名 NPC
都由本地逻辑移动 85 cm，随后播放匹配当前精确骨架的回应动画。

发布截图复核结果：

- 叶澄眼部没有黑色遮罩；
- 顾衡双眼颜色正常；
- 行走和回应阶段没有手臂穿体；
- 身体、面部、头发、手指和衣物没有新增骨架扭曲。

证据：

- `Validation/ShippingSmoke/loopback_performance_guheng_Walk.png`
- `Validation/ShippingSmoke/loopback_performance_guheng_Acknowledge.png`
- `Validation/ShippingSmoke/loopback_performance_yecheng_Walk.png`
- `Validation/ShippingSmoke/loopback_performance_yecheng_Acknowledge.png`

## 视觉检查

- “暴风雪逼近｜电力正在衰减”完整收纳在顶部面板内；
- 左侧目标和建议文本未发现越界；
- 空心圆和手形准心位于视口中心；
- 交互文字与墨刷背景对齐；
- 新墙面和地面材质已出现在实际 Shipping 场景；
- 十句开场只能由玩家输入推进，完成后黑幕正常淡出；
- 顾衡对话开局只开放符合当前阶段的两个意向；
- 生存手册和人物详情均以 0—10 展示状态。

## 尚待真人样本验证

- 首次游玩者对十句开场信息量和节奏的评价；
- 玩家在没有外部文档时能否独立理解 AP、资源和状态变化；
- 不同输入法、键盘布局、Windows 显示缩放和超宽屏体验；
- 长时间在线对话的自然度、重复率和 DeepSeek 服务延迟。
