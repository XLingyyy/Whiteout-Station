# Whiteout Station v0.8 - 发布检查清单

日期：2026-07-29

发布等级：`local_review_only`

## 源码与用户内容

- [x] 分支为 `codex/v0.8-evolution`
- [x] Shipping 源码提交已推送
- [x] 构建前工作树干净
- [x] 用户地图与 v0.8 开工基线一致
- [x] 顾衡、叶澄源模型和人物绑定与开工基线一致
- [x] 保护目录只含 14 个允许新增的动画
- [x] Git LFS 指针与对象完整
- [x] 已跟踪文件和完整 Git 历史敏感信息扫描通过
- [x] API key 未写入源码、配置、日志、证据或发布包

## UI、叙事与状态

- [x] 顶部警报文字不溢出且不与目标面板重叠
- [x] 小空心圆准心位于屏幕中心
- [x] 聚焦可交互对象时准心切换为手形
- [x] 交互提示文字与墨刷背景对齐
- [x] 面板打开和关闭时鼠标重置到视口中心
- [x] 六次真实输入鼠标回中误差均为 0 px
- [x] 十句黑幕开场逐句手动推进并正常淡出
- [x] 开场说明 NPC 职责、合作原因和求救目标
- [x] 对话意向按阶段开放
- [x] 中文自由文本输入与纯键盘退出可用
- [x] 人物指标和阈值统一为 0—10
- [x] 生存手册解释每项状态和改善方式
- [x] 新墙面、地面材质已接入且不覆盖用户自定义材质
- [x] 四类结局闭环可达

## NPC 渲染与表演

- [x] 叶澄眼部黑框已清除
- [x] 顾衡右眼异色已清除
- [x] 两名 NPC 均恢复模型自身导入材质
- [x] 两个精确骨架共 14 个 v0.8 动画通过审计
- [x] 行走和回应截图未见手臂穿体或身体扭曲
- [x] 模型只选择固定移动和反应枚举
- [x] 移动受碰撞、玩家距离、岗位半径、冷却和单步距离约束
- [x] 两名 NPC 的 85 cm 移动和回应截图已纳入包内证据

## AI

- [x] DeepSeek V4-Flash 官方端点脱敏探针 HTTP 200
- [x] 严格六字段响应校验
- [x] AP、资源、事实、承诺、任务、评分和结局保持本地权威
- [x] 缺少密钥、显式离线和网络故障均确定性降级
- [x] 同一 NPC 对话历史连续传递
- [x] 在线请求禁用 thinking 和 stream，并要求 JSON Object

## 测试与构建

- [x] Agent 测试 48/48
- [x] Rules 测试 30/30
- [x] Release 测试 18/18
- [x] 敏感信息规则测试 4/4
- [x] UE Automation 8/8
- [x] 动画资产审计 14/14
- [x] 720p/1080p 准心状态审计 4/4
- [x] 真实输入烟测 2/2
- [x] Shipping 路线、降级、历史与表演探针 12/12
- [x] UE 5.8 Build/Cook/Stage/Pak/IoStore/Archive 成功
- [x] 源码门禁和发布包门禁验证通过
- [x] 发布包 153 个文件全部纳入最终目录或清单约束

## 发布物

- [x] `README_v0.8.txt`
- [x] `ASSET_LICENSES.md`
- [x] `Validation/gate_manifest.json`
- [x] `Validation/InputSmoke`
- [x] `Validation/ShippingSmoke`
- [x] 启动器与 Shipping 二进制 SHA-256 已记录
- [x] PAK/UCAS/UTOC SHA-256 已记录

## 磁盘清理

- [x] 删除 v0.5 独立旧构建工作树
- [x] 删除 v0.6 独立旧工作树
- [x] 释放约 14.74 GiB
- [x] 保留用户主工作树、v0.7 回滚工作树和 v0.8 发布工作树

## 法务边界

- [x] 包根 README 标记 `LOCAL REVIEW BUILD - DO NOT REDISTRIBUTE`
- [x] `distribution_class` 固定为 `local_review_only`
- [x] Noanoa 发型许可风险已写入 README、许可清单和发布文档
- [ ] 取得 Noanoa 发型嵌入及再分发书面许可，或替换该发型
- [ ] 完成 Windows 代码签名

未完成的两项不影响本机私人开发评审，会阻止当前候选包公开、商业或跨设备
分发。

## 最终候选

`G:\WhiteoutStation-v08-worktree\Builds\WhiteoutStation-v0.8-Win64-20260728T194955Z-6f131d00-release`

Run ID：`20260728T194955Z-6f131d00-release`

Shipping 源码提交：`6f131d006a6a8ec825582b8f230ad8d358ed2986`
