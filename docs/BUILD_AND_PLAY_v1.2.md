# Whiteout Station v1.2：构建、运行与验收

## 环境

- Windows 64-bit、Unreal Engine 5.8、Visual Studio 2022 C++ 工具链。
- 项目文件：`WhiteoutStation/WhiteoutStation.uproject`。
- 默认地图：`/Game/WindStation/World/MVP_StationMap`。
- 项目版本：`1.2.0`；基础玩法规则仍为 `1.1.0/schema 4`；对话协议为 `1.2.0/schema 5`。

## Editor 构建与运行

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
  WhiteoutStationEditor Win64 Development `
  'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -WaitMutex -NoHotReloadFromIDE
```

在 UE 编辑器打开项目并运行 `MVP_StationMap`。自由文本复现：与顾衡对话，选择“询问”，输入“要怎么样你才会帮我修理发电机？”。回复第一部分应直接说明玩家协作、维修间供暖与体能恢复，或可靠替代件，并把手伤描述为风险。

## 在线与离线

运行配置为 `WhiteoutStation/Content/Agents/AgentRuntime.v1.2.json`，默认 `llm_enabled=false`。离线模式完整保留规则生成的语义骨架。

在线调试可在设置页选择提供商、BaseURL、模型并输入 API Key。Key 仅保存在当前进程内存。开发环境也支持进程环境变量：

```powershell
$env:WHITEOUT_LLM_API_KEY = '<your-key>'
$env:WHITEOUT_LLM_ENABLED = 'true'
```

退出游戏后清理该进程环境。仓库、配置、日志和发布包均不得包含 Key。

## 对话调试与审计

Development 构建控制台执行：

```text
Whiteout.DialogueDebug 1
```

对话状态区会显示 SpeechAct、QueryType、TargetActionId、置信度、解析来源和最终答案来源。日志位于 `WhiteoutStation/Saved/Logs`：

- `WhiteoutStation_DialogueAudit.jsonl`：语义来源、条件、脊柱哈希、尾句结果和答案来源。
- `WhiteoutStation_ModelAudit.jsonl`：HTTP、延迟、重试和校验结果，不记录凭据。
- `WhiteoutStation_OfferAudit.jsonl`：条件固定、协议接受、履约和违约。

## 自动化

```powershell
python -X utf8 -m pytest Tools/Rules -q -p no:cacheprovider
python -X utf8 -m pytest Tools/Agents -q -p no:cacheprovider
python -X utf8 -m pytest Tools/Release/test_v11_release_gates.py -q -p no:cacheprovider
```

UE 自动化：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -unattended -nop4 -nosplash -nullrhi `
  '-ExecCmds=Automation RunTests WhiteoutStation;Quit' `
  '-TestExit=Automation Test Queue Empty' -log
```

确认 `WhiteoutStation/Saved/Logs/WhiteoutStation.log` 中所有 `Test Completed` 均为 `Result={Success}`。

## Shipping

使用 UAT 生成 Win64 Shipping 包后，将输出放入唯一目录 `Artifacts/WhiteoutStation-v1.2-Win64-<UTC>-<commit>/Windows`。运行：

```powershell
python -X utf8 Tools/Release/run_v12_shipping_smoke.py `
  --artifact-root 'Artifacts/WhiteoutStation-v1.2-Win64-<UTC>-<commit>'
```

该门禁覆盖无 Key、显式离线、错误凭据、超时/不可达端点、loopback、确定性路线、连续对话、NPC 表演和 AI A/B 权威一致性。

最终源码门禁：

```powershell
python -X utf8 Tools/Release/validate_source_v12.py --repo-root . --final
```

## 存档兼容

v1.2 写入 `WhiteoutStation_Autosave_v1_2`。当 v1.2 槽不存在时，会读取 `WhiteoutStation_Autosave_v1_1`，接受 `1.1.0` 存档并迁移到 v1.2；旧存档新增的语义帧和协商列表使用安全默认值。
