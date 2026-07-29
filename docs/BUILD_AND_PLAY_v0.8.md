# Whiteout Station v0.8 - 构建、运行与验收

日期：2026-07-29

## 范围

v0.8 工作树：`G:\WhiteoutStation-v08-worktree`

分支：`codex/v0.8-evolution`

用户主工作树、用户地图摆放、顾衡和叶澄的源模型均不参与自动改写。精确骨架
动画只新增到两个 `AnimationsV08` 目录。

当前发布等级为 `local_review_only`。叶澄 V10 的 Noanoa 发型尚未取得产品
嵌入和再分发书面许可，公开分发前必须取得许可或替换该资产。

## Editor 构建与启动

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
  WhiteoutStationEditor Win64 Development `
  'G:\WhiteoutStation-v08-worktree\WhiteoutStation\WhiteoutStation.uproject' `
  -WaitMutex -NoHotReload

& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  'G:\WhiteoutStation-v08-worktree\WhiteoutStation\WhiteoutStation.uproject'
```

打开 `/Game/WindStation/World/MVP_StationMap` 后进入 Play。

## DeepSeek

运行配置：

`WhiteoutStation/Content/Agents/AgentRuntime.v0.8.json`

官方模型与端点：

```text
deepseek-v4-flash
https://api.deepseek.com/chat/completions
```

仅对当前启动进程设置密钥：

```powershell
$env:WHITEOUT_LLM_API_KEY = '<your-key>'
```

显式离线：

```powershell
WhiteoutStation.exe -WhiteoutLLMEnabled=false
```

模型输出只允许 NPC 台词、情绪、已提交行动、已授权事实、移动枚举和反应枚举。
本地 C++ 继续控制坐标约束、碰撞、AP、资源、事实、承诺、任务、评分和结局。

## 自动化

```powershell
python -X utf8 -m pytest Tools/Agents -q
python -X utf8 -m pytest Tools/Rules -q
python -X utf8 -m pytest Tools/Release/test_v08_release_gates.py -q
python -X utf8 -m unittest Tools.Release.test_scan_secrets -v
python -X utf8 Tools/Release/scan_secrets.py --history
python -X utf8 Tools/Capture/audit_v08_ui.py
python -X utf8 Tools/Release/validate_source_v08.py --repo-root . --final
```

精确骨架动画审计：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\WhiteoutStation-v08-worktree\WhiteoutStation\WhiteoutStation.uproject' `
  -run=pythonscript `
  -script='G:\WhiteoutStation-v08-worktree\Tools\Editor\audit_v08_npc_animations.py' `
  -unattended -nop4 -nosplash -NullRHI
```

UE 自动化：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\WhiteoutStation-v08-worktree\WhiteoutStation\WhiteoutStation.uproject' `
  '-ExecCmds=Automation RunTests WhiteoutStation' `
  '-TestExit=Automation Test Queue Empty' `
  -unattended -nop4 -nosplash -NullRHI -log
```

## Shipping 候选

以下流程要求源码已提交且工作树干净：

```powershell
Set-Location 'G:\WhiteoutStation-v08-worktree'
$sourceCommit = (git rev-parse HEAD).Trim()
$buildStarted = (Get-Date).ToUniversalTime()
$runId = '{0}-{1}-release' -f `
  $buildStarted.ToString('yyyyMMddTHHmmssZ'), `
  $sourceCommit.Substring(0, 8)
$artifact = Join-Path (Resolve-Path '.\Builds') `
  "WhiteoutStation-v0.8-Win64-$runId"

& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
  -project='G:\WhiteoutStation-v08-worktree\WhiteoutStation\WhiteoutStation.uproject' `
  -noP4 -platform=Win64 -clientconfig=Shipping `
  -build -cook -stage -pak -iostore -archive `
  "-archivedirectory=$artifact" -utf8output

Copy-Item '.\Distribution\README_v0.8.txt' `
  (Join-Path $artifact 'README_v0.8.txt')
Copy-Item '.\SourceAssets\ASSET_LICENSES.md' `
  (Join-Path $artifact 'ASSET_LICENSES.md')

python -X utf8 Tools/Release/run_v08_input_smoke.py `
  --artifact-root $artifact
python -X utf8 Tools/Release/run_v08_shipping_smoke.py `
  --artifact-root $artifact --timeout-seconds 120

python -X utf8 Tools/Release/create_release_manifest_v08.py `
  --repo-root . --artifact-root $artifact --run-id $runId `
  --source-ref $sourceCommit `
  --build-timestamp-utc $buildStarted.ToString('o')

python -X utf8 Tools/Release/validate_release_v08.py `
  --repo-root . --artifact-root $artifact `
  --expected-source-ref $sourceCommit
```

真实输入和 Shipping 烟测必须在 manifest 生成前完成。manifest 建立后，门禁会
拒绝混入新证据。
