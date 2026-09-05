# Whiteout Station / 风雪站：断电前夜

Unreal Engine 5.8 C++ 社会生存与轻推理 Demo。当前版本为 v1.4（运行时版本 `1.4.0`）。

玩家在暴风雪抵达前管理早晨、午后、黄昏三个阶段共 12 点行动力，与工程师顾衡、医生叶澄调查停电、
分配物资、修复发电机和室外天线，并尝试发出求救信号。开场、探索、行动
预览、分阶段对话、自由文本、结算和四类结局已形成完整闭环。

## v1.4 重点

- 采用认知约束型角色扮演：模型只接收当前 NPC 的档案、主观状态、过滤后的 Top-K 知识和安全记忆，未满足披露条件的隐藏知识不进入上下文。
- 每条玩家输入最多发起一次模型请求，生成完整台词、知识引用、断言、记忆摘要、表现意图和受限动作提案。
- 对话沿用 Prepare → Realize → Validate → Commit 事务；AP、资源、关系、任务、事实升级和提案执行由本地规则校验并提交。
- 一次私聊最多三轮，首轮成功提交消耗 1 AP，后两轮不额外消耗 AP；切换 NPC、结束私聊、跨阶段或读档会结束会话。
- 网络失败、超时、非法 JSON、知识越界或非法提案立即使用一次状态感知的本地回退，不重试模型请求。
- 普通人物闲聊不自动解锁诊断或维修路线；明确的医疗询问、实际调查证据及相应披露条件共同控制事实解锁。
- 供暖与设备回答依据当前状态；上下文中的长期记忆按提交顺序选取最近六条可见记录，避免跨会话轮次干扰排序。

## 基础玩法

- 每轮分为早晨、午后、黄昏，每阶段 4 AP；阶段开始时从四个房间中选择供暖区。
- 食物、休整、泛化治疗、体能、体温、伤势、压力和信任共同影响行动条件与结局。
- 左侧任务指引展示多种可选下一步，由玩家自行组合医疗、技术和风险推进路线。
- 室外天线改由可见的控制终端交互，并通过玩家视线查询同源的场景可达性审计。
- 设置页可选主流语言模型厂商、BaseURL 和模型，API Key 只保存在本次运行内存中。
- 自由文本由本地提取语义并筛选 NPC 知识；模型回复验证通过后再提交对话结果，模型不能直接修改世界状态。

## 环境与运行

- Windows 64-bit
- Unreal Engine 5.8
- Visual Studio 2022 C++ 工具链（从源码构建时需要）
- 项目：`WhiteoutStation/WhiteoutStation.uproject`
- 默认地图：`/Game/WindStation/World/MVP_StationMap`

编辑器内操作：

- `WASD` 移动，鼠标观察，`Space` 跳跃或推进开场；
- `F` 对话或预览/确认行动，`Q` 切换行动方案；
- `E` 证据板，`H` 生存手册，`Esc` 返回或暂停；
- `Enter` 结算，`C` 读取最近自动存档，`R` 开始新一轮。

构建、Shipping、验收和 AI 配置见
[`docs/BUILD_AND_PLAY_v1.4.md`](docs/BUILD_AND_PLAY_v1.4.md)；历史说明保留在
[`v1.3`](docs/BUILD_AND_PLAY_v1.3.md) 和 [`v1.2`](docs/BUILD_AND_PLAY_v1.2.md)。关卡对象的编辑器
拖动与替换方法见 [`docs/LEVEL_EDITING.md`](docs/LEVEL_EDITING.md)。

## LLM 配置与离线运行

运行配置位于
[`WhiteoutStation/Content/Agents/AgentRuntime.v1.4.json`](WhiteoutStation/Content/Agents/AgentRuntime.v1.4.json)。
当前协议为 `bounded_roleplay_v4`，schema 为 `7`，默认预设为
`deepseek-v4-flash` 与官方 Chat Completions 端点；`llm_enabled=false`，默认离线可玩。

在游戏设置页选择 provider、BaseURL 和 model，输入仅驻留本次进程内存的 API Key，
再开启 LLM。Development 联调也可通过启动进程环境启用：

```powershell
$env:WHITEOUT_LLM_API_KEY = '<your-key>'
$env:WHITEOUT_LLM_ENABLED = 'true'
```

没有密钥、显式离线、网络失败、超时或非法响应时，游戏使用本地安全回退。
密钥不得写入运行配置 JSON、源码、日志或发布包；loopback mock 不携带 Authorization。

v1.4 自动存档槽为 `WhiteoutStation_Autosave_v1_4`，支持读取并迁移 v1.3/v1.2/v1.1 存档。
未完成的对话事务、网络请求及会话轮次不会跨读档恢复。

## 回归

```powershell
python -X utf8 Tools/Dialogue/validate_roleplay_content.py
python -X utf8 -m pytest -p no:cacheprovider `
  Tools/Dialogue/test_roleplay_content.py `
  Tools/Release/test_v14_dialogue_release_gates.py -q
python -X utf8 Tools/Release/validate_source_v14.py --repo-root . --contract-only
```

最终发布时，在 `main` 已提交、已推送且发布源干净后执行
`python -X utf8 Tools/Release/validate_source_v14.py --repo-root . --final`。
`--contract-only` 不能替代完整发布门禁。UE 自动化及独立包冒烟命令见
[构建与验收文档](docs/BUILD_AND_PLAY_v1.4.md)。

2026-09-05 工程验收：Editor 与 Win64 Shipping 构建成功；Python 定向测试 45/45、
UE 全量自动化 59/59、独立 Shipping 冒烟 13/13 通过，最终源码门禁通过。
冒烟覆盖五条离线路线、三条有对话路线的在线/离线结算对比，以及五类异常响应回退。
三条对照路线的 AP、资源、事实、关系、任务、结局与评分一致。

在线链路验收使用本机 HTTP 合成响应。真实模型的对话自然度、语义改写泄露边界和
八人试玩尚未验收；自动化结果不替代这些项目。

## 目录

- `WhiteoutStation/Source/WhiteoutStation`：C++ 运行时
- `WhiteoutStation/Content/WindStation`：地图、角色、材质、UI、音频与资产
- `WhiteoutStation/Content/Rules`：版本化规则和平衡配置
- `WhiteoutStation/Content/Agents`：AI 运行配置
- `WhiteoutStation/Content/Dialogue/v1.4`：世界知识、NPC 知识、关系经历、对话策略与安全回退，共六个 JSON
- `Tools/Agents`：协议、mock 和在线脱敏探针
- `Tools/Dialogue`：v1.4 角色知识内容校验与回归
- `Tools/Rules`：规则模拟与回归
- `Tools/Editor`：资产生成与审计
- `Tools/Capture`：视觉基线与像素审计
- `Tools/Release`：源码、Shipping 和发布门禁
- `docs`：设计、操作、进度、QA 与发布记录

`.uasset`、`.umap` 和大型媒体由 Git LFS 管理。本机密钥、构建缓存、日志与
临时文件不进入仓库。
