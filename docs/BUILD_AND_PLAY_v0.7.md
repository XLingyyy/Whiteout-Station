# Whiteout Station v0.7 - 构建、运行与验收

日期：2026-07-28

## 范围与许可

v0.7 在 `G:\WhiteoutStation-v07-worktree`、分支
`codex/v0.7-evolution` 中构建。用户主工作树不参与构建，也不得清理、
重置或覆盖。

叶澄 V10 的 Noanoa 发型尚未取得“嵌入产品/服务软件”的书面许可。
当前可执行包仅供同一设备上的私人开发与评审，必须保留
`LOCAL REVIEW BUILD - DO NOT REDISTRIBUTE` 标记。公开或商业分发前需
取得许可，或替换该发型。

## 环境

- Windows 11
- Unreal Engine 5.8：`G:\UnrealEngine\UE_5.8`
- 项目：`G:\WhiteoutStation-v07-worktree\WhiteoutStation\WhiteoutStation.uproject`
- Git LFS 已安装且对象完整

## 构建 Editor

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
  WhiteoutStationEditor Win64 Development `
  'G:\WhiteoutStation-v07-worktree\WhiteoutStation\WhiteoutStation.uproject' `
  -WaitMutex -NoHotReloadFromIDE
```

启动 Editor：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  'G:\WhiteoutStation-v07-worktree\WhiteoutStation\WhiteoutStation.uproject'
```

## 玩家运行

在 Editor 中打开：

`/Game/WindStation/World/MVP_StationMap`

进入游戏后，Space 或鼠标左键逐句推进黑幕开场。常用操作：

- `WASD` 移动，鼠标观察；
- `F` 对话或预览/确认行动；
- `Q` 在行动预览中切换资源方案；
- `E` 证据板，`H` 生存手册；
- `Esc` 返回或暂停；
- `Enter` 结束本轮；
- `C` 读取最近自动存档，`R` 开始新一轮。

默认准心为空心圆；瞄准可交互对象时切换为手形。

## DeepSeek 与确定性降级

`AgentRuntime.v0.7.json` 默认启用 AI 接入。没有密钥时不会发起网络请求，
直接使用本地确定性结果。

当前官方模型：

```text
deepseek-v4-flash
https://api.deepseek.com/chat/completions
```

仅在当前进程中设置密钥：

```powershell
$env:WHITEOUT_LLM_API_KEY = '<your-key>'
```

显式离线：

```powershell
WhiteoutStation.exe -WhiteoutLLMEnabled=false
```

模型只选择固定移动与反应枚举。C++ 负责碰撞、安全距离、岗位半径、单步
距离和冷却，并继续独占 AP、资源、事实、承诺、评分与结局。

## 自动化

```powershell
python -X utf8 -m pytest Tools/Agents -q
python -X utf8 -m pytest Tools/Rules -q
python -X utf8 -m pytest Tools/Release/test_v07_release_gates.py -q
python -X utf8 -m unittest Tools.Release.test_scan_secrets -v
python -X utf8 Tools/Release/scan_secrets.py --history
python -X utf8 Tools/Capture/audit_v07_ui.py
```

UE 自动化：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\WhiteoutStation-v07-worktree\WhiteoutStation\WhiteoutStation.uproject' `
  '-ExecCmds=Automation RunTests WhiteoutStation' `
  '-TestExit=Automation Test Queue Empty' `
  -unattended -nop4 -nosplash -NullRHI -log
```

## 唯一 Shipping 候选

以下流程要求源码已提交且工作树干净：

```powershell
Set-Location 'G:\WhiteoutStation-v07-worktree'
$sourceCommit = (git rev-parse HEAD).Trim()
$buildStarted = (Get-Date).ToUniversalTime()
$runId = '{0}-{1}-release' -f `
  $buildStarted.ToString('yyyyMMddTHHmmssZ'), `
  $sourceCommit.Substring(0, 8)
New-Item -ItemType Directory -Force -Path '.\Builds' | Out-Null
$artifact = Join-Path (Resolve-Path '.\Builds') `
  "WhiteoutStation-v0.7-Win64-$runId"

& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
  -project='G:\WhiteoutStation-v07-worktree\WhiteoutStation\WhiteoutStation.uproject' `
  -noP4 -platform=Win64 -clientconfig=Shipping `
  -build -cook -stage -pak -iostore -archive `
  "-archivedirectory=$artifact" -utf8output

Copy-Item '.\Distribution\README_v0.7.txt' `
  (Join-Path $artifact 'README_v0.7.txt')
Copy-Item '.\SourceAssets\ASSET_LICENSES.md' `
  (Join-Path $artifact 'ASSET_LICENSES.md')

python -X utf8 Tools/Release/run_v07_input_smoke.py `
  --artifact-root $artifact
python -X utf8 Tools/Release/run_v07_shipping_smoke.py `
  --artifact-root $artifact --timeout-seconds 120

python -X utf8 Tools/Release/create_release_manifest_v07.py `
  --repo-root . --artifact-root $artifact --run-id $runId `
  --source-ref $sourceCommit `
  --build-timestamp-utc $buildStarted.ToString('o')

python -X utf8 Tools/Release/validate_release_v07.py `
  --repo-root . --artifact-root $artifact `
  --expected-source-ref $sourceCommit
```

输入与 Shipping 烟测必须先于 manifest 创建；清单生成后工具会拒绝混入
新证据。最终包内 `distribution_class` 固定为 `local_review_only`。
