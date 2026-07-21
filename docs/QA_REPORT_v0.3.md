# v0.3 QA / 验收记录

更新日期：2026-07-22

## 最终状态

v0.3 全部 26 项任务与 G0—G5 六个门禁均已完成。验证覆盖源码规则、编辑器自动化、实际渲染、交互体验、Shipping 独立包、无密钥降级、性能、包体、资产许可与 Git 凭据安全。

## 自动化与构建结果

| 层级 | 结果 | 覆盖 |
|---|---:|---|
| Python 规则回归 | 17 / 17 | AP/危机、事务原子性、三路线、四结局、评分、承诺、知识隔离、Agent 越权 |
| UE Automation | 6 / 6 | 对话边界、模型预算、AP 流程、知识/Agent 校验、路线、事务 |
| 规则冻结 | 5 / 5 | v0.1 权威规则与工具哈希未改变 |
| StringTable | PASS | 255 条目、210 个引用键 |
| UI/场景基线 | 92 / 92 | 46 个视角、双分辨率、5/5 反馈参考对应视角 |
| Editor Development | PASS | C++、UHT、编辑器模块与运行时资产 |
| Win64 Shipping BuildCookRun | PASS | 694 个 Cook 包、687 个运行时包、Pak/IoStore、Stage、Archive |
| 当前索引与可推送历史密钥扫描 | PASS | 源码、配置、文档、OOXML、分支、远端跟踪分支与标签 |

最终 UE Automation 报告为 6 succeeded、0 succeeded with warnings、0 failed、0 not run；六个测试均为 `Success` 且单项 warning/error 为 0。

## Shipping 无密钥路线矩阵

三条路线均从最终的 `Builds/WhiteoutStation-v0.3-Win64/Windows/WhiteoutStation.exe` 独立启动。启动器仅向子进程传递去除 `WHITEOUT_LLM_API_KEY`、去除 `WHITEOUT_LLM_ENABLED` 且显式 `-WhiteoutLLMEnabled=false` 的环境；每次启动前清除旧 QA 输出并校验新文件时间戳，杜绝复用旧证据。

| 路线 | 结局 | 事件 | AP | 分数 | `model_calls` | 截图 |
|---|---|---:|---:|---:|---:|---|
| medical | TaskSuccess | 8 | 0 | 76.64 | 0 | 1280×720 PASS |
| technical | TaskSuccess | 8 | 0 | 71.90 | 0 | 1280×720 PASS |
| quick | TaskSuccess | 6 | 2 | 72.06 | 0 | 1280×720 PASS |

路线分数与 v0.2、G1、G2、G3、G4 完全一致；危机均只触发一次，medical 的承诺正常兑现。截图人工复核未发现中文缺字、状态/评分错位、时间线裁切或演出覆盖结果页。

## 视觉与体验回归

- G1：HUD、ESC、证据板、焦点卡、行动卡、toast、开场与结算迁移到统一 v0.3 风格；5 张参考 UI 对应视角完整。
- G2：F 进入对话、轮盘、自由文本、本地词典/模型边界与 NPC 近距 LookAt 完整。
- G3：10 张场景问题图清零，65/65 碰撞、41/41 贴地、3/3 通道、13/13 热点和双方角色审计通过。
- G4：设置跨进程持久化；时钟 08:15—18:15；跳跃实测 43.07 cm 且低顶无重叠；3 组灯光对照通过。
- 最终基线：46 个视角中 UI 26、场景 10、角色 4、LookAt 3、灯光 3；两档分辨率各 46 张。

## 1080p Shipping 性能

测试机：Intel Core i7-13650HX、NVIDIA GeForce RTX 4060 Laptop GPU、16 GB 内存。测试场景为室外暴雪视角，1920×1080，Shipping 离屏实渲染，预热 5 秒后采样 15 秒。

| 采样帧 | 平均 FPS | 1% Low FPS | P95 帧时 | P99 帧时 | 最大帧时 |
|---:|---:|---:|---:|---:|---:|
| 1522 | 101.40 | 84.42 | 10.508 ms | 11.000 ms | 16.336 ms |

平均 FPS 与 1% Low 均高于 60 FPS 门槛，性能门禁通过。结果由 Shipping 进程直接写入 `WhiteoutPerformance.json`，不依赖 Shipping 日志开关；仓库证据为 `docs/evidence_v0.3/g5_shipping/performance_1080p.json`。

## 包体与发布完整性

- 归档路径：`Builds/WhiteoutStation-v0.3-Win64`；42 个文件，774,272,463 bytes（约 738.4 MiB），低于 2.5 GB。
- 包内附 `README_v0.3.txt`、`ASSET_LICENSES.md` 与 `Validation/` 八个无密钥/性能证据文件。
- 发布配置为 ProjectVersion 0.3.0；权威规则版本仍为 0.1.0，兼容存档版本仍为 0.1.0。
- 默认 Agent 配置固定模型与端点但关闭在线路径，且不含 `api_key`、authorization、password 或 token 字段。
- 主可执行文件与 Pak/IoStore SHA-256 见 `docs/RELEASE_MANIFEST_v0.3.md`。

UE 5.8 在 Win64 Cook 初始化时仍会报告未安装的非目标平台 SDK，以及实验性编辑器工具集的启动提示；Win64 SDK 为 VALID，这些信息未进入项目测试失败/警告计数，也不影响 Cook、Archive 或 Shipping 运行。

## 模型路径说明

指定模型仍为 `deepseek-v4-flash`。由于没有注入轮换后的安全凭据，G0 的真实提供方探针结论保持 `SKIPPED_NO_SAFE_CREDENTIAL`，未使用反馈文档中的旧密钥，也未静默换模型。Mock 在线路径、30/30 中文意图集、无提供方降级与最终三条无密钥 Shipping 路线均已通过；模型从未获得规则状态写权限。
