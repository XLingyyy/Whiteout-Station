# 叶澄高精度重制（制作中）

此目录保存新一轮制作源文件。当前尚未替换游戏资产，尚未达到最终验收质量。

- `YeCheng_ClothingSculpt.glb`：使用用户三视图，通过 Tencent Hunyuan3D-2mv 本地生成的原始雕刻底稿，613,686 顶点、1,227,680 三角面。只采用其服装结构；原始头部和手部需要替换。
- `YeCheng_HighDetail_work.blend`：Blender 制作工程，含减面服装、重新建立的脸部曲面、独立发束和五指手套。
- `FacePaint.png`：内置 imagegen 根据叶澄正面参考生成的高清脸部绘制素材，用于皮肤贴图，不作为三维模型替代。
- 正面、侧脸和背面 PNG：制作检查图，包含仍需修正的材质投射及接缝问题。

生成基于用户参考图重新构建，没有使用《鸣潮》的模型或贴图。《鸣潮》仅作为精细度和风格参考。

当前待完成：身体多视角贴图、袖口连接、头发细化、游戏 UV 烘焙、骨骼和权重、UE 替换与运行验证。

## 可复现入口

- `Tools/Blender/generate_yecheng_multiview.py`：三视图形体生成。模型 `tencent/Hunyuan3D-2mv`，seed 8426，50 步，512 体素分辨率。
- `Tools/Blender/refine_yecheng_highdetail.py`：Blender 几何细化与制作预览。
- `Tools/Blender/texture_yecheng_multiview.py`：身体纹理生成。

生成环境位于忽略目录 `Artifacts/YeChengHighDetail`，使用 Python 3.13、PyTorch 2.9/CUDA 12.8、Hunyuan3D-2 官方源码。权重与虚拟环境不提交。

FacePaint 使用内置 imagegen，提示要求：保留参考角色的蓝灰瞳、眼型、细眉、闭口表情与纤细下颌，生成正交正面、均匀照明、清晰动漫游戏脸部绘制素材；移除遮挡皮肤的头发，显示完整头皮和短颈，供独立三维头发覆盖，无文字和衣服。

方案来源：https://github.com/Tencent-Hunyuan/Hunyuan3D-2
