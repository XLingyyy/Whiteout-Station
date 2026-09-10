# v1.7 实施记录

施工依据：`WhiteoutStation_v1.7_A方案_详细施工文档.docx`。基线 `c9b8435`，直接在 main 迭代。

## 已验证基线

- v1.6 源码门禁与密钥扫描通过；照护平衡 Python 测试 5/5。
- 开工记录保存于 `Artifacts/v1.7-evidence/baseline.json`。
- 262 个用户地图、NPC 模型和材质的哈希保存于 `protected-assets-before.json`。这些资产不属于本轮修改范围。

## 当前实现

- 原生 UMG 五页教程，独立阅读状态、模式配图、异步纹理预加载、输入释放门控、Esc 跳过确认、手册重看。
- 教程独立占用 UI 层和暂停请求，返回时仅释放自身锁；角色入口同时门控。
- AP 模型使用 PhaseActionPoints 与 ActionPointsPerPhase，报价用斜纹显示；不会修改规则状态。
- A 配色、分格 AP、人物状态图标、单层教程模糊、响应式边栏。
- 产品版本 1.7.0、独立 v1.7 存档槽；规则 schema 8、AI 配置 schema 9 及 v1.6 数据路径保留。
- 新增源码门禁、教程配图溯源门禁及导入脚本。配图缺失时门禁失败。

## 验证进度

第一批 Editor 编译通过，新增定向自动化 3/3，通过项：ReadingAndInput、PhaseBudgetAndQuote、PreservesV16State。报告位于 `Artifacts/v1.7-evidence/unit-tests/`。

实机配图、完整输入与布局矩阵、Shipping 路线、性能测量和保护资产发布复核尚在实施。此记录不构成 v1.7 发布批准；现用 v1.6 包保留。

陌生玩家理解验收需要实际玩家参与，自动化结果不代替真人结果。

## 工程参考

- [Epic UBackgroundBlur](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UBackgroundBlur)：教程单层模糊与低画质替代画刷。
- [Epic UIOnly 输入模式](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/UWidgetBlueprintLibrary/SetInputMode_UIOnlyEx)：使用 Widget 原生鼠标与键盘事件。
- [Epic FScreenshotRequest](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FScreenshotRequest/RequestScreenshot)：含 Slate UI 捕获完整游戏视口后按真实几何裁剪。
