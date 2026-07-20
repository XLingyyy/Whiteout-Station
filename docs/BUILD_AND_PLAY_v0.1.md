# v0.1 构建与游玩说明

## 直接游玩

Windows Shipping 可执行文件：

`Builds/WhiteoutStation-v0.1-Win64/Windows/WhiteoutStation.exe`

双击即可开始新一局。Demo 使用键盘和鼠标，目标是在 8 AP 内稳定发电机、校准天线并发出救援信号，同时处理两名队员、物资与信息责任。

## 操作

| 输入 | 功能 |
|---|---|
| WASD | 移动 |
| 鼠标 | 观察 |
| F | 执行准星指向的交互 |
| Q | 切换询问、质疑、安抚与三类承诺 |
| E | 打开/关闭证据板 |
| C | 读取当前自动存档 |
| Enter | 在发报或行动结束后进入结算 |
| R | 开始新一局 |

HUD 左侧显示 AP、设备进度、物资、队员状态和证据数。每次有效行动会自动存档；行动被拒绝时不会扣 AP 或消耗物资。

## 存档与日志

Shipping 版本将运行数据写入：

`%LOCALAPPDATA%/WhiteoutStation/Saved`

- `SaveGames/WhiteoutStation_Autosave_v0_1.sav`：自动存档。
- `Logs/WhiteoutStation_EventLog.json`：逐行动事务、AP、危机、结局、评分、模型调用和承诺结算。
- `WhiteoutRuntimeSmoke.png`：仅在传入自动截图参数时生成。

命令行加入 `-WhiteoutContinue` 可从自动存档继续。

## 可选共享表达服务

默认不联网，NPC 使用确定性本地台词。可用以下参数连接兼容项目 JSON 协议的本地或受控代理：

```powershell
WhiteoutStation.exe -WhiteoutAgentEndpoint=http://127.0.0.1:8765
```

模型只可改写台词，不能修改回应类型、事实权限、AP、资源或结局；整局硬限制 10 次调用，失败时自动回退本地台词。仓库不包含任何密钥。

## 从源码复现 Shipping 包

项目固定使用 UE 5.8。调整引擎路径后在仓库根目录执行：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
  -project='G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive `
  -archivedirectory='G:\Whiteout Station\Builds\WhiteoutStation-v0.1-Win64' -utf8output
```

规则测试、路线矩阵和独立包验收见 `docs/QA_REPORT_v0.1.md`。第三方资产来源与 CC0 记录见 `SourceAssets/ASSET_LICENSES.md`。
