# v1.5 实施记录

施工依据：本地 `Whiteout_Station_v1.5_迭代施工指导.docx`。接手基线为 `ae04a46`，直接在 main 开发。

## 当前进度

- WP0：读取完整施工指导；受保护地图及 NPC 共 262 个文件的 SHA-256 已记录到本机 `.codex_tmp/v15_protected_assets.json`。
- WP1 部分完成：修复发电机部分维修状态误判完成；计划句不再因“把发电机修好”被拒绝。完成阈值取规则配置。
- WP1 部分完成：会话账本记录 PaidAP、正向奖励及行为键；两名 NPC 的免费命令追问均产生负向后果，相同会话中的同类同目标命令仅结算一次。承诺记录不受正向奖励上限阻断。
- 统一规范意图、离线作者内容、两阶段在线协议、HUD、输入隔离、迁移和 Shipping 尚未完成；当前仍为开发中的 v1.4 运行配置，不能按 v1.5 发布。

## 验证证据

- 修改前失败：`Saved/AutomationReports/V15BaselineFailure` 复现部分维修误判；`V15EconomyFailure` 复现两名 NPC 的免费命令无后果，共 6 条失败断言。
- 修改后：Editor Win64 Development 编译成功；`Saved/AutomationReports/V15WP1` 下 UE 对话测试 38/38 通过。
- Python：`Tools/Dialogue/test_roleplay_content.py` 与 `Tools/Release/test_v14_dialogue_release_gates.py` 共 45/45 通过。
- 这些测试覆盖首批规则修改，不代表 v1.5 整体验收。真实模型、图形输入法、多分辨率、八人盲测及新 Shipping 包尚未验收。

## 清理状态

已核对旧构建路径与运行进程，计划保留最新已验证的 v1.4 归档，清理八份旧归档及重复 StagedBuilds。自动审批以 `blocked by policy` 拒绝删除命令，未提供具体原因；没有删除文件。接手时磁盘可用约 32.8 GiB。

## 接续工作

按指导的 WP1–WP6 顺序完成剩余合同，不把旧轮盘换皮视为完成。每个工作包单独验证、提交和推送。用户提供的四份未跟踪施工 DOCX 保留为本地输入；API Key 不写入源码、日志、存档或发布包。
