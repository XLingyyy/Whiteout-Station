# v1.5 实施记录

施工依据：本地 `Whiteout_Station_v1.5_迭代施工指导.docx`。接手基线为 `ae04a46`，直接在 main 开发。

## 当前进度

- WP0：读取完整施工指导；受保护地图及 NPC 共 262 个文件的 SHA-256 已记录到本机 `.codex_tmp/v15_protected_assets.json`。
- WP1 部分完成：修复发电机部分维修状态误判完成；计划句不再因“把发电机修好”被拒绝。完成阈值取规则配置。
- WP1 部分完成：会话账本记录 PaidAP、正向奖励及行为键；两名 NPC 的免费命令追问均产生负向后果，相同会话中的同类同目标命令仅结算一次。承诺记录不受正向奖励上限阻断。
- 已接入规范意图、14 条离线选项、29 条作者台词及 7 个受权限控制的事实片段；离线提交经过原事务并绕过 HTTP。
- 已接入在线 A 意图 / B 受控表达，待确认承诺、本地事实片段展开、状态修订与会话失效检查。失败仅允许匹配的作者台词回退。
- 已替换旧对话控件为滚动记录、多行输入及五项分页选项；新增配置 AI / 切换离线入口。
- 已接入自我状态卡和单 NPC 权限视图；中心射线、300/340 cm 距离、获取 / 丢失延时及交谈目标弱引用。
- 已加入 schema 8 / bounded_roleplay_v5 运行配置和 v1.5 存档槽，保留 v1.4–v1.1 迁移。
- 当前为开发检查点，图形交互、真实模型基准、五条离线路线、输入法、多分辨率与 Shipping 仍需验证，不能视为完成发布。

## 验证证据

- 修改前失败：`Saved/AutomationReports/V15BaselineFailure` 复现部分维修误判；`V15EconomyFailure` 复现两名 NPC 的免费命令无后果，共 6 条失败断言。
- 修改后：Editor Win64 Development 编译成功；`Saved/AutomationReports/V15WP1` 下 UE 对话测试 38/38 通过。
- Python：`Tools/Dialogue/test_roleplay_content.py` 与 `Tools/Release/test_v14_dialogue_release_gates.py` 共 45/45 通过。
- 这些测试覆盖首批规则修改，不代表 v1.5 整体验收。真实模型、图形输入法、多分辨率、八人盲测及新 Shipping 包尚未验收。

- 新增验证：`V15StatusIntegrated` 共 42 项，41 通过，1 项测试夹具占位符替换导致无效 JSON。修正后 `V15ClaimsFixed` 的对应测试 1/1 通过；未重复运行其余已通过项。
- `Tools/Dialogue/test_dialogue_v15.py` 2/2 通过；当前 Editor 编译成功。
- 真实 DeepSeek `/models` 确认密钥可用。默认思考模式耗尽 256 token 导致空正文，正在修正请求参数；未据此宣称在线模式验收。

## 清理状态

已核对旧构建路径与运行进程，计划保留最新已验证的 v1.4 归档，清理八份旧归档及重复 StagedBuilds。自动审批以 `blocked by policy` 拒绝删除命令，未提供具体原因；没有删除文件。接手时磁盘可用约 32.8 GiB。

## 接续工作

按指导的 WP1–WP6 顺序完成剩余合同，不把旧轮盘换皮视为完成。每个工作包单独验证、提交和推送。用户提供的四份未跟踪施工 DOCX 保留为本地输入；API Key 不写入源码、日志、存档或发布包。
