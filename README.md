# Whiteout Station / 风雪站：断电前夜

UE 5.8 C++ 项目，目标是制作一局 10—15 分钟的社会生存与轻推理 Demo。

## 固定开发环境

- Unreal Engine 5.8
- Windows 64-bit
- C++ 权威规则层，蓝图/UMG 表现层
- Enhanced Input
- 官方 Model Context Protocol 插件（仅开发期）

## 当前范围

v0.4 已完成全局 ESC 返回、可点击证据板、固定十字准心与细白高亮、统一低遮挡毛玻璃 UI、HUD/对话肖像移除、叶澄夹克、NPC 开场白、四意图与可选玩家输入、离线/在线表达降级链，以及双分辨率基线和 Win64 Shipping 归档。两套指定 Idle 源动画的前伸手臂姿势已由用户接受为已知限制；v0.4 任务清单 19 项和 G0—G5 全部门禁已关闭。详见 `docs/PROGRESS_v0.4.md`。

## 规则回归

在仓库根目录运行：

```powershell
python -X utf8 -m unittest discover -s Tools/Rules -p "test_*.py" -v
python -X utf8 Tools/Rules/run_routes.py
```

规则数据的单一可信来源是 `WhiteoutStation/Content/Rules/WhiteoutStationRules.v0.1.json`。

## v0.4 可玩 Demo

默认打开 `MVP_StationMap`。WASD/鼠标移动观察；看向物体后按 F 打开预览、再次按 F 确认；看向 NPC 按 F 开始对话；E 打开/关闭证据板；Space 跳过开场；Enter 在条件满足后结算；R 重开；Esc 在任何界面返回上一层，游戏态打开暂停。启动参数 `-WhiteoutContinue` 可从自动存档进入。

Windows Shipping 版本位于 `Builds/WhiteoutStation-v0.4-Win64/Windows/WhiteoutStation.exe`。完整启动、操作、存档和复现构建说明见 `docs/BUILD_AND_PLAY_v0.4.md`；验收数据见 `docs/QA_REPORT_v0.4.md`。

UE 自动化回归可在编辑器 Session Frontend 中运行 `WhiteoutStation` 测试集，也可用命令行运行同名测试集。

运行态回归可额外传入 `-WhiteoutAutoRoute=medical|technical|quick -WhiteoutAutoCapture`，自动走完指定路线、结算、导出事件日志并保存 `Saved/WhiteoutRuntimeSmoke.png`。

对话默认使用完全离线的确定性预设。NPC 先开口，玩家显式选择询问、质疑、承诺或安抚，并可输入最多 280 字补充说法。传入 `-WhiteoutAgentEndpoint=http://127.0.0.1:8765` 可连接兼容项目 JSON 协议的表达服务；模型只能改写台词，意图、事实权限和全部游戏状态仍由 C++ 决定。开发烟测服务可运行 `python -X utf8 Tools/Agents/mock_agent_server.py`。

## 工程目录

- `WhiteoutStation/Source/WhiteoutStation`：UE C++ 运行时代码
- `WhiteoutStation/Content/WindStation`：场景、数据、UI、角色、音频和测试资产
- `WhiteoutStation/Content/Rules`：可复现的规则配置
- `Tools/Rules`：脱离编辑器运行的规则模拟与回归
- `docs`：设计、实施清单、范围与进度记录

## 版本管理

`.uasset`、`.umap` 与大型媒体通过 Git LFS 管理；`Binaries`、`Intermediate`、`Saved`、`DerivedDataCache` 和 IDE 临时文件不入库。
