# Whiteout Station v1.3：构建、运行与验收

## 环境与版本

- Windows 64-bit、Unreal Engine 5.8、Visual Studio 2022 C++ 工具链。
- 项目文件：`WhiteoutStation/WhiteoutStation.uproject`。
- 默认地图：`/Game/WindStation/World/MVP_StationMap`。
- 项目版本：`1.3.0`；Agent runtime：`1.3.0/schema 6`；对话协议：`dialogue_epistemic_v3`；规则保持 `1.1.0/schema 6`。

两份 v1.2/v1.3 施工 DOCX 是用户提供的本地输入，不属于发布源、构建输入或提交内容。

## Editor 构建与自动化

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
  '-ReportOutputPath=G:\Whiteout Station\WhiteoutStation\Saved\AutomationReports\V13Final'
```

Python 回归：

```powershell
python -X utf8 -m pytest Tools/Agents -q
python -X utf8 -m pytest Tools/Rules -q
python -X utf8 -m pytest Tools/Release -q
```

## 在线表达与本地回退

运行契约位于 `WhiteoutStation/Content/Agents/AgentRuntime.v1.3.json`，默认关闭在线表达。在线输出只实现本地冻结的语义原子和披露事实；AP、人物状态、资源、任务、结局与分数始终由本地规则提交。网络失败、超时、非法 JSON、缺少原子、泄露事实或新增条件时只使用一次本地自然回退，不进行第二次模型调用。

密钥只能放在未提交的 `LocalConfig/WhiteoutLLM.ini`、当前进程环境或运行时设置会话中。不得写入 Runtime JSON、源码、日志、发布包或 Git 历史。进程环境示例：

```powershell
$env:WHITEOUT_LLM_API_KEY = '<your-key>'
$env:WHITEOUT_LLM_ENABLED = 'true'
```

Development 构建可在控制台执行 `ws.DialogueDebug 1` 查看语义与校验信息。Shipping 构建固定隐藏 provider、answer source 和 validation reason，设置页只显示在线角色表达是否可用。

## 审计与存档

- `WhiteoutStation/Saved/Logs/WhiteoutStation_DialogueAudit.jsonl`：每次已提交对话的事务、语义原子、事实 ID、来源、校验结果与 token 数；不记录玩家原文、NPC 台词、prompt、请求/响应、端点、模型或 API Key。
- `WhiteoutStation/Saved/Logs/WhiteoutStation_ModelAudit.jsonl`：Development 联调所需的请求元数据与校验结果，不记录凭据或完整内容。
- `WhiteoutStation/Saved/Logs/WhiteoutStation_OfferAudit.jsonl`：条件固定、协议接受、履约与违约。

v1.3 写入 `WhiteoutStation_Autosave_v1_3`。槽不存在时可读取 v1.2，再兼容 v1.1，并迁移到当前规则结构；PendingDialogue、网络请求与会话内历史不会跨读档恢复。

## Win64 Shipping

先确保 `main` 已提交且待发布文件干净。使用唯一归档目录：

```powershell
$artifact = 'G:\Whiteout Station\Artifacts\WhiteoutStation-v1.3-Win64-<UTC>-<commit>'
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
  -project='G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -noP4 -platform=Win64 -clientconfig=Shipping `
  -build -cook -stage -pak -archive -utf8output `
  -archivedirectory=$artifact
```

确认包内存在 `Windows/WhiteoutStation/Content/Agents/AgentRuntime.v1.3.json`，随后运行真实键鼠、对话降级、隐私审计与 AI A/B 门禁：

```powershell
python -X utf8 Tools/Release/run_v13_shipping_smoke.py `
  --artifact-root $artifact
```

最终源码门禁必须在提交与推送后运行：

```powershell
python -X utf8 Tools/Release/validate_source_v13.py --repo-root . --final
```

门禁要求 Shipping 使用 `semantic_atoms_full_line`、六字段 v3 输出，AI 开关的权威状态完全一致，DialogueAudit 仅含白名单字段，受保护地图与人物资产哈希未变化，仓库无密钥或发布源残留。
