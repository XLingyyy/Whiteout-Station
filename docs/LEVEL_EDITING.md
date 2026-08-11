# 关卡资产手工编辑

`MVP_StationMap` 中的站体、陈设、灯光、标牌、NPC 与交互点均可保存为独立 Actor，直接在编辑器视口或 World Outliner 中选择和拖动。v1.1 共需要 16 个交互点。

## 首次补齐 v1.1 交互点

1. 编译并打开 `WhiteoutStationEditor`，加载 `Content/WindStation/World/MVP_StationMap`。
2. 在 World Outliner 选择 `WS Station Layout Builder`。
3. 在 Details 的 `Level Editing` 中点击 `Sync Missing v1.1 Hotspots (Keeps Edits)`。
4. 新增的交互点会进入 `WS Editable Station/Hotspots`。该操作只补缺失项，并升级旧交互 ID/必要标签；不会移动、替换或删除已有 Actor，支持撤销。
5. 点击 `Save All`，让新增 Actor 与之后的手工 Transform 写入关卡。

## 调整位置和样式

1. 打开 `Content/WindStation/World/MVP_StationMap`。
2. 在 World Outliner 展开 `WS Editable Station`。Actor 已按 Geometry、Presentation、Lighting、Signs、Hotspots 等目录分类。
3. 选择 Actor 后使用 W/E/R 调整位置、旋转和缩放。
4. Static Mesh Actor 可在 Details 的 Static Mesh Component 中替换 `Static Mesh`、Materials 和碰撞设置。
5. 交互点可调整 Actor Transform，并在 `Mesh`、`CharacterMesh` 等组件中替换静态或骨骼网格。保留 `Interaction > Action Id`、`WSEditableStation` 与 `WSRuntimeHotspot` 标签可继续绑定原有玩法和运行时注册。
6. 手工拖入的新 Actor 会正常随关卡保存，不需要额外标签。

## Builder

World Outliner 中的 `WS Station Layout Builder` 提供三个 Details 按钮：

- `Sync Missing v1.1 Hotspots (Keeps Edits)`：只补齐缺失的 v1.1 交互点，日常更新关卡使用这个按钮。
- `Reset Editable Layout (Replaces Edits)`：重新生成默认布局，会覆盖所有已生成 Actor 上的手工调整。
- `Clear Editable Layout`：只删除 Builder 生成的 Actor，保留手工添加的内容。

Play In Editor 会直接使用关卡中保存的 Actor，不再重复动态生成站体。关卡中已经保存的天线交互点会保留编辑器 Transform；只有关卡缺失该交互点时，运行时兜底 Actor 才会自动选择位置。危机灯光、发电机灯光、结局表现、NPC 和交互逻辑会继续注册到现有 Actor。

`Tools/Editor/bake_editable_station_layout.py` 在检测到现有可编辑布局时也只执行缺失项同步。空白关卡才会走完整生成流程。
