# GATE G3 v0.4 — NPC 站桩与服装

日期：2026-07-22
结论：**BLOCKED — 需要用户决定是否进入 DCC 动画修正**

## 已完成

- `ApplyCharacterState`、`HandleCharacterActionCommitted`、`SetCharacterPreviewMood` 已统一只播 `IdleAnimation`。
- Guarded / Gesture / Work 资产与成员保留但不再调用，LookAt 保持现状。
- 叶澄救援夹克源贴图已去除胸前、手臂和背部徽章/文字，并重新导入 UE；许可记录已补充。
- HUD 右栏两张 NPC 卡和对话 NPC 卡的肖像已移除。
- 脚底 Z 保持当前已贴地设置，未修改 XY、骨架或动画资产。

## 门禁触发

Idle 源动画本身仍将两只前臂举在躯干前方，手掌上翻且手指明显张开。此现象在正面、侧面和手臂特写中一致，不是信任分支、角色状态或贴图导致。

结论：**Idle 源动画需 DCC 修正**。按 V4-10 第 4 步，此处停止自救，不擅自更换骨架、不切 MetaHuman、不用其他动画掩盖。

## 证据

- 顾衡：`UI_character_gu_front_1280x720.png`、`UI_character_gu_side_1280x720.png`、`UI_character_gu_feet_1280x720.png`
- 叶澄：`UI_character_ye_front_1280x720.png`、`UI_character_ye_side_1280x720.png`、`UI_character_ye_feet_1280x720.png`
- 夹克：`UI_character_ye_front_1280x720.png`、`UI_character_ye_back_1280x720.png`、`UI_character_ye_arm_1280x720.png`
- 路径：`docs/baseline_v0.4/`

待用户决定：授权 DCC/动画修正，或接受当前 Idle 作为 v0.4 已知限制。
