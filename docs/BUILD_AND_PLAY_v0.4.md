# v0.4 构建与游玩说明

## 直接游玩

Windows Shipping：

`Builds/WhiteoutStation-v0.4-Win64/Windows/WhiteoutStation.exe`

双击即可开始。游戏从 08:15 开始，每消耗 1 AP 推进 75 分钟；在 8 AP 内修复两次发电机、校准天线并发出求救信号。默认完全离线运行，不需要 API Key。

## 操作

| 输入 | 功能 |
|---|---|
| WASD | 移动 |
| 鼠标 | 观察；在菜单、证据板和对话中选择 |
| F | 与 NPC 开始对话；查看/确认物体行动 |
| E | 打开/关闭证据板 |
| Space | 跳过开场；跳跃 |
| Enter | 条件满足后结束本轮并进入结算 |
| R | 开始新一轮 |
| Esc | 全局返回上一层；游戏态打开暂停菜单 |
| Alt+F4 | 直接退出 |

对准顾衡或叶澄按 F 后，NPC 先说开场白；再选择询问、质疑、承诺或安抚。可以直接发送确定性意图，也可输入最多 280 个字符补充具体说法。离线时 NPC 使用确定性中文台词，完整流程、规则、AP、资源和结局均不依赖模型。

证据板打开时释放鼠标；左侧五类过滤和证据卡均可点击。Esc 或 E 返回游戏。任意全屏面板打开时，主 HUD 和准心自动隐藏。

## 设置与本地数据

ESC 设置页提供 75°—105° FOV 与主/氛围/效果/反馈四路音量，调整立即生效并写入本机 `GameUserSettings.ini`。

Shipping 本地数据位于 `%LOCALAPPDATA%/WhiteoutStation/Saved`：

- `SaveGames/WhiteoutStation_Autosave_v0_1.sav`：规则 v0.1 兼容自动存档；
- `Logs/WhiteoutStation_EventLog.json`：行动、AP、危机、结局、评分、承诺和模型调用计数；
- `Config/Windows/GameUserSettings.ini`：FOV 与音量；
- `WhiteoutRuntimeSmoke.png`：仅 QA 自动截图参数启用时生成。

命令行加入 `-WhiteoutContinue` 可从自动存档继续。

## 可选 DeepSeek 表达服务

默认关闭。模型固定 `deepseek-v4-flash`，只改写 NPC 对玩家说法的表达；四个规则意图由玩家显式选择，确定性 C++ 规则拥有最终决定权。整局最多 10 次调用；网络、schema、事实白名单或受保护短语校验失败均回退本地台词。

密钥只允许通过 `WHITEOUT_LLM_API_KEY` 环境变量或玩家本机 `LocalConfig/WhiteoutLLM.ini` 注入；同时需设置 `WHITEOUT_LLM_ENABLED=true` 或 `-WhiteoutLLMEnabled=true`。不要把密钥放入游戏目录、命令行、日志或分发文件。

## 从源码复现 Shipping

项目固定使用 UE 5.8。在仓库根目录执行：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
  -project='G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive `
  -archivedirectory='G:\Whiteout Station\Builds\WhiteoutStation-v0.4-Win64' -utf8output
```

构建后复制 `Distribution/README_v0.4.txt` 与 `SourceAssets/ASSET_LICENSES.md` 到归档根目录，再运行：

```powershell
python -X utf8 Tools/Release/run_v04_no_key_smoke.py
python -X utf8 Tools/Release/run_v04_performance.py
python -X utf8 Tools/Release/run_v04_navigation_smoke.py
python -X utf8 Tools/Release/validate_v04_baseline.py
python -X utf8 Tools/Release/validate_release_v04.py
```

最终结果见 `docs/GATE_G5_v0.4.md`、`docs/QA_REPORT_v0.4.md` 和 `docs/RELEASE_MANIFEST_v0.4.md`。
