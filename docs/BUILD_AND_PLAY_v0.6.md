# v0.6 构建与游玩说明

本文适用于 Whiteout Station v0.6 源码、Unreal Engine 5.8 Editor Development 版本和最终 Win64 Shipping Demo。

## 最终 Shipping Demo

发布目录：

```text
G:\WhiteoutStation-v06-worktree\Builds\WhiteoutStation-v0.6-Win64-20260726T120458Z-4326f3ba-release
```

启动：

```powershell
& 'G:\WhiteoutStation-v06-worktree\Builds\WhiteoutStation-v0.6-Win64-20260726T120458Z-4326f3ba-release\Windows\WhiteoutStation.exe'
```

该包绑定源码提交 `4326f3ba268de4846f1b7889ca84858daf70984b`。包内
`Validation/InputSmoke` 保存两条真实输入路径的截图与事件日志，
`Validation/ShippingSmoke` 保存四结局、在线/离线和故障降级矩阵，
`Validation/gate_manifest.json` 保存 89 个包内文件的 SHA-256。

## 目标与操作

游戏从 08:15 开始。玩家在 8 AP 内修复发电机、校准室外天线并发出求救信号；每消耗 1 AP 推进 75 分钟。路线、人员状态、物资、证据、关系和承诺共同决定评分与结局。

| 输入 | 功能 |
|---|---|
| WASD | 移动 |
| 鼠标 | 观察；点击推进开场；在界面中选择 |
| F | 与人物对话；查看或确认物体行动 |
| Q | 在行动预览中切换口粮分配或治疗资源 |
| E | 打开 / 关闭证据板 |
| H | 打开 / 关闭生存手册 |
| Space | 逐句推进开场；开场结束后跳跃 |
| Enter | 结算；提前失败结算需再次确认 |
| C | 读取最近自动存档 |
| R | 开始新一轮 |
| Esc | 返回上一层；游戏态打开暂停菜单 |
| Alt+F4 | 退出 |

黑幕开场没有自动跳句。Space 或点击每次只推进一句；第 7 句结束后黑幕渐隐到第一人称站内视角。HUD 的“当前建议”随状态更新；按 H 可查看完整目标、状态影响、危险阈值和改变方式。

行动物体第一次按 F 打开规则预览，按 Q 切换合法资源方案，第二次按 F 提交。对话先显示当前语境允许的意向，再接受最多 280 字的自由输入。初始顾衡只显示询问和安抚，叶澄只显示询问；质疑与顾衡承诺会随证据、伤情、关系、压力和危机阶段开放，叶澄始终没有承诺入口。

## 暂停、设置与存档

暂停菜单提供继续、保存、读取、生存手册、设置、重开和退出。设置页可实时调整：

- 75°—105° FOV；
- 主音量、音乐、环境、效果四路音量；
- 90%—120% 界面字号；
- “减少动态效果”，缩短开场、结局和面板过渡。

本地数据位于 `%LOCALAPPDATA%\WhiteoutStation\Saved`：

- `SaveGames/WhiteoutStation_Autosave_v0_6.sav`：v0.6 自动/手动存档；
- `Logs/WhiteoutStation_EventLog.json`：行动、意向、承诺、AP、结局和评分；
- `Logs/WhiteoutStation_ModelAudit.jsonl`：脱敏模型调用元数据；
- `Config/Windows/GameUserSettings.ini`：画面、音量、字号和动态效果设置。

## DeepSeek 可选表达

完整流程默认离线，不需要 API Key。在线表达固定使用 `deepseek-v4-flash`，只生成 NPC 台词；AP、物资、事实权限、承诺、状态、评分和结局始终由 C++ 规则提交。

启动游戏进程前设置：

```powershell
$env:WHITEOUT_LLM_ENABLED = 'true'
$env:WHITEOUT_LLM_API_KEY = '<DeepSeek API Key>'
& '.\Windows\WhiteoutStation.exe'
```

同一会话最多携带最近 4 轮只读上下文。离开对话、新局或读档会取消活动请求；断网、超时、限流、非法 envelope/schema/事实或无密钥都会回退到本地确定性台词。凭据只向 HTTPS 且主机严格等于 `api.deepseek.com` 的官方端点发送；loopback 不携带 Authorization。

## 构建 Editor

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
  WhiteoutStationEditor Win64 Development `
  'G:\WhiteoutStation-v06-worktree\WhiteoutStation\WhiteoutStation.uproject' `
  -WaitMutex
```

启动 Editor：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  'G:\WhiteoutStation-v06-worktree\WhiteoutStation\WhiteoutStation.uproject'
```

默认地图为 `MVP_StationMap`。

## 回归命令

```powershell
python -X utf8 -m pytest Tools/Agents -q

Push-Location Tools/Rules
python -X utf8 -m pytest test_whiteout_rules.py -q
Pop-Location

python -X utf8 -m pytest Tools/Release/test_v06_release_gates.py -q

& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'G:\WhiteoutStation-v06-worktree\WhiteoutStation\WhiteoutStation.uproject' `
  -unattended -nop4 -nosplash -nullrhi `
  '-ExecCmds=Automation RunTests WhiteoutStation' `
  '-TestExit=Automation Test Queue Empty' `
  '-ReportExportPath=G:\WhiteoutStation-v06-worktree\Artifacts\TestResults\manual-v06'
```

最终确认结果：Agents 43、Rules 30、Release 15、UE Automation 8 / 8、真实输入 2 / 2、Shipping 10 / 10。发布包校验命令与结果记录于 `RELEASE_CHECKLIST_v0.6.md`。

## 角色范围

v0.6 未修改顾衡与叶澄的人物模型、骨骼、材质、动作、动画、AnimBP 或 LookAt。保护清单位于 `PROTECTED_CHARACTER_ASSETS_v0.6.json`；5 个基线 Git 对象在最终源码与发布校验中一致。
