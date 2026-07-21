# v0.3 角色资产升级评估

评估日期：2026-07-22

## 结论

G3 保留现有 MakeHuman 角色，不在本门禁替换为 MetaHuman 或 MB-Lab。现资产在修复眼材质、服装材质、默认 Idle 和截图灯光后，已经满足 Demo 的中近景可读性；此时整体替换会引入骨架重定向、4 段状态动画重做、服装再适配、LookAt 验证与包体/性能回归，收益不足以覆盖 G3 风险。该结论是“本版本保持”，不阻止后续版本以专门角色美术里程碑升级。

## 当前基线

- 顾衡：6 个材质槽全部有效；工程连体服、鞋、头发、眉毛与皮肤资源齐全。
- 叶澄：8 个材质槽全部有效；救援夹克、裤装、鞋、头发、眉睫与皮肤资源齐全。
- 两人均具备 Idle / Gesture / Guarded / Work 四段动画，运行时仍按信任和任务状态驱动。
- 共用 `M_WS_Eye` 眼材质，启用 SkeletalMesh usage，降低高光与亮度，消除白眼/发光感。
- MakeHuman 导出模型为 CC0；本项目叠加服装的 CC-BY 归属记录继续由 `SourceAssets/MakeHuman/README.md` 与 `SourceAssets/ASSET_LICENSES.md` 管理。MakeHuman 官方 FAQ 明确导出模型可作为 CC0 用于闭源游戏：<https://static.makehumancommunity.org/makehuman/faq/can_i_sell_models_created_with_makehuman.html>。

## 候选对比

| 方案 | 画面提升 | 许可/维护 | 工程代价 | G3 决策 |
|---|---|---|---|---|
| 现有 MakeHuman + 细分/材质/服装修复 | 中等，足以解决当前反馈 | 导出 CC0，服装许可已审计；生成脚本可复现 | 低；保留现有骨架、热点与 4 段动画 | 采用 |
| MetaHuman Optimized | 面部、皮肤与毛发显著提升 | 走 UE 官方资产管线 | 高；需额外 Creator Core Data、面体双骨架/IK 重定向、服装与 LOD 验证，包体和显存明显增加 | 延后到独立美术里程碑 |
| MB-Lab | 面部与皮肤可能提升 | 原仓库已于 2024-07-21 归档，只读；维护风险高 | 中高；Blender 中转、骨架与材质管线重建 | 不采用 |

MetaHuman 官方文档确认 Optimized/Cine 装配资产可直接用于 UE，并提供 IK Rig、IK Retargeter、LOD 与 Groom 管线：<https://dev.epicgames.com/documentation/en-us/metahuman/metahumans-in-unreal-engine>。但 Creator 需要额外 Core Data 和插件，官方硬件建议也高于本项目当前轻量角色管线：<https://dev.epicgames.com/documentation/en-us/metahuman/getting-started-with-metahuman-creator>、<https://dev.epicgames.com/documentation/en-us/metahuman/metahuman-hardware-requirements-in-unreal-engine>。MB-Lab 上游归档状态见：<https://github.com/animate1978/MB-Lab/issues>。

## 替换触发条件

仅在后续版本同时满足以下条件时重开替换：确定目标硬件与包体预算；完成两名角色定制冬季服装；4 段状态动画与 LookAt 重定向全部通过；近/中景 A/B 显示显著提升；第三方许可和来源可完整归档。

## 验收证据

- 机器审计：`WhiteoutStation/Saved/Automation/v03-g3-character-audit.json`，整体 `passed=true`。
- 近/中景：`docs/baseline_v0.3/UI_character_{gu,ye}_{near,mid}_*`。
- 场景姿态：`UI_scene_06_after_gu_idle_*` 与 `UI_scene_09_after_ye_idle_*`。
