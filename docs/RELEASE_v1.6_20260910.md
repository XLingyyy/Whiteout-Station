# v1.6 Shipping 更新记录

2026-09-10，Windows 64-bit Shipping，UE 5.8。版本保持 `1.6.0`，规则 schema 8，运行时配置 schema 9。用户完成编辑器检查后授权构建，并要求保留最新地图摆放、覆盖原发行包。

运行时源码与最终地图提交：`72e78cb`。其后的发行文档提交不改变二进制内容。此前修复提交为 `d0d88e5`、`b94ed5d`。

## 启动位置

沿用原路径：

`G:\Whiteout Station\Artifacts\WhiteoutStation-v1.6-Win64-20260908-candidate\Windows\WhiteoutStation.exe`

文件夹名称保留以维持原启动路径，其中内容已更新为本次 2026-09-10 构建。包内 `build-manifest.json` 记录实际构建日期、源码提交和规则版本。

默认离线可玩。在线设置、操作及存档说明见 [构建与使用](BUILD_AND_PLAY_v1.6.md)。本轮没有写入 API Key，也没有改动玩家存档。

## 本次内容

- 保留用户最后调整的地图物品位置，地图文件已单独提交并推送。
- 包含证据图标等比缩放、紧凑行动选择、准确消耗与 10 秒实际反馈，以及结算光标和输入修复。
- 包含暖区立即回温、三人食物分配、一次性协查维修准备、人员与有效储备评分、目标提示、手册和存档迁移。
- 完整规则与 12 AP 规划见 [参数表](Whiteout_Station_v1.6_编辑器修复与重平衡.md)。上一轮 Editor、原生规则与真实对话结果见 [编辑器验收记录](QA/v1.6_editor_rebalance_20260910.md)。

## 本次验证

| 检查 | 结果 |
|---|---|
| BuildCookRun | 成功，退出码 0；Shipping 编译、完整 Cook、Stage、Pak、Archive 完成 |
| 包内容 | v1.6 内容检查 0 错误，包内规则 schema 8 |
| Shipping 路线 | 10/10 符合预期，包含五种基础路线、四条低效选择路线和照护路线 |
| 照护路线 | 12 AP，91.0507 分，S 评级；实际界面按一位小数显示 91.1 |
| 实机启动 | 通过根目录启动器启动真正的 Shipping 程序，加载地图并显示更新后的结算 |
| 结算交互 | 光标显示、鼠标点击暂停返回、连续滚轮查看完整行动记录成功 |
| 用户资产 | 本次构建前后 262 个地图与 NPC 资产哈希一致，最新地图未被覆盖 |

本次复用已通过的 Editor 修复验证，重点检查发行产物；没有把上一轮全部测试重复执行。Shipping 路线使用确定性作者选项，模型调用数为 0；本轮未重复真实 DeepSeek 测试，上一轮定向 7 次自然回复的结果不冒充本次 Shipping 服务测试。

实机输入记录：启动直达结算的测试夹具首次自动化滚轮未观察到位移；打开暂停并鼠标点击返回后，连续滚动能正常查看行动记录。正常结尾、读档与减少动态效果的 PIE 专项已在上一轮通过。本轮未因这一自动化窗口激活现象修改运行时源码。

本地证据目录：`Artifacts/v1.6-release-20260910-evidence/`。

- `shipping-build.log`：构建完整日志。
- `shipping-routes/summary.json` 及每条 `.events.json`：10 条路线的实际结果。
- `shipping-results-scroll.jpg`：真实 Shipping 鼠标滚动后的结算记录。
- `protected-assets-before.json`、`protected-assets-result.json`：本次资产保护基线与复核。

构建使用项目既有 UAT 流程，参照 [Epic Build Operations 文档](https://dev.epicgames.com/documentation/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine)。新包先在临时归档目录验证，再替换原发行目录；从原路径复跑照护路线成功，91.0507 分。

删除替换产生的旧包备份时，自动审批审查返回 `blocked by policy`，删除未执行。旧备份暂留在 `Artifacts/v1.6-replaced-20260910/`，约 0.786 GiB。该备份不参与新版启动；原发行路径已指向新版内容。
