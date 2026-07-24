# 关卡资产手工编辑

`MVP_StationMap` 中的站体、陈设、灯光、标牌、NPC 与 13 个交互点已经保存为独立 Actor，可直接在编辑器视口或 World Outliner 中选择。

## 调整位置和样式

1. 打开 `Content/WindStation/World/MVP_StationMap`。
2. 在 World Outliner 展开 `WS Editable Station`。Actor 已按 Geometry、Presentation、Lighting、Signs、Hotspots 等目录分类。
3. 选择 Actor 后使用 W/E/R 调整位置、旋转和缩放。
4. Static Mesh Actor 可在 Details 的 Static Mesh Component 中替换 `Static Mesh`、Materials 和碰撞设置。
5. 交互点可调整 Actor Transform，并在 `Mesh`、`CharacterMesh` 等组件中替换静态或骨骼网格。保留 `Interaction > Action Id` 可继续绑定原有玩法。
6. 手工拖入的新 Actor 会正常随关卡保存，不需要额外标签。

## Builder

World Outliner 中的 `WS Station Layout Builder` 提供两个 Details 按钮：

- `Reset Editable Layout (Replaces Edits)`：重新生成默认布局，会覆盖所有已生成 Actor 上的手工调整。
- `Clear Editable Layout`：只删除 Builder 生成的 Actor，保留手工添加的内容。

Play In Editor 会直接使用关卡中保存的 Actor，不再重复动态生成站体。危机灯光、发电机灯光、结局表现、NPC 和交互逻辑会继续注册到现有 Actor。
