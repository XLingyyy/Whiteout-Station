# Whiteout Station / 风雪站：断电前夜

Unreal Engine 5.8 C++ 社会生存与轻推理 Demo。当前版本为 v0.9。

玩家在暴风雪抵达前管理 8 点行动力，与工程师顾衡、医生叶澄调查停电、
分配物资、修复发电机和室外天线，并尝试发出求救信号。开场、探索、行动
预览、分阶段对话、自由文本、结算和四类结局已形成完整闭环。

## v0.9 重点

- HUD 警报和任务面板完成裁剪与多分辨率验证；默认准心为小型空心圆，
  瞄准可交互对象后变为居中的手形。
- 打开或关闭手册、证据板、对话、暂停和设置等面板时，鼠标回到视口中心。
- 所有人物健康、体温、精力、饱腹、稳定与信任统一使用 0—10。
- 黑幕开场扩展为 10 句手动推进文本，明确顾衡、叶澄的职责、合作价值和
  玩家目标。
- 修复叶澄眼部黑框、顾衡眼睛异常材质；保留用户替换的人物模型和地图摆放。
- 为两个当前模型的精确骨架重新生成待机、行走和五类反应动画；躯干使用
  参考姿势，双手全动画采样间距不低于 48.8 cm。
- NPC 会先平滑旋转全身正面朝向玩家，再用限制在小角度内的头部视线跟随；
  离开交互范围后平滑返回岗位朝向。
- DeepSeek 可选择受本地约束的移动意向和反应动作；C++ 继续独占 AP、资源、
  事实、承诺、任务、评分和结局。
- 墙面与地面升级为项目内生成的 v0.8 材质，用户自定义材质覆盖不被替换。

## 环境与运行

- Windows 64-bit
- Unreal Engine 5.8
- 项目：`WhiteoutStation/WhiteoutStation.uproject`
- 默认地图：`/Game/WindStation/World/MVP_StationMap`

编辑器内操作：

- `WASD` 移动，鼠标观察，`Space` 跳跃或推进开场；
- `F` 对话或预览/确认行动，`Q` 切换行动方案；
- `E` 证据板，`H` 生存手册，`Esc` 返回或暂停；
- `Enter` 结算，`C` 读取最近自动存档，`R` 开始新一轮。

构建、Shipping、验收和 AI 配置见
[`docs/BUILD_AND_PLAY_v0.9.md`](docs/BUILD_AND_PLAY_v0.9.md)；关卡对象的编辑器
拖动与替换方法见 [`docs/LEVEL_EDITING.md`](docs/LEVEL_EDITING.md)。

## DeepSeek 接入

运行配置位于
`WhiteoutStation/Content/Agents/AgentRuntime.v0.9.json`。当前使用
`deepseek-v4-flash`、官方 Chat Completions 端点、非思考模式、非流式输出和
JSON Object 响应。

仅在启动进程环境中提供密钥：

```powershell
$env:WHITEOUT_LLM_API_KEY = '<your-key>'
```

没有密钥、显式离线、网络失败、超时或非法响应时，游戏立即使用本地确定性
结果。密钥只允许发送给 `api.deepseek.com` 的 HTTPS 端点；loopback mock
不会携带 Authorization。

## 回归

```powershell
python -X utf8 -m pytest Tools/Agents -q
python -X utf8 -m pytest Tools/Rules -q
python -X utf8 -m pytest Tools/Release/test_v09_release_gates.py -q
python -X utf8 Tools/Release/validate_source_v09.py --repo-root . --final
```

UE 自动化覆盖对话边界、模型预算、AP、资源选择、阶段意向、知识权限、路线与
事务。Shipping 发布流程另执行真实键鼠输入、九条路线/降级场景、连续对话
历史，以及顾衡和叶澄的移动与反应探针。

## 目录

- `WhiteoutStation/Source/WhiteoutStation`：C++ 运行时
- `WhiteoutStation/Content/WindStation`：地图、角色、材质、UI、音频与资产
- `WhiteoutStation/Content/Rules`：版本化规则和平衡配置
- `WhiteoutStation/Content/Agents`：AI 运行配置
- `Tools/Agents`：协议、mock 和在线脱敏探针
- `Tools/Rules`：规则模拟与回归
- `Tools/Editor`：资产生成与审计
- `Tools/Capture`：视觉基线与像素审计
- `Tools/Release`：源码、Shipping 和发布门禁
- `docs`：设计、操作、进度、QA 与发布记录

`.uasset`、`.umap` 和大型媒体由 Git LFS 管理。本机密钥、构建缓存、日志与
临时文件不进入仓库。
