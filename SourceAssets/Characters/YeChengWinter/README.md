# 叶澄冬装模型

制作日期：2026-09-08。参考图位于仓库 `yecheng/`。

## 文件

- `YeCheng_Winter.blend`：可编辑 Blender 5.2 文件，纹理已打包。
- `YeCheng_Winter.vrm`：带骨架、面部形态键的交换文件。
- `front.png`、`side.png`、`back.png`：Blender 正交预览。
- `ue_validation.json`：UE 运行实例检查和七组动画的采样结果。
- `build_report.json`：Blender 网格统计。
- `Validation/`：独立游戏进程九张角色截图、场景预览及运行报告。

模型在原有 V10 面部、手部、头发和骨架基础上制作，新增长款防寒外套、内衬、围巾、裤装、靴子、手套、毛领、背带、医疗包及证件。160 根骨骼和 57 个面部形态键保留，约 4.86 万三角形。外观采用简化建模；未达到参考插画的发束、褶皱和配件精细度。外套使用蒙皮，未制作独立布料模拟。

源模型来自本机 `G:/MyWorkSpace/AnimeNPCKit/Models/Custom/YeCheng_NoanoaHair_RefinedFace_v10.blend`；基础资产来源记录保留在该套件的 `LICENSES_AND_SOURCES.md`。本次没有将既有模型声明为原创或 CC0。

## UE 集成

新网格：`/Game/WindStation/Art/AnimeNPC/YeChengWinter/SK_YeCheng_Winter`。

叶澄数据资产及 `MVP_StationMap` 中的已放置角色组件已指向新网格。旧 NPC 网格、材质和动画不覆盖；新骨架将 V10 骨架列为兼容骨架。场景 Actor 变换保持原值。

材质使用 19 个有效槽。Blender 构建脚本删除旧服装的空槽，避免 VRM4U/Assimp 压缩空材质后导致索引错位。UE 面部、头发复用原 V10 材质，新服装采用独立 PBR 材质；VRM 直接导入后仍需执行材质配置脚本。

新材质已显式保存 Skeletal Mesh 和 Morph Targets 使用标记，避免仅在编辑器中自动补标记、独立游戏进程缺少着色器排列的问题。已通过现有 `v10characters` 独立进程截图流程，九张叶澄截图生成完整，缺失材质使用标记警告为零。截图测试台的照明较暗，场景效果见 `Validation/station_preview.png`。本次未重新制作 Shipping 安装包。

重新制作时，用 Blender 打开 V10 源文件并执行 `Tools/Blender/build_yecheng_winter.py`。首次导入依次在 UE Python 中执行：

1. `Tools/Editor/import_yecheng_winter.py`
2. `Tools/Editor/install_yecheng_winter.py`
3. `Tools/Editor/fix_yecheng_winter_materials.py`

导入脚本会拒绝覆盖已存在的目标目录。日后重新导入时，应只操作本版本目录，保留用户的其他角色资产。

启动 PIE 后执行 `Tools/Editor/validate_yecheng_winter.py`，检查运行网格及 Idle、Walk、Acknowledge、Consider、Reassure、Reject、Alarmed 动画。该脚本最后会把 PIE 内的角色切到 Reassure 动作供观察；停止 PIE 后恢复正常。
