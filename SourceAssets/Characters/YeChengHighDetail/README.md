# 叶澄高精度重制

本轮根据用户三视图重新制作并接入 UE。以《鸣潮》的动漫角色表现作为方向参考，没有使用该游戏的模型或贴图。“60%商业化”没有统一量化标准；本目录提供实际模型、贴图和游戏截图供判断，不以面数代替视觉质量验收。

## 交付文件

- `YeCheng_HighDetail_work.blend`：可编辑的 A Pose 制作源文件，服装、脸部曲面、分层发束、后发壳体、五指手套和毛领分别保留。
- `YeCheng_HighDetail.blend`：T Pose 游戏绑定工程，53 根骨骼、6 组材质表面。
- `YeCheng_HighDetail.fbx`：UE 导入用合并骨骼网格。
- `YC_HD_Reference_BaseColor.png`：4096 × 4096 服装贴图；脸部及头发为独立 2048 × 2048 贴图。
- `build_report.json`：最终导出几何统计。`animation_validation.json` 记录七组动画的抽样检查；`install_validation.json` 记录替换目标及关卡摆放检查。
- `Validation/`：UE 实际运行的正面、侧面、近景和动作截图；`bound_*.png` 为 Blender 绑定检查图。

## 制作方法与来源

`YeCheng_ClothingSculpt.glb` 是使用用户三视图通过 Tencent Hunyuan3D-2mv 本地生成的原始雕刻底稿，613,686 顶点、1,227,680 三角面。游戏模型采用减面后的服装表面，移除生成的头部、手部和后发，重新建立脸部曲面、分层头发及五指手套。衣服使用用户前后参考投射和生成侧面纹理混合后烘焙。脸部使用连续圆柱 UV，避免小 UV 岛造成的近景接缝。

`FacePaint.png` 使用内置 imagegen，根据叶澄正面参考生成脸部绘制素材，再烘焙到三维脸部。提示要求：保留参考角色的蓝灰瞳、眼型、细眉、闭口表情与纤细下颌；正交正面、均匀照明、清晰动漫游戏脸部；移除遮挡皮肤的头发，显示完整头皮和短颈，无文字和衣服。

方法参考：[Hunyuan3D-2 官方代码](https://github.com/Tencent-Hunyuan/Hunyuan3D-2)、[技术论文](https://arxiv.org/abs/2501.12202)、[Epic 对《鸣潮》开发团队的访谈](https://www.unrealengine.com/developer-interviews/exploring-the-post-apocalyptic-charm-of-asg-open-worlds-in-wuthering-waves)。

## 重建与接入

1. `Tools/Blender/generate_yecheng_multiview.py`：三视图形体生成，模型 `tencent/Hunyuan3D-2mv`，seed 8426，50 步，512 体素分辨率。
2. `Tools/Blender/texture_yecheng_multiview.py`：生成衣服侧面纹理。
3. `Tools/Blender/refine_yecheng_highdetail.py`：从本目录的 GLB、参考图和 FacePaint 重建细化工程。
4. `Tools/Blender/export_yecheng_highdetail.py`：烘焙游戏 UV、建立骨骼权重、导出 FBX。
5. UE 编辑器执行 `Tools/Editor/install_yecheng_highdetail.py`：导入网格、建立七组动画及材质、替换叶澄数据资产和关卡组件。

Blender 5.2 LTS。生成环境位于忽略目录 `Artifacts/YeChengHighDetail`，Python 3.13、PyTorch 2.9/CUDA 12.8、Hunyuan3D-2 官方源码，权重及虚拟环境不提交。生成环境包含适配 Windows/CUDA 和 FP16 权重加载的本地修改；从保留的 GLB 开始执行步骤 3–5 不依赖该生成环境。

目标网格：`/Game/WindStation/Art/AnimeNPC/YeChengHighDetail/SK_YeCheng_HighDetail`。替换同时更新关卡中的叶澄组件和 `DA_WS_YeCheng_AnimeNPC`。C++ 动画解析按新网格路径选用新骨骼动画。

## 当前边界

已提供 Idle、Walk、Acknowledge、Consider、Reassure、Reject、Alarmed 七组身体动作。尚无面部表情形变、口型同步、独立眼球动画、头发/衣服物理及 LOD；手套五指有独立几何，但当前整体随手骨运动。服装生成表面仍有局部褶皱噪声，部分细节依赖颜色贴图。当前资产适合本工程个人使用和继续精修，不能据此声称已达到《鸣潮》成品角色的制作标准。运行验证覆盖编辑器构建和游戏截图，未执行新的完整发行打包。
