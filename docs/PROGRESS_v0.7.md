# Whiteout Station v0.7 - 进度总结

日期：2026-07-28

状态：已完成

分支：`codex/v0.7-evolution`

## 版本结果

v0.7 已形成可从开场、探索、交互、对话、行动结算推进到四类结局的
Windows Shipping 候选包。本轮完成以下核心增量：

- 修复左侧 HUD 文本溢出、交互提示文字与墨刷背景错位；
- 默认准心改为空心圆，瞄准可交互对象时切换为手形，中心误差审计不超过
  1.5 px；
- 补全所有规则行动的场景交互入口；地图缺失的食物分配入口仅在运行时补入，
  未改写用户地图；
- AI 回复升级为严格六字段契约，并在本地权威边界内选择 NPC 移动与反应；
- 为顾衡、叶澄各接入 1 个行走和 5 个反应动画，共 12 个动画资产；
- 保留分阶段对话意向、自由输入、引导手册、状态解释、手动推进黑幕开场和
  四类结局闭环；
- 修复真实键盘操作中对话回复后焦点滞留输入框的问题。

## 用户内容保护

- 用户调整后的关卡
  `/Game/WindStation/World/MVP_StationMap` 未被修改；
- 顾衡、叶澄现有人物模型资产的 Git 对象与 v0.7 开工基线一致；
- `WindStation/AnimeNPC`、`WindStation/Presentation/Characters` 等保护目录
  仅增加了允许清单中的 12 个动画资产；
- 基线与保护清单分别记录在
  `docs/USER_BASELINE_v0.7.json` 和
  `docs/PROTECTED_CHARACTER_ASSETS_v0.7.json`。

## AI 与表演

模型输出固定为：

`npc_line`、`emotion`、`used_action_id`、`referenced_fact_ids`、
`movement_intent`、`reaction_action`。

移动枚举为 `stay`、`step_closer`、`step_back`、`return_to_post`；反应枚举为
`neutral`、`acknowledge`、`consider`、`reassure`、`reject`、`alarmed`。
C++ 继续独占 AP、资源、事实、承诺、任务、评分和结局。移动由本地碰撞检测、
岗位半径、冷却和单步距离约束；验证中的两名 NPC 均按模型选择移动 85 cm，
随后播放 `acknowledge` 反应。

## 验证状态

| 验证层 | 结果 |
|---|---:|
| Agent Python 测试 | 48 passed |
| Rules Python 测试 | 30 passed |
| Release Python 测试 | 18 passed |
| UE Automation | 8 passed |
| Shipping 真实输入场景 | 2 passed |
| Shipping 路线、降级、历史与表演探针 | 12 passed |
| 源码门禁、LFS、保护目录、敏感信息扫描 | PASS |

真实输入证据覆盖逐句开场、帮助页、证据板、交互瞄准、行动预览与确认、
阶段意向、中文自由输入、退出对话、结算、结局动画和结果页。

## 发布候选

- 根目录：
  `G:\WhiteoutStation-v07-worktree\Builds\WhiteoutStation-v0.7-Win64-20260728T150103Z-0e96ad1b-release`
- Run ID：`20260728T150103Z-0e96ad1b-release`
- 源码提交：`0e96ad1b22c4ab894e5c5f5e7dc9f1c103f38bda`
- 源码树：`0678c323d73213f034944c16bb30c686aca7886d`
- 规模：151 个文件，868,619,705 bytes（828.38 MiB）
- 分发等级：`local_review_only`

## 当前边界

- 未进行独立外部玩家可用性样本测试；
- 在线回复依赖 DeepSeek 服务可用性，缺少密钥或请求失败时会确定性降级；
- Windows 可执行文件未做代码签名；
- 叶澄 Noanoa 发型的再分发许可尚未取得书面确认，当前包只能在本机用于私人
  开发和评审，禁止上传或转发。
