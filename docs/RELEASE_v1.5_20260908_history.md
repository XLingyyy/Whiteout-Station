# v1.5 对话历史更新发行记录 · 2026-09-08

运行时代码 `d4d1ba7`，主分支 `main`。候选版本，新增按 NPC 保存原文、关闭重开恢复、读档恢复和跨会话在线记忆。

启动：`G:\Whiteout Station\Artifacts\WhiteoutStation-v1.5-Win64-20260908-d4d1ba7-candidate\Windows\WhiteoutStation.exe`。默认离线模式；在线设置和密钥使用方式不变，发行包不包含测试密钥。

- Win64 Shipping BuildCookRun 成功，155.37 秒，Cook 0 errors / 0 warnings。
- 包内容检查 0 errors，五条离线路线全部通过，结局及评分与上包一致。
- Development 7 项对话自动化全部通过；实际游戏中完成交谈、离开、重新打开，原文完整恢复。
- 新包真实 DeepSeek 3 组、6 条消息全部解析：两名 NPC 的首次问答及跨会话追问共 4 条均为模型表达，无作者替代；另 2 条证明旧提议无法跨会话确认，AP 与承诺数量不变。
- 262 个受保护地图/模型/材质文件与基线一致，包内 80 份文本配置和数据未发现密钥。
- 对话原文保留在当前局的存档中；模型读取最近 6 次问答和更早已聊话题索引。新游戏清空历史，更新前已丢弃的原文无法恢复。

[行为、实机验证和已知回应问题](QA/v1.5_conversation_history_20260908.md)；[新包逐消息证据与元数据](QA/v1.5_history_shipping_20260908.json)。不将局部记忆功能通过等同于整体模型表达质量或完整玩法验收通过。

旧归档 `Artifacts/WhiteoutStation-v1.5-Win64-20260908-9adfe31-candidate` 的删除被自动审批审查拒绝，返回 `blocked by policy`，本次保留。新版归档独立可运行，清理尚未完成。

发行包按现有仓库规则保存在本地 Artifacts，Git 推送源码、测试工具和发行记录。
