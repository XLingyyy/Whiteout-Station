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
- 聚焦物品的白色轮廓使用独立后处理材质，补齐 CustomDepth / Stencil 渲染；没有修改地图、NPC 或用户材质。
- 七张真实配图及五张原始帧保存在 `SourceAssets/UI/v17/Tutorial/`，纹理和六条模式页记录已导入内容资产。T03 的 AP 与费用裁片来自同一帧。
- v1.7 新槽保存 UE 原生序列化内容和 CRC32 尾部校验。读取新槽时先校验完整字节，再交给 UE 反序列化；校验失败返回读档失败，由玩家显式选择旧备份。旧槽保持原格式、只读。

## 验证进度

Editor 编译通过；定向自动化 5/5，包括 ReadingAndInput、PhaseBudgetAndQuote、PreservesV16State、Focus.CollisionAndLease、Status.Permissions。报告位于 `Artifacts/v1.7-evidence/unit-tests-final/`。

渲染回归已经覆盖五页导航、游戏按键隔离、跳过取消、完成释放锁、重看返回手册滚动位置、30 次重看、全量 FWSGameState/StateRevision/角色位置不变，以及实际 AP 报价、取消、提交与通知去重。最终结果以 `runtime-probe_verified.json` 为准。

首轮损坏存档回归复现 UE FName 长度断言：直接将四字节损坏文件交给 LoadGameFromSlot 会在返回失败前崩溃。新增读取前校验，并扩展随机字节翻转、截断和正常文件恢复用例。配图导入同时修正 Python 类名映射及嵌套数组修改未保存的问题。

布局矩阵、Shipping 路线和保护资产发布复核尚在实施。此记录不构成 v1.7 发布批准；现用 v1.6 包保留。

陌生玩家理解验收需要实际玩家参与，自动化结果不代替真人结果。

## 工程参考

- [Epic UBackgroundBlur](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UBackgroundBlur)：教程单层模糊与低画质替代画刷。
- [Epic UIOnly 输入模式](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/UWidgetBlueprintLibrary/SetInputMode_UIOnlyEx)：使用 Widget 原生鼠标与键盘事件。
- [Epic FScreenshotRequest](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FScreenshotRequest/RequestScreenshot)：含 Slate UI 捕获完整游戏视口后按真实几何裁剪。
- [Epic SaveGame 二进制保存与读取](https://dev.epicgames.com/documentation/unreal-engine/saving-and-loading-your-game-in-unreal-engine)：使用 SaveGameToMemory、SaveDataToSlot 和 LoadGameFromMemory，在原生序列化前后实施内容校验。
