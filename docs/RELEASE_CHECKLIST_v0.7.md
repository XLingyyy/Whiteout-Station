# Whiteout Station v0.7 - 发布检查清单

日期：2026-07-28

发布等级：`local_review_only`

## 源码与用户内容

- [x] 分支为 `codex/v0.7-evolution`
- [x] Shipping 源码提交已推送
- [x] 构建前工作树干净
- [x] 用户地图与 v0.7 开工基线一致
- [x] 顾衡、叶澄模型资产与开工基线一致
- [x] 保护目录只含 12 个允许新增的动画资产
- [x] Git LFS 指针与对象完整
- [x] 已跟踪文件和完整 Git 历史敏感信息扫描通过
- [x] API key 未写入源码、配置、日志、证据或发布包

## 功能

- [x] 左侧面板文字不溢出
- [x] 空心圆准心位于屏幕中心
- [x] 聚焦可交互对象时准心切换为手形
- [x] 交互提示文字与墨刷背景对齐
- [x] 关卡中所有行动均有交互入口
- [x] 食物分配入口采用运行时缺失补全，未修改用户地图
- [x] 黑幕开场逐句手动推进
- [x] 对话意向按阶段开放
- [x] 自由文本输入与纯键盘退出可用
- [x] 生存手册、证据板和状态解释可用
- [x] 四类结局闭环可达

## AI 与 NPC 表演

- [x] 严格六字段响应校验
- [x] 移动与反应仅接受固定枚举
- [x] AP、资源、事实、承诺、任务、评分和结局保持本地权威
- [x] 缺少密钥、显式离线和网络故障均确定性降级
- [x] 对话历史按 NPC 连续传递
- [x] 移动受碰撞、岗位半径、冷却和单步距离约束
- [x] 顾衡、叶澄行走与反应动画已接入
- [x] 两名 NPC 的 85 cm 移动和反应截图已纳入包内证据

## 测试与构建

- [x] Agent 测试 48/48
- [x] Rules 测试 30/30
- [x] Release 测试 18/18
- [x] UE Automation 8/8
- [x] 真实输入烟测 2/2
- [x] Shipping 路线、降级、历史与表演探针 12/12
- [x] UE 5.8 Build/Cook/Stage/Pak/IoStore/Archive 成功
- [x] 发布包门禁验证通过
- [x] 发布包 151 个文件均受最终清单约束

## 发布物

- [x] `README_v0.7.txt`
- [x] `ASSET_LICENSES.md`
- [x] `Validation/gate_manifest.json`
- [x] `Validation/InputSmoke`
- [x] `Validation/ShippingSmoke`
- [x] 启动器与 Shipping 二进制 SHA-256 已记录
- [x] PAK/UCAS/UTOC SHA-256 已记录

## 法务边界

- [x] 包根目录标记 `LOCAL REVIEW BUILD - DO NOT REDISTRIBUTE`
- [x] `distribution_class` 固定为 `local_review_only`
- [x] Noanoa 发型许可风险已写入 README、许可清单和发布文档
- [ ] 取得 Noanoa 发型嵌入及再分发书面许可，或替换该发型
- [ ] 完成 Windows 代码签名

未完成的两项不影响本机私人开发评审，会阻止当前候选包公开、商业或跨设备
分发。

## 最终候选

`G:\WhiteoutStation-v07-worktree\Builds\WhiteoutStation-v0.7-Win64-20260728T150103Z-0e96ad1b-release`

Run ID：`20260728T150103Z-0e96ad1b-release`

源码提交：`0e96ad1b22c4ab894e5c5f5e7dc9f1c103f38bda`
