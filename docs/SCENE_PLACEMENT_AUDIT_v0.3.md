# v0.3 全场景陈设审查

审查日期：2026-07-22

## 结论

五区陈设审查通过。运行态审计共检查 65 个装配网格，碰撞声明与实际碰撞 65/65 匹配；41 个要求落地的道具 41/41 通过；3 条角色胶囊主通道全部可通行；13 个交互热点全部可达。审计原始结果为 `WhiteoutStation/Saved/Automation/v03-g3-scene-audit.json`。

## 反馈 10 图逐条勾销

| 组 | 反馈图（修复前） | 修复项 | 修复后基线 |
|---|---|---|---|
| 01 | `reference_v0.3/feedback/Scene_01.png` | 控制台与座椅按包围盒贴地，去除悬空 | `baseline_v0.3/UI_scene_01_after_control_grounding_*` |
| 02 | `reference_v0.3/feedback/Scene_02.png` | 控制室机柜、货架与椅子重排，保留操作距离 | `baseline_v0.3/UI_scene_02_after_control_storage_*` |
| 03 | `reference_v0.3/feedback/Scene_03.png` | 中央通道移除书架/箱体阻塞，保持胶囊宽度 | `baseline_v0.3/UI_scene_03_after_central_passage_*` |
| 04 | `reference_v0.3/feedback/Scene_04.png` | 室内楼梯改为涂装金属；厨房桶、柜与床重新落地 | `baseline_v0.3/UI_scene_04_after_quarters_grounding_*` |
| 05 | `reference_v0.3/feedback/Scene_05.png` | 维修间油罐、油桶、箱体与管架解除穿插 | `baseline_v0.3/UI_scene_05_after_repair_passage_*` |
| 06 | `reference_v0.3/feedback/Scene_06.png` | 顾衡默认姿态改为 Idle；维修区设备贴地 | `baseline_v0.3/UI_scene_06_after_gu_idle_*` |
| 07 | `reference_v0.3/feedback/Scene_07.png` | 发电机周边柜体、箱体与长凳重新落地并让出动线 | `baseline_v0.3/UI_scene_07_after_repair_grounding_*` |
| 08 | `reference_v0.3/feedback/Scene_08.png` | 医疗箱/床柜改用合理网格并移出病床，墙柜与床贴地 | `baseline_v0.3/UI_scene_08_after_medical_layout_*` |
| 09 | `reference_v0.3/feedback/Scene_09.png` | 叶澄默认姿态、眼材质及悬空设备修复 | `baseline_v0.3/UI_scene_09_after_ye_idle_*` |
| 10 | `reference_v0.3/feedback/Scene_10.png` | 床头脱离墙体，书架移出门口，双层床间保留通道 | `baseline_v0.3/UI_scene_10_after_bed_passage_*` |

每项修复后均有 1280×720 与 1920×1080 两个分辨率版本；`*` 对应文件名中的分辨率后缀。

## 五区审查表

| 区域 | 贴地/穿插 | 碰撞 | 通行 | 用途与朝向 | 结果 |
|---|---|---|---|---|---|
| 控制室 | 控制台、椅子、货架按底面贴地 | 操作台/货架阻挡，装饰灯不阻挡 | 门口与控制台前留出胶囊宽度 | 座椅朝向控制台，存储沿墙 | PASS |
| 维修间 | 发电机、油罐、箱体、长凳解除穿插 | 大型设备阻挡，墙挂件不阻挡 | 发电机两侧及入口畅通 | 工具/备件围绕维修工位 | PASS |
| 医务室 | 病床、床柜、药柜与医疗设备贴地 | 病床/柜体阻挡，墙灯不阻挡 | 病床侧与门口畅通 | 医疗箱移至床旁，柜体沿墙 | PASS |
| 厨房宿舍 | 双层床、桌凳、桶柜落地，床头不穿墙 | 床柜/桌阻挡，室内楼梯踏步碰撞保持 | 两组床之间与楼梯口畅通 | 生活区和储物区分离 | PASS |
| 室外天线 | 平台、梯架与设备相互脱离 | 装饰平台/梯架按设计不阻断自动路线 | 主检修走廊保持连续 | 检修灯与天线设备沿工作面布置 | PASS |

## 路线回归

运行态无模型 AutoRoute 与 v0.2/G2 基线一致：

| 路线 | 结局 | 分数 |
|---|---|---:|
| `medical` | TaskSuccess | 76.64 |
| `technical` | TaskSuccess | 71.90 |
| `quick` | TaskSuccess | 72.06 |

本轮仅修改表现与场景装配，未修改规则配置、状态实现或路线脚本。
