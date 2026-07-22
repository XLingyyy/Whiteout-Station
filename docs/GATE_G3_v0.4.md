# GATE G3 v0.4 — NPC 站桩与服装

日期：2026-07-22
结论：**PASS — 用户已接受 Idle 源动画姿势为 v0.4 已知限制**

## 已完成

- `ApplyCharacterState`、`HandleCharacterActionCommitted`、`SetCharacterPreviewMood` 已统一只播 `IdleAnimation`。
- Guarded / Gesture / Work 资产与成员保留但不再调用，LookAt 保持现状。
- 叶澄救援夹克源贴图已去除胸前、手臂和背部徽章/文字，并重新导入 UE；许可记录已补充。
- HUD 右栏两张 NPC 卡和对话 NPC 卡的肖像已移除。
- 脚底 Z 保持当前已贴地设置，未修改 XY、骨架或动画资产。

## 已知限制与签字

Idle 源动画本身仍将两只前臂举在躯干前方，手掌上翻且手指明显张开。此现象在正面、侧面和手臂特写中一致，不是信任分支、角色状态或贴图导致。

技术结论仍为：**若要实现双臂完全自然下垂，Idle 源动画需 DCC 修正**。按 V4-10 第 4 步停止自救，未擅自更换骨架、切换 MetaHuman 或使用其他动画掩盖。

2026-07-22，用户明确选择“接受限制”。因此当前姿势登记为 v0.4 已知限制，不再阻塞 G3 与 v0.4 发布；未来美术里程碑可单独安排 DCC 修正。

## 证据

- 顾衡：`UI_character_gu_front_1280x720.png`、`UI_character_gu_side_1280x720.png`、`UI_character_gu_feet_1280x720.png`
- 叶澄：`UI_character_ye_front_1280x720.png`、`UI_character_ye_side_1280x720.png`、`UI_character_ye_feet_1280x720.png`
- 夹克：`UI_character_ye_front_1280x720.png`、`UI_character_ye_back_1280x720.png`、`UI_character_ye_arm_1280x720.png`
- 路径：`docs/baseline_v0.4/`

验收签字：V4-10 以“动画调用固化完成 + 源资产限制已接受”通过；V4-11、V4-12 完整通过。G3 关闭。
