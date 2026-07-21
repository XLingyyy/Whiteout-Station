# v0.2 QA / 验收记录

更新日期：2026-07-21

## 自动化与构建结果

| 层级 | 结果 | 覆盖 |
|---|---:|---|
| 纯 Python 规则回归 | 17 / 17 通过 | 配置、AP/危机、事务原子性、三路线、四结局、评分、承诺、知识隔离、Agent 越权 |
| UE Automation | 6 / 6 通过 | 对话边界、模型预算、AP 流程、知识与 Agent 校验、三路线、事务 |
| 规则冻结校验 | 5 / 5 通过 | v0.1 权威规则和工具哈希未改变 |
| 中文 StringTable 校验 | 通过 | 236 个条目、210 个引用键均有效 |
| UE Editor Win64 Development | 通过 | C++、UHT、编辑器模块和运行时资产加载 |
| Win64 Shipping BuildCookRun | 通过 | 645 个 cook 包（638 个运行时包），Pak/IoStore、stage 与 archive 成功 |

## Shipping 独立包路线矩阵

三条路线均从 `Builds/WhiteoutStation-v0.2-Win64/Windows/WhiteoutStation.exe` 独立启动，以 `-WhiteoutAutoRoute=<route> -WhiteoutAutoCapture` 完成实际执行、结算、截图与事件日志导出。

| 路线 | 结局 | 事件 | 剩余 AP | 分数 | 结果 |
|---|---|---:|---:|---:|---|
| medical | TaskSuccess | 8 | 0 | 76.64 | 通过 |
| technical | TaskSuccess | 8 | 0 | 71.90 | 通过 |
| quick | TaskSuccess | 6 | 2 | 72.06 | 通过 |

发布校验器汇总：`RELEASE VALIDATION v0.2: PASS`。三张 Shipping 结算截图均完成视觉复核，中文路线、分数、AP 与事件时间线显示正确，无缺失 StringTable 条目、开场遮挡或结算演出回跳。

## 视觉基线与发布缺陷回归

- `docs/baseline_v0.2/` 共保留 61 张基线图，覆盖五区、双分辨率 HUD/证据板/结算页、双角色、热点反馈、开场、危机和四类结局。
- 打包版视觉烟测发现并修复两类仅在 Shipping 中暴露的问题：UI StringTable 未被 cook；AutoRoute 完成后开场或结局演出覆盖结果页。
- 最终包显式 cook `/Game/WindStation/UI`，同时在表现文字层保留缺失条目的中文降级；AutoRoute 截图稳定停留于完整结果页。

## 1080p 性能实测

测试场景为室外雪暴最重视角，1920×1080，预热 5 秒后连续采样 15 秒。测试机为 Intel Core i7-13650HX、NVIDIA GeForce RTX 4060 Laptop GPU、16 GB 内存。

| 采样帧 | 平均 FPS | 1% Low FPS | P95 帧时 | P99 帧时 | 最大帧时 |
|---:|---:|---:|---:|---:|---:|
| 1490 | 99.30 | 83.59 | 10.754 ms | 11.473 ms | 13.205 ms |

平均与 1% Low 均高于 60 FPS，性能门禁通过。

## 包体与发布完整性

- 发布归档共 40 个文件、764,172,929 bytes（约 728.8 MiB），低于 2.5 GB 预算。
- 包内随附启动说明、资产许可和三条路线的日志/截图验证证据。
- 关键可执行文件与 Pak/IoStore 哈希见 `docs/RELEASE_MANIFEST_v0.2.md`。

## 盲测准备状态

`docs/playtest_v0.2.md` 已提供 2 分钟目标理解检查、拒绝文案可读性检查和真人试玩问卷模板。当前记录为自动化、开发者烟测与视觉 QA；尚未虚构外部真人样本，后续真实数据应按该模板采集并作为平衡输入。
