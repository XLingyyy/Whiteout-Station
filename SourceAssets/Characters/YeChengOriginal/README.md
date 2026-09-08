# 叶澄：从空白场景重建

2026-09-08，参考仓库 `yecheng` 三视图。所有人体、五官、头发、衣物、配件网格及材质、UV、骨架、权重均由 `Tools/Blender/build_yecheng_original.py` 新建。构建过程不加载 V10 或上一版冬装模型。

- `YeCheng_Original.blend`：保留分件和完整人体的可编辑源文件。
- `YeCheng_Original.fbx`：合并蒙皮网格的游戏交换文件，覆盖在服装内的人体不导出。
- `front.png`、`side.png`、`back.png`、`face.png`：Blender 渲染检查。
- `materials.json`：14 种独立材质的颜色、粗糙度和金属度。
- `animation_validation.json`：七种表演动作在新骨架上的五时刻采样。
- `install_validation.json`：地图安装与场景变换检查。

骨架包含 53 根新骨骼，其中手指 30 根。FBX 导入额外包含一个对象根节点。动画复用项目既有的表演曲线逻辑，在新参考骨架上重新生成；没有复用旧模型的顶点、纹理、权重或骨架数据。新模型约 26.2 万三角形。

当前模型为简化的三维重建，使用几何五官、分片发束和纯色 PBR 材质。已展开 UV；尚未绘制插画式贴图，未制作面部表情形态键、布料模拟及发丝物理。与参考图的细节精度仍有差距。

UE 资源位于 `/Game/WindStation/Art/AnimeNPC/YeChengOriginal`。叶澄数据资产和地图实例指向新网格；组件采用项目的 +Y 正面朝向约定，Actor 摆场变换保持原值。代码根据当前网格选择对应动画集。

编辑器中依次执行 `import_yecheng_original.py`、`animate_yecheng_original.py`、`install_yecheng_original.py`。导入脚本仅覆盖本版本目录。旧角色模型与材质保留。

## 验证

Editor Development 编译成功。独立 `-game` 进程运行 `v10characters` 截图流程，9 张叶澄截图位于 `Validation`。检查了正面朝向、待机手臂、行走及安抚动作，修正首次检查中发现的反向朝向和外套后摆权重问题。材质使用标记缺失警告和动画集缺失错误均为零。

游戏截图测试台照明较暗，Blender 预览用于查看模型的基础配色。此次未生成 Shipping 安装包。

发布前复核了 361 个受保护资产，原有 NPC 模型和材质均保持不变；现有资产改动仅包含目标地图与叶澄数据资产。地图内 104 个 Actor 的摆场变换保持原值。
