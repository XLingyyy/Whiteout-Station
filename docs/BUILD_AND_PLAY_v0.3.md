# v0.3 构建与游玩说明

## 直接游玩

Windows Shipping 可执行文件：

`Builds/WhiteoutStation-v0.3-Win64/Windows/WhiteoutStation.exe`

双击即可开始。游戏从 08:15 开始，每消耗 1 AP 推进 75 分钟；在 8 AP 内修复发电机、校准天线并发出求救信号。默认完全离线运行，不需要 API Key。

## 操作

| 输入 | 功能 |
|---|---|
| WASD | 移动 |
| 鼠标 | 观察；在菜单、对话轮盘与设置页中选择 |
| F | 与 NPC 开始对话；查看/确认物体行动 |
| E | 打开/关闭证据板 |
| Space | 跳过开场；跳跃 |
| Enter | 结束本轮并进入结算 |
| R | 开始新一轮 |
| Esc | 暂停；进入设置、操作说明、重开或退出 |
| Alt+F4 | 直接退出 |

对准顾衡或叶澄按 F 后，可选择询问、质疑、承诺、安抚或自由输入。离线时自由输入使用本地意图词典，NPC 使用确定性中文台词；规则、AP、资源和结局不依赖模型。

## 设置与本地数据

ESC 设置页提供 75°—105° FOV 与主/氛围/效果/反馈四路音量，调整立即生效并写入本机 `GameUserSettings.ini`。

Shipping 版本的存档、日志和本地设置位于 `%LOCALAPPDATA%/WhiteoutStation/Saved`：

- `SaveGames/WhiteoutStation_Autosave_v0_1.sav`：保持规则 v0.1 的兼容自动存档；
- `Logs/WhiteoutStation_EventLog.json`：行动事务、AP、危机、结局、评分、承诺与模型调用计数；
- `Config/Windows/GameUserSettings.ini`：FOV 与音量设置；
- `WhiteoutRuntimeSmoke.png`：仅 QA 自动截图参数启用时生成。

命令行加入 `-WhiteoutContinue` 可从自动存档继续。

## 可选 DeepSeek 表达服务

默认关闭。指定模型固定为 `deepseek-v4-flash`，仅负责意图分类与 NPC 措辞，确定性 C++ 规则拥有最终决定权；整局最多 10 次调用，任何网络、schema、白名单或事实校验失败都会回退本地路径。

密钥只允许通过 `WHITEOUT_LLM_API_KEY` 环境变量或玩家本机 `LocalConfig/WhiteoutLLM.ini` 注入；同时需用 `WHITEOUT_LLM_ENABLED=true` 或 `-WhiteoutLLMEnabled=true` 显式开启。不要把密钥放入游戏目录、命令行、日志或可分发文件。

## 从源码复现 Shipping 包

项目固定使用 UE 5.8。调整引擎路径后，在仓库根目录执行：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
  -project='G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive `
  -archivedirectory='G:\Whiteout Station\Builds\WhiteoutStation-v0.3-Win64' -utf8output
```

构建完成后，把 `Distribution/README_v0.3.txt` 与 `SourceAssets/ASSET_LICENSES.md` 复制到归档根目录。再在明确清空 `WHITEOUT_LLM_API_KEY`、`WHITEOUT_LLM_ENABLED` 的子进程环境中分别运行 `medical`、`technical`、`quick` 三条 `-WhiteoutAutoRoute` 并保留自动截图。

最终回归、包体、哈希与无密钥独立包结果见 `docs/GATE_G5_v0.3.md`、`docs/QA_REPORT_v0.3.md` 和 `docs/RELEASE_MANIFEST_v0.3.md`。
