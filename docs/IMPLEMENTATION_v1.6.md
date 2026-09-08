# v1.6 实施记录

基线 main `811f822`。依据本地《Whiteout_Station_v1.6_迭代实现方案》实施，直接在 main 提交和推送。用户输入 DOCX 保留在本地，不随源码提交。

## 产品口径

开发初值：每名 NPC 每局 10 轮；正常澄清计轮；有效提议的纯确认／取消不计轮。普通消息最多两次请求，关键回复最多三次；总时限初值分别 10 秒／15 秒，失败不自动重试。保留现有实际动作与成本。

## 当前状态

- 已完整读取方案，核对其参考快照与当前 main 一致。
- 已记录地图与 NPC 模型／材质共 262 个文件的保护基线：本地 `.codex_tmp/v16_protected_assets.json`。
- 构建前清理已检查三个无依赖旧产物：Builds 中 v1.1 包、Artifacts 中 9adfe31 包、Saved/StagedBuilds。删除命令被自动审批审查以 `blocked by policy` 拒绝，未删除；当时可用约 30.8 GiB。保留 d4d1ba7 的 v1.5 包供回退。
- 已实现整句自然输出、按目标授权上下文、关键全文核查、当前治疗状态、独立额度账本与旧存档迁移。旧实现的额度／AP／刷关系回归夹具先记录失败，新增三项 v1.6 核心测试全部通过。
- 更新旧额度断言、补充混合提议失败回滚后，完整 UE 自动化 71/71 通过（V16IntegrationFinal）。五条 Editor 离线路线全部完成且结局正确；医疗／技术／风险／等待／失控评分依次为 74.38、71.12、58.94、36.92、42.16。
- 真实 DeepSeek 对话集扩展为 14 链，包含错误旧历史纠正与实际临时支持行动。前两批失败保留；已修复读档后意图历史轮号、诊断披露后的条件预览、完整治疗后态度以及转述／承诺主体。最终批与 Shipping 复测单独统计。
- 实机通过 computer-use 验证：交谈 10/10→9/10，行动力仍为 4/4，关窗重开后历史与 9/10 额度保留，无重复台词。截图本地存于 Artifacts/v1.6-evidence/screenshots。
- 陌生玩家盲测尚未执行；自动化与真实提供商测试不能替代八名真人评分。

## 工程参考

- [Epic BuildCookRun 构建阶段](https://dev.epicgames.com/documentation/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine)：沿用已有编译、Cook、Stage、Pak、Archive 流程。
- [DeepSeek JSON 输出](https://api-docs.deepseek.com/guides/json_mode/)及[思考模式](https://api-docs.deepseek.com/guides/thinking_mode/)：设置 JSON 格式和独立输出预算；JSON 合法性与台词事实核查分开。
- [Generative Agents](https://arxiv.org/abs/2304.03442)：参考按来源记录记忆、按当前情境检索的设计；本工程继续使用现有本地资料与有限原文历史。
