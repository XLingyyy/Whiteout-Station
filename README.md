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

## 工程目录

- `WhiteoutStation/Source/WhiteoutStation`：UE C++ 运行时代码
- `WhiteoutStation/Content/WindStation`：场景、数据、UI、角色、音频和测试资产
- `WhiteoutStation/Content/Rules`：可复现的规则配置
- `Tools/Rules`：脱离编辑器运行的规则模拟与回归
- `docs`：设计、实施清单、范围与进度记录

## 版本管理

`.uasset`、`.umap` 与大型媒体通过 Git LFS 管理；`Binaries`、`Intermediate`、`Saved`、`DerivedDataCache` 和 IDE 临时文件不入库。
