# v0.2 构建与游玩说明

## 直接游玩

Windows Shipping 可执行文件：

`Builds/WhiteoutStation-v0.2-Win64/Windows/WhiteoutStation.exe`

双击后即可开始。开场会在 14 秒内依次说明背景、三个首要目标、8 点行动力预算与操作方式；按 Space 可跳过。

## 操作

| 输入 | 功能 |
|---|---|
| WASD | 移动 |
| 鼠标 | 观察；热点会高亮并显示名称/AP |
| F | 第一次打开行动预览，第二次确认提交 |
| Q | 打开/关闭对话方式菜单 |
| 1—6 | 选择询问、质疑、三类承诺或安抚 |
| E | 打开/关闭证据板 |
| Space | 跳过开场 |
| Enter | 结束本轮并进入结算 |
| R | 开始新一轮 |
| Esc | 暂停并显示继续/退出；Alt+F4 也可直接退出 |

目标是在 8 AP 内修复发电机、校准室外天线并发出求救信号，同时管理三人状态、物资、证据与承诺。查看界面、关闭提示和演出不消耗 AP；行动被拒绝时也不扣 AP 或物资。

## 存档与日志

Shipping 版本将运行数据写入 `%LOCALAPPDATA%/WhiteoutStation/Saved`：

- `SaveGames/WhiteoutStation_Autosave_v0_1.sav`：规则版本保持 v0.1 的兼容自动存档。
- `Logs/WhiteoutStation_EventLog.json`：逐行动事务、AP、危机、结局、评分、模型调用和承诺结算。
- `WhiteoutRuntimeSmoke.png`：仅在传入自动截图参数时生成。

命令行加入 `-WhiteoutContinue` 可从自动存档继续。

## 可选共享表达服务

默认不联网，NPC 使用确定性本地台词。可传入 `-WhiteoutAgentEndpoint=http://127.0.0.1:8765` 连接兼容项目 JSON 协议的本地或受控代理。模型只能改写台词，不能修改回应类型、事实权限、AP、资源或结局；整局硬限制 10 次调用，失败时自动回退本地台词。仓库不包含任何密钥。

## 从源码复现 Shipping 包

项目固定使用 UE 5.8。调整引擎路径后在仓库根目录执行：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
  -project='G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' `
  -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive `
  -archivedirectory='G:\Whiteout Station\Builds\WhiteoutStation-v0.2-Win64' -utf8output
```

构建后复制 `Distribution/README_v0.2.txt` 与 `SourceAssets/ASSET_LICENSES.md` 至归档根目录。完整测试、性能数据、包体和独立包烟测见 `docs/QA_REPORT_v0.2.md` 与 `docs/RELEASE_MANIFEST_v0.2.md`。
