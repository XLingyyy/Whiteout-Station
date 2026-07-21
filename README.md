# Whiteout Station / 风雪站：断电前夜

UE 5.8 C++ 项目，目标是制作一局 10—15 分钟的社会生存与轻推理 Demo。

## 固定开发环境

- Unreal Engine 5.8
- Windows 64-bit
- C++ 权威规则层，蓝图/UMG 表现层
- Enhanced Input
- 官方 Model Context Protocol 插件（仅开发期）

## 当前范围

v0.2 已在不改规则的前提下完成写实五区场景、两名可动画 NPC、全中文 UMG、两步行动确认、13 热点高亮、分层音频、开场/危机/四结局演出与五维复盘。详细范围见 `docs/SCOPE_v0.2.md`。

## 规则回归

在仓库根目录运行：

```powershell
python -X utf8 -m unittest discover -s Tools/Rules -p "test_*.py" -v
python -X utf8 Tools/Rules/run_routes.py
```

规则数据的单一可信来源是 `WhiteoutStation/Content/Rules/WhiteoutStationRules.v0.1.json`。

## v0.2 可玩 Demo

默认打开 `MVP_StationMap`。WASD/鼠标移动观察；看向热点后按 F 打开预览、再次按 F 确认；Q 打开六种对话方式，E 打开证据板，Space 跳过开场，Enter 结算，R 重开，Esc 暂停/退出。启动参数 `-WhiteoutContinue` 可直接从自动存档进入。

Windows Shipping 版本位于 `Builds/WhiteoutStation-v0.2-Win64/Windows/WhiteoutStation.exe`。完整启动、操作、存档和复现构建说明见 `docs/BUILD_AND_PLAY_v0.2.md`；验收数据见 `docs/QA_REPORT_v0.2.md`。

UE 自动化回归可在编辑器 Session Frontend 中运行 `WhiteoutStation` 测试集，也可用命令行运行同名测试集。

运行态回归可额外传入 `-WhiteoutAutoRoute=medical|technical|quick -WhiteoutAutoCapture`，自动走完指定路线、结算、导出事件日志并保存 `Saved/WhiteoutRuntimeSmoke.png`。

对话默认使用完全离线的确定性预设。传入 `-WhiteoutAgentEndpoint=http://127.0.0.1:8765` 可连接兼容项目 JSON 协议的本地共享表达服务；模型只能改写台词，回应类型、事实权限和全部游戏状态仍由 C++ 决定。开发烟测服务可运行 `python -X utf8 Tools/Agents/mock_agent_server.py`。

## 工程目录

- `WhiteoutStation/Source/WhiteoutStation`：UE C++ 运行时代码
- `WhiteoutStation/Content/WindStation`：场景、数据、UI、角色、音频和测试资产
- `WhiteoutStation/Content/Rules`：可复现的规则配置
- `Tools/Rules`：脱离编辑器运行的规则模拟与回归
- `docs`：设计、实施清单、范围与进度记录

## 版本管理

`.uasset`、`.umap` 与大型媒体通过 Git LFS 管理；`Binaries`、`Intermediate`、`Saved`、`DerivedDataCache` 和 IDE 临时文件不入库。
