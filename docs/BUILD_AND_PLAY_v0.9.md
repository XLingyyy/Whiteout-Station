# Whiteout Station v0.9 - 构建、运行与验收

日期：2026-07-29

v0.9 直接维护在 `main`，不使用额外工作树。用户地图、模型、骨骼和材质
保持不变；当前动画只位于两个 `AnimationsV09` 目录。

## Editor

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
  WhiteoutStationEditor Win64 Development `
  'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -WaitMutex -NoHotReload
```

打开 `/Game/WindStation/World/MVP_StationMap` 后进入 Play。

## 自动化

```powershell
python -X utf8 -m pytest Tools/Agents -q
python -X utf8 -m pytest Tools/Rules -q
python -X utf8 -m pytest Tools/Release/test_v09_release_gates.py -q
python -X utf8 -m unittest Tools.Release.test_scan_secrets -v
python -X utf8 Tools/Release/scan_secrets.py --history
python -X utf8 Tools/Release/validate_source_v09.py --repo-root . --final
```

UE 自动化：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  '-ExecCmds=Automation RunTests WhiteoutStation' `
  '-TestExit=Automation Test Queue Empty' `
  -unattended -nop4 -nosplash -NullRHI -log
```

动画姿态审计：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -run=pythonscript `
  -script='G:\Whiteout Station\Tools\Editor\audit_v09_npc_animations.py' `
  -unattended -nop4 -nosplash -NullRHI
```

## DeepSeek

运行配置为
`WhiteoutStation/Content/Agents/AgentRuntime.v0.9.json`。只在当前启动
进程设置 `WHITEOUT_LLM_API_KEY`；无密钥和网络异常均走本地确定性降级。

## Shipping

源码提交且工作树干净后，使用 `RunUAT BuildCookRun` 构建 Win64 Shipping，
随后依次运行：

```powershell
python -X utf8 Tools/Release/run_v09_input_smoke.py --artifact-root <artifact>
python -X utf8 Tools/Release/run_v09_shipping_smoke.py --artifact-root <artifact>
python -X utf8 Tools/Release/create_release_manifest_v09.py `
  --repo-root . --artifact-root <artifact> --run-id <run-id> `
  --source-ref <commit> --build-timestamp-utc <timestamp>
python -X utf8 Tools/Release/validate_release_v09.py `
  --repo-root . --artifact-root <artifact> --expected-source-ref <commit>
```

当前分发等级为 `local_review_only`。叶澄 V10 的 Noanoa 发型在公开分发前
必须取得嵌入/再分发书面许可或替换。
