# v1.5 Win64 发行包更新 · 2026-09-08

状态：候选发行包。运行时代码提交：`9adfe31`，主分支 `main`。本次仅更新发行产物和验证记录，不提升为完整玩法验收通过。

启动位置：`G:\Whiteout Station\Artifacts\WhiteoutStation-v1.5-Win64-20260908-9adfe31-candidate\Windows\WhiteoutStation.exe`。

默认使用离线作者对话。在线模式继续保留上一轮报告中的模型限制；发行包不包含测试密钥。完整目录应一起保留，不能只复制启动 EXE。

## 构建与回归

- 使用 UE 5.8 的 BuildCookRun，完整执行 Win64 Shipping Build、Cook、Stage、Pak、Archive。BuildCookRun 用时 199.44 秒，UAT 退出码 0；Cook 1,100 个资源，0 errors、0 warnings。流程参考 [Epic 构建与打包说明](https://dev.epicgames.com/documentation/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine)。
- 包内 v1.5 运行配置和九份对话数据验证：0 errors。
- 新包五条离线路线全部成功，模型调用均为 0：医疗 74.70／TaskSuccess、技术 71.44／TaskSuccess、风险 58.94／CostUncontrolled、等待 36.92／SurvivalWait、失控 42.16／TotalCollapse。
- 去掉 NullRHI，以 RenderOffscreen 运行新包医疗路线，退出码 0。这是渲染启动冒烟测试，不代表界面布局或输入法人工验收。
- 新包真实 DeepSeek 补测 6 组、14 条消息：13 条通过协议解析，6 条正常提交；其中 2 条使用固定／混合恢复。覆盖施压、多问题、改地点改阶段、第三轮混合消息确认收尾、取消和已知诊断条件下的替代方案。
- 取消后的多余“确认”有一次解析失败，未扣 AP、未登记承诺，保留失败结果。该批未发现未提交消息扣 AP、擅自维修／治疗、药品消耗或生成继电器。不把恢复或安全拒绝计作纯模型表达成功。
- 262 个受保护地图／NPC 模型及材质与基线一致；密钥扫描通过。

## 清理

新包完成构建、包内容检查及回归后，删除以下三个旧归档，共 406 个文件。实测可用空间增加 **2.36 GiB**，清理后 G 盘可用 **31.64 GiB**：

- `Artifacts/WhiteoutStation-v1.4-Win64-20260905T105713Z-71bd524e-final`
- `Artifacts/WhiteoutStation-v1.5-Win64-20260906-8ba299e-candidate`
- `Artifacts/WhiteoutStation-v1.5-Win64-20260907-8faee88-candidate`

仅删除以上明确列出的独立旧归档。保留最新包、源码、用户资产和测试证据。

## 证据

- [包内数据、源码版本及清理清单](QA/v1.5_shipping_20260908.json)
- [五条离线路线](../Artifacts/v1.5-evidence/shipping-9adfe31-routes/summary.json)
- [真实模型逐消息结果](../Artifacts/v1.5-evidence/shipping-9adfe31-online/results.json)
- [清理前后空间与删除路径](../Artifacts/v1.5-evidence/cleanup-20260908.json)
- [前一轮四维验证报告与已知限制](QA/v1.5_semantic_revision_results_20260908.md)

发行包保存在本地 Artifacts，按仓库现有规则不纳入 Git；主分支推送源码、测试工具及发行记录。
