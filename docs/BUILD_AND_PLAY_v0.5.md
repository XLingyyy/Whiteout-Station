# v0.5 构建与游玩说明

本文适用于 v0.5 发布源码、UE 5.8 Editor Development 版本和最终 Win64 Shipping Demo。

## 环境

- Windows 64-bit
- Unreal Engine 5.8，当前工作站安装于 `G:\UnrealEngine\UE_5.8`
- 项目：`G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject`

## 最终 Shipping Demo

发布目录：

```text
G:\Whiteout Station\Builds\WhiteoutStation-v0.5-Win64-20260725T134938Z-9bd94fab-release
```

启动：

```powershell
& 'G:\Whiteout Station\Builds\WhiteoutStation-v0.5-Win64-20260725T134938Z-9bd94fab-release\Windows\WhiteoutStation.exe'
```

该包绑定源码提交 `9bd94fab63f446290fbb5ababf809529a91c1b7c`。包内 `Validation/ShippingSmoke` 保存三条路线和两类 AI 降级的事件日志、截图与脱敏汇总。

## 构建 Editor

关闭占用项目模块的编辑器实例后，在仓库根目录运行：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
  WhiteoutStationEditor Win64 Development `
  'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -WaitMutex
```

当前 v0.5 已通过该 UE 5.8 Editor Development 构建。

## 启动与游玩

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject'
```

打开默认地图 `MVP_StationMap` 后运行 PIE。游戏从 08:15 开始，每消耗 1 AP 推进 75 分钟；在 8 AP 内修复发电机、校准室外天线并发出求救信号。完整流程默认离线，不需要 API Key。

## 操作

| 输入 | 功能 |
|---|---|
| WASD | 移动 |
| 鼠标 | 观察；在菜单、证据板和对话中选择 |
| F | 与 NPC 开始对话；查看或确认物体行动 |
| Q | 在行动预览中循环口粮分配或治疗资源 |
| E | 打开 / 关闭证据板 |
| Space | 跳过开场；跳跃 |
| Enter | 结算；提前失败结算需在状态未变化时再次确认 |
| R | 开始新一轮 |
| C | 读取自动存档 |
| Esc | 返回上一层；游戏态打开暂停菜单 |
| Alt+F4 | 直接退出 |

对准顾衡或叶澄按 F 后，由 NPC 先开口，再选择询问、质疑、承诺或安抚。叶澄不提供承诺；顾衡承诺需选择合法条件。可以直接发送结构化意图，也可输入最多 280 个字符补充说法。

对准口粮分配点或治疗点第一次按 F 打开预览，按 Q 循环当前行动的资源方案，第二次按 F 按预览中的完整参数提交。规则层会再次校验资源、前置条件和 AP。

证据板打开时释放鼠标；左侧五类过滤和证据卡可点击。Esc 或 E 返回游戏。任意全屏面板打开时，主 HUD 与准心自动隐藏。

## 默认离线与 DeepSeek

在线表达默认关闭。离线状态下，NPC 使用与意图、证据和承诺条件对应的确定性中文台词，规则、AP、资源、存档、结算和三条路线均不依赖模型。

官方表达服务固定使用 `deepseek-v4-flash`。启动游戏进程前，在该进程环境中同时设置：

- `WHITEOUT_LLM_ENABLED=true`
- `WHITEOUT_LLM_API_KEY` 为有效 DeepSeek 密钥

密钥不得放入仓库、命令行、日志或分发文件。运行时只向 HTTPS 且主机严格等于 `api.deepseek.com` 的官方端点发送 Authorization。在线请求使用非思考模式、非流式 JSON Object 输出；正文只有以下四个字段合法：

```json
{
  "npc_line": "NPC 台词",
  "emotion": "情绪标签",
  "used_action_id": "已提交的对话动作 ID",
  "referenced_fact_ids": []
}
```

模型只提供表达。C++ 先完成动作、意图、承诺和事实许可决策，再把只读投影发送给模型；模型回复不能改写游戏状态。任何网络、HTTP、结束原因、envelope、schema 或事实校验失败都会在有限时间内显示本地台词。

HUD 会区分：

- `DeepSeek 在线表达`
- `本地预设`
- `在线失败后本地降级`

## 本地合同 mock

先在仓库根目录启动无凭据 mock：

```powershell
python -X utf8 Tools/Agents/mock_agent_server.py `
  --host 127.0.0.1 `
  --port 8765 `
  --audit Artifacts/Integration/manual-mock-audit.jsonl
```

再启动游戏：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -game `
  -WhiteoutLLMEnabled=true `
  -WhiteoutAgentEndpoint=http://127.0.0.1:8765/chat/completions
```

loopback 请求不会携带 DeepSeek Key。停止 mock 后，后续对话会回退到本地台词。

## 本地数据

Editor 本地数据位于 `WhiteoutStation/Saved`；打包运行时位于 `%LOCALAPPDATA%/WhiteoutStation/Saved`。

- `SaveGames/WhiteoutStation_Autosave_v0_5.sav`：v0.5 自动存档。
- `Logs/WhiteoutStation_EventLog.json`：行动、意图、承诺、AP、危机、结局、评分与模型调用计数。
- `Logs/WhiteoutStation_ModelAudit.jsonl`：脱敏模型调用元数据，达到 2 MiB 后轮转。
- `Config/Windows/GameUserSettings.ini`：FOV 与四路音量。
- `WhiteoutRuntimeSmoke.png`：仅传入自动截图参数时生成。

启动参数 `-WhiteoutContinue` 可在开始时读取自动存档；游戏中按 C 也可读取。v0.5 使用独立存档槽，避免旧规则存档静默混入。

## 回归

Python：

```powershell
Push-Location Tools/Agents
python -X utf8 -m pytest . -q
Pop-Location

Push-Location Tools/Rules
python -X utf8 -m pytest test_whiteout_rules.py -q
python -X utf8 run_routes.py
Pop-Location

Push-Location Tools/Release
python -X utf8 -m pytest test_v05_release_gates.py -q
Pop-Location
```

UE Automation：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -unattended -nop4 -nosplash -nullrhi `
  '-ExecCmds=Automation RunTests WhiteoutStation' `
  '-TestExit=Automation Test Queue Empty' `
  '-ReportExportPath=G:\Whiteout Station\Artifacts\TestResults\manual-v05'
```

当前确认结果为 Agents 39 passed、Rules 28 passed、Release 14 passed、UE Automation 7 / 7 且 0 warning / failed / not run。最终自动化报告位于 `Artifacts/TestResults/v05-final-20260725-212000`。最终 Shipping 烟测 5 / 5，源码门禁和发布清单校验均为 PASS。

## 角色与地图保护

v0.5 未修改顾衡或叶澄的人物模型、骨骼、材质、动作、动画、AnimBP 或 LookAt 表现。用户工作树中的 `MVP_StationMap.umap` 既有修改保持原样，构建和回归过程不覆盖、暂存或清理该文件。
