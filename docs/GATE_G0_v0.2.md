# G0 视觉基准与资产就位验收记录

验收日期：2026-07-21

## 结论

G0 通过。控制室样板间、可复现资产管线、许可记录、DataAsset 装配与规则回归证据均已落地。

## 视觉与资产证据

- `baseline_v0.2/ControlRoom_01_Entry.png`：1280×720，控制室入口与行动热点。
- `baseline_v0.2/ControlRoom_02_Consoles.png`：1280×720，控制台、座椅及冷暖照明关系。
- `baseline_v0.2/ControlRoom_03_WindowWall.png`：1280×720，控制台阵列与窗侧冷光。
- `SourceAssets/ASSET_LICENSES.md`：Quaternius CC0、Poly Haven CC0、Noto Sans CJK OFL 来源与许可记录。
- `Tools/Assets/`：下载、校验与选定资产清单可复现。
- `/Game/WindStation/Presentation/DA_WS_StationAssembly`：区域到网格、材质、灯光映射；运行时生成 38 个陈设网格与 6 盏数据驱动灯。

## 回归证据

| 检查 | 结果 |
|---|---|
| `python -X utf8 -m unittest discover -s Tools/Rules -p "test_*.py" -v` | 17/17 通过 |
| `python -X utf8 Tools/Release/validate_v02_rule_freeze.py` | 5 个冻结文件校验通过 |
| UE Automation `WhiteoutStation` | 6/6 通过：DialogueBoundary、ModelBudget、APFlow、KnowledgeAndAgentValidation、Routes、Transactions |
| 运行态 `-WhiteoutAutoRoute=medical` | 成功，TaskSuccess，76.64 |
| 运行态 `-WhiteoutAutoRoute=technical` | 成功，TaskSuccess，71.90 |
| 运行态 `-WhiteoutAutoRoute=quick` | 成功，TaskSuccess，72.06 |
| `WhiteoutStationEditor Win64 Development` | 编译通过 |

## 设计铁律核对

- 规则冻结文件未改动；表现层继续通过状态快照与事件委托读取状态。
- 浏览证据板、对话菜单、暂停页与结算页不扣行动点。
- 角色状态仅显示中文等级，不显示内部精确数值。
- 样板间移除了坐标系不匹配的悬空模块与默认棋盘格材质，不改变五区坐标、13 个交互点或碰撞契约。
