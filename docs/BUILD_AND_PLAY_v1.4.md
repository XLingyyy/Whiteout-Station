# Whiteout Station v1.4：构建、运行与验收

## 环境与发布契约

- Windows 64-bit、Unreal Engine 5.8、Visual Studio 2022 C++ 工具链。
- 项目：`WhiteoutStation/WhiteoutStation.uproject`；默认地图：`/Game/WindStation/World/MVP_StationMap`。
- 项目与 Agent runtime 版本均为 `1.4.0`，runtime schema 为 `7`，对话协议为 `bounded_roleplay_v4`，prompt 模式为 `subjective_context_single_call`。
- `Content/Dialogue/v1.4` 下六个 JSON 以 NonUFS 文件进入发布包。角色知识、世界知识、关系经历、对话策略和安全回退均从这里加载。

v1.2、v1.3、v1.4 施工 DOCX 是用户提供的本地输入，不属于构建输入和提交内容。

## 定向验证与 Editor 构建

先运行对话内容和发行契约测试：

```powershell
python -X utf8 Tools/Dialogue/validate_roleplay_content.py
python -X utf8 -m pytest -p no:cacheprovider `
  Tools/Dialogue/test_roleplay_content.py `
  Tools/Release/test_v14_dialogue_release_gates.py -q
```

构建 Editor，再运行项目自动化测试：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
  WhiteoutStationEditor Win64 Development `
  'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -WaitMutex -NoHotReloadFromIDE

& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -unattended -nop4 -nosplash -NullRHI `
  '-ExecCmds=Automation RunTests WhiteoutStation;Quit' `
  '-TestExit=Automation Test Queue Empty' `
  '-ReportOutputPath=G:\Whiteout Station\WhiteoutStation\Saved\AutomationReports\V14Final'
```

最终源码门禁需在 `main` 已提交、已推送且发布源干净时执行：

```powershell
python -X utf8 Tools/Release/validate_source_v14.py --repo-root . --final
```

开发中可用 `--contract-only` 只检查 v1.4 版本、资产与代码路径；最终发布不得用它替代完整门禁。

## 运行与 LLM 开关

`WhiteoutStation/Content/Agents/AgentRuntime.v1.4.json` 默认设置 `llm_enabled=false`。关闭在线模型时，所有对话走本地、状态感知的安全回退。启用方式有两种：

- 在游戏设置页选择 provider、BaseURL、model，输入只驻留当前进程的 API Key，再开启 LLM。
- Development 联调时设置进程环境变量：

```powershell
$env:WHITEOUT_LLM_API_KEY = '<your-key>'
$env:WHITEOUT_LLM_ENABLED = 'true'
```

密钥只能留在进程内存、环境变量或未提交的 `LocalConfig/WhiteoutLLM.ini`。Runtime JSON、源码、日志、发布包和 Git 历史不得包含凭据。

Development 控制台执行 `ws.DialogueDebug 1` 可查看本轮知识 ID、校验来源和回退原因；Shipping 固定隐藏这些调试信息。

## 三轮会话与安全边界

- 一次私聊最多三轮。第一轮成功提交消耗 1 AP，后续两轮不再消耗 AP；切换 NPC、结束私聊、跨日阶段或读档会结束会话。
- 每条玩家输入最多发起一次模型请求。网络失败、超时、非法 JSON、知识越界或动作提案非法时立即采用一次本地回退，不进行模型重试。
- 模型只接收当前 NPC 档案、主观状态、过滤后的 Top-K 知识、近期对话与安全长期记忆。未满足披露条件的隐藏知识不进入 prompt。
- 模型输出只提供台词、知识引用、断言、记忆摘要、表现意图和动作提案。AP、资源、关系、任务、事实升级、结局及提案执行均由本地规则校验并原子提交。
- 已锁定的供暖区、已完成任务及已确认状态按当前快照回答，不能再次要求玩家做同一选择。

存档槽为 `WhiteoutStation_Autosave_v1_4`。读取 v1.3/v1.2/v1.1 存档后迁移到 v1.4；未完成事务、网络请求和会话轮次不会跨读档恢复。

## Win64 Shipping

为每次归档创建唯一目录：

```powershell
$artifact = 'G:\Whiteout Station\Artifacts\WhiteoutStation-v1.4-Win64-<UTC>-<commit>'
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
  -project='G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -noP4 -platform=Win64 -clientconfig=Shipping `
  -build -cook -stage -pak -archive -utf8output `
  -archivedirectory=$artifact
```

归档后确认以下 NonUFS 文件存在：

- `Windows/WhiteoutStation/Content/Agents/AgentRuntime.v1.4.json`
- `Windows/WhiteoutStation/Content/Dialogue/v1.4/WorldKnowledge.json`
- `Windows/WhiteoutStation/Content/Dialogue/v1.4/NPC_GuHeng.json`
- `Windows/WhiteoutStation/Content/Dialogue/v1.4/NPC_YeCheng.json`
- `Windows/WhiteoutStation/Content/Dialogue/v1.4/Relationship_GuHeng_YeCheng.json`
- `Windows/WhiteoutStation/Content/Dialogue/v1.4/DialoguePolicy.json`
- `Windows/WhiteoutStation/Content/Dialogue/v1.4/SafeFallbacks.json`

## 发布前手动验收

用同一初始存档分别关闭、开启 LLM，执行以下最小回归：

1. 向叶澄询问“对于顾衡，你知道多少”，确认回答直接、符合叶澄视角且不泄露未解锁秘密。
2. 锁定供暖区后继续询问供暖，确认 NPC 陈述现状，不再要求重新选择。
3. 在同一次私聊连续完成三轮，确认只扣一次 AP；第四轮不可继续。
4. 触发在线超时、非法 JSON 和越界知识响应，确认每次只出现一次本地安全回退。
5. 对比 LLM 开关两组结果，确认 AP、资源、关系、任务、事实、结局和分数一致。
6. 复核受保护地图与 NPC 资产未变化，并确认审计日志不含玩家原文、NPC 台词、prompt、请求正文或凭据。
