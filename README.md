# Whiteout Station / 风雪站：断电前夜

Unreal Engine 5.8 C++ 社会生存与轻推理 Demo。当前开发版本为 v1.5（运行时版本 `1.5.0`）。

玩家在暴风雪抵达前管理早晨、午后、黄昏三个阶段共 12 点行动力，与工程师顾衡、医生叶澄调查停电、
分配物资、修复发电机和室外天线，并尝试发出求救信号。开场、探索、行动
预览、分阶段对话、自由文本、结算和四类结局已形成完整闭环。

## v1.5 重点

- 默认离线：通过固定选项和作者台词交谈，不发送模型请求。
- 在线采用两阶段协议：模型 A 解析规范意图，本地规则规划授权事实，模型 B 组织受控表达；每轮最多两次请求，总预算 10 秒。
- 模型 A 失败不扣费；模型 B 失败仅在存在等价且符合当前状态的作者台词时提交替代回复。
- 一次私聊最多三轮，首轮成功提交消耗 1 AP，后续两轮免费；正向关系收益每会话最多一次。
- 在线提出承诺先澄清，明确确认后登记；故障恢复切换离线保留同一会话账本。
- 右侧常驻自己的四项状态，仅在有效注视或交谈时显示一名 NPC；未确诊伤势与隐藏数值不显示。
- 对话使用多行文本输入，Enter 换行，点击发送；游戏快捷键与对话输入隔离。

v1.5 当前为候选版本。真实模型与人工验收结果、未完成项见构建文档，不能将自动化通过视为完整发布验收。

## 基础玩法

- 每轮分为早晨、午后、黄昏，每阶段 4 AP；阶段开始时从四个房间中选择供暖区。
- 食物、休整、泛化治疗、体能、体温、伤势、压力和信任共同影响行动条件与结局。
- 左侧任务指引展示多种可选下一步，由玩家自行组合医疗、技术和风险推进路线。
- 室外天线改由可见的控制终端交互，并通过玩家视线查询同源的场景可达性审计。
- 设置页可选主流语言模型厂商、BaseURL 和模型，API Key 只保存在本次运行内存中。
- 在线自由文本由模型 A 解析意图，本地规则筛选 NPC 知识；模型回复验证通过后再提交对话结果，模型不能直接修改世界状态。

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
[`docs/BUILD_AND_PLAY_v1.5.md`](docs/BUILD_AND_PLAY_v1.5.md)；历史说明保留在
[`v1.3`](docs/BUILD_AND_PLAY_v1.3.md) 和 [`v1.2`](docs/BUILD_AND_PLAY_v1.2.md)。关卡对象的编辑器
拖动与替换方法见 [`docs/LEVEL_EDITING.md`](docs/LEVEL_EDITING.md)。

## LLM 配置与离线运行

运行配置位于
[`WhiteoutStation/Content/Agents/AgentRuntime.v1.5.json`](WhiteoutStation/Content/Agents/AgentRuntime.v1.5.json)。
当前协议为 `bounded_roleplay_v5`，schema 为 `8`，默认预设为
`deepseek-v4-flash` 与官方 Chat Completions 端点；`llm_enabled=false`，默认离线可玩。

在游戏设置页选择 provider、BaseURL 和 model，输入仅驻留本次进程内存的 API Key，
再开启 LLM。Development 联调也可通过启动进程环境启用：

```powershell
$env:WHITEOUT_LLM_API_KEY = '<your-key>'
$env:WHITEOUT_LLM_ENABLED = 'true'
```

关闭 AI 时使用固定选项。在线失败保留输入并显示系统提示；仅表达阶段有等价作者台词时可用该台词提交。
密钥不得写入运行配置 JSON、源码、日志或发布包；loopback mock 不携带 Authorization。

v1.5 自动存档槽为 `WhiteoutStation_Autosave_v1_5`，支持读取并迁移 v1.4/v1.3/v1.2/v1.1 存档。
未完成的对话事务、网络请求及会话轮次不会跨读档恢复。

## 回归

```powershell
python -X utf8 Tools/Dialogue/validate_dialogue_v15.py
python -X utf8 Tools/Release/validate_source_v15.py --contract-only
python -X utf8 -m pytest Tools/Dialogue/test_dialogue_v15.py -q
```

UE 自动化、作者选项路线和真实服务测试的实际入口见 v1.5 构建文档。

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
