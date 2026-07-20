# Whiteout Station / 风雪站：断电前夜

UE 5.8 C++ 项目，目标是制作一局 10—15 分钟的社会生存与轻推理 Demo。

## 固定开发环境

- Unreal Engine 5.8
- Windows 64-bit
- C++ 权威规则层，蓝图/UMG 表现层
- Enhanced Input
- 官方 Model Context Protocol 插件（仅开发期）

## 当前范围

v0.1 必须保留 8 AP 闭环、两名 NPC、13 项核心行动、三条成功路线、中段危机、确定性结算、模型故障降级、存档日志和五维复盘。详细范围见 `docs/SCOPE_v0.1.md`。

## 规则回归

在仓库根目录运行：

```powershell
python -X utf8 -m unittest discover -s Tools/Rules -p "test_*.py" -v
python -X utf8 Tools/Rules/run_routes.py
```

规则数据的单一可信来源是 `WhiteoutStation/Content/Rules/WhiteoutStationRules.v0.1.json`。

## 当前可玩灰盒

默认打开 `MVP_StationMap`，运行后可用 WASD 移动、鼠标观察、F 执行当前交互、E 打开证据板、Enter 结算、R 重开。当前灰盒已覆盖五个站区、13 项行动、三条成功路线、中段危机、零 AP 发报窗口、四类结局、事件日志与自动存档。

UE 自动化回归可在编辑器 Session Frontend 中运行 `WhiteoutStation` 测试集，也可用命令行运行同名测试集。

## 工程目录

- `WhiteoutStation/Source/WhiteoutStation`：UE C++ 运行时代码
- `WhiteoutStation/Content/WindStation`：场景、数据、UI、角色、音频和测试资产
- `WhiteoutStation/Content/Rules`：可复现的规则配置
- `Tools/Rules`：脱离编辑器运行的规则模拟与回归
- `docs`：设计、实施清单、范围与进度记录

## 版本管理

`.uasset`、`.umap` 与大型媒体通过 Git LFS 管理；`Binaries`、`Intermediate`、`Saved`、`DerivedDataCache` 和 IDE 临时文件不入库。
