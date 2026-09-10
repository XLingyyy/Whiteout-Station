# v1.7.0 候选记录 · 2026-09-10

本轮已完成实现、Editor 编译、Shipping 候选打包及下列自动化验收。当前状态为候选，未替换现用 v1.6 包；尚未签收的 OS 操作和陌生玩家理解测试列在文末。

## 候选位置

- 运行时源码：`4394e2d067eed49ecd442b52fd7338d758aaef5a`。
- 最终包：`Artifacts/WhiteoutStation-v1.7-Win64-20260910-candidate2/Windows/`。
- 双击启动器：该目录下的 `WhiteoutStation.exe`。
- 包大小约 806 MiB；产品版本 1.7.0，UIRevision=1，TutorialVersion=1，规则 schema 8，AI 配置 schema 9。
- 构建清单：候选根目录的 `build-manifest.json`，仓库副本为 `docs/QA/build-manifest.v1.7.json`。
- 回滚位置及 v1.6 现用包说明继续以 `docs/RELEASE_v1.6_20260910.md` 为准。

## 本轮交付

五页中央教程、首次安全点触发、阅读记录、跳过与手册重看；A 风格 HUD、阶段 AP 格与费用预览；玩家和 NPC 状态卡；聚焦物品白色轮廓；七张带来源记录的真实游戏配图；独立 v1.7 存档槽及读取前内容校验。

回归中修复了损坏存档直接进入 UE 反序列化导致 FName 长度断言的崩溃，以及教程末页按钮文字拆行。旧档仅读，迁移后的新进度写入 v1.7 槽。详细操作见 `BUILD_AND_PLAY_v1.7.md`。

## 验证结果

| 项目 | 结果 | 本机证据（相对 Artifacts/v1.7-evidence） |
| --- | --- | --- |
| Editor / Shipping | 编译、Cook、Stage、Archive 成功，最终 Cook 0 错误、0 警告 | `shipping-final-build.log` |
| 教程/AP/状态/焦点定向自动化 | 5/5 | `unit-tests-final/index.json` |
| v1.6 额度与存档定向自动化 | 3/3 | `save-compat-tests/index.json` |
| Editor 渲染集成 | 33/33，含 30 次重看、完整状态不变、存档损坏与恢复 | `runtime-probe_verified.json` |
| Shipping 真正启动器集成 | 33/33，720p / 1.5 倍文字，含教程重置命令 | `runtime-probe_shipping_720_150.json` |
| 最终候选布局矩阵 | 8/8 配置，每组 11 项；40 张五页截图齐全 | `layout-matrix.json`、`frames/tutorial_*_matrix_*.png` |
| 十条既有玩法路线 | 全部通过；除随机 transaction_id 外，完整 JSON 与 v1.6 基线一致 | `shipping-routes/summary.json`、`shipping-routes/baseline-comparison.json` |
| 配图、源码与包内容门禁 | 0 错误，七张正式纹理运行时可达 | `docs/QA/v1.7_tutorial_assets.json`、集成报告 |
| 受保护资产 | 262/262 与开工记录一致 | `protected-assets-final.json` |

完整玩法路线和存档集成运行于 `5131d44` 首个候选。最终源码 `4394e2d` 仅补充教程按钮单行布局、限制正式包教程日志及操作文档；玩法与存档实现相同，因此保留既有路线结论，最终包另跑全部布局配置和五页操作检查。

布局矩阵为 1280×720 和 1920×1080 各自 1.0/1.25/1.5 倍文字，以及 2560×1440、3440×1440 的 1.0 倍文字。每种配置采集五页；视觉抽查覆盖全部配置及交谈、白色轮廓、AP、玩家与 NPC 状态页，未发现标题截断、图片拉伸或页脚溢出。矩阵集中覆盖教程；各个旧菜单在所有分辨率下的人工遍历未穷举。

## 性能采样

i7-13650HX / 16 GB / RTX 4060 Laptop，1920×1080，文字 1.0，画质组为 3，VSync 关闭，配置记录的 sg.ResolutionQuality=0（引擎默认解析）。每段预热后连续采样至少 30 秒。

| 状态 | 总 GPU P95 ms | Slate UI GPU P95 ms | GameThread P95 ms | Slate DrawWindows P95 ms | 进程内存 MiB 范围 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 游戏 | 10.6780 | 0.0508 | 3.4586 | 0.1224 | 3805.2–3806.7 |
| 教程模糊 | 10.5889 | 0.2162 | 2.6569 | 0.1681 | 3785.0–3799.3 |
| 低画质替代 | 10.5414 | 0.1689 | 2.5932 | 0.1733 | 3760.1–3785.4 |

教程相对游戏的 Slate UI GPU P95 增量为 0.1654 ms，低画质替代为 0.1181 ms，低于施工文档暂定的 2 ms 预警线。暂停降低世界负载，总 GPU 时间下降不证明模糊没有成本。本机同时存在其他前台应用，因此这些数据用于工程预算参考。原始 CSV 和 180 条进程内存采样保存在 `performance/` 与 Editor 集成报告。

## 未签收项目

OS 鼠标命中、长按、双击和点击穿透，真实 Alt-Tab 返回、Tab 焦点、中文 IME 与草稿，以及部分旧档/在途请求/切关卡的逐场景人工操作尚未签收。桌面自动化返回的画面曾与 UE 窗口身份不符，已停止向桌面发送输入；原生 Slate 驱动不能代替这些 OS 结果。

3–5 名陌生玩家的理解测试尚未进行。全部用例的覆盖边界见 `QA/v1.7_acceptance.md`。候选可以试运行，但不宣称施工文档所有 P0/P1 与真人验收完成，保留 v1.6 包和旧存档作为回滚路径。
