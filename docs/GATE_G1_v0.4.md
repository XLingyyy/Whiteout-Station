# GATE G1 v0.4 — 准心与交互提示

日期：2026-07-22
结论：**PASS**

## 验收结果

- 游戏态准心固定为细白十字，不再根据目标切换菱形或方框。
- 无目标、远距、遮挡、中距、近距和 NPC 聚焦均有双分辨率基线。
- 交互信息只保留底部墨刷式提示；没有黑/红方框包围目标。
- 可交互物只使用细白描边，不再整物体发白或叠彩色 Overlay。
- 治疗台、无线电与 NPC 聚焦都保持可读，NPC 聚焦显示 `[F] 开始对话`。

## 证据

- `docs/baseline_v0.4/UI_focus_far_*`
- `docs/baseline_v0.4/UI_focus_blocked_*`
- `docs/baseline_v0.4/UI_focus_mid_*`
- `docs/baseline_v0.4/UI_focus_near_*`
- `docs/baseline_v0.4/UI_focus_npc_*`
- `docs/reference_v0.3/feedback_after/After_05_FocusObject.png`
- `docs/reference_v0.3/feedback_after/After_06_FocusNPC.png`

结论：V4-04、V4-05 均通过，G1 关闭。
