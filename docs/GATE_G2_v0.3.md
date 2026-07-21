# G2 对话体验验收记录

验收日期：2026-07-22

## 结论

G2 通过。NPC 交互现为 `F 进入对话 → 镜头与输入锁定 → 鼠标选择轮盘/自由文本 → 既有规则事务结算`；旧 Q 键和数字 1–6 选择已移除。模型仅负责意图分类与 NPC 表达，不拥有任何状态写权限。

## V3-11 / V3-12 对话流程与轮盘

- NPC 焦点提示直接显示 `[F] 开始对话`；普通交互物仍保留预览/确认流程。
- 轮盘只暴露询问、质疑、承诺、安抚、自由输入和取消。承诺二级页只含不弃站、保留药品、维修间升温三类规则条件。
- 对话时锁定移动与视角、显示鼠标并使用 `UIOnly`；取消后恢复 `GameOnly`、移动与视角。
- 右侧 NPC 卡从状态快照生成肖像、姓名/职务/年龄、信任关系、当前立场、健康/体温/压力/信任条；顾衡年龄按 StringTable 为 41 岁，未写死编造立场。
- 所有选择最终调用原有 `Interact` 事务，单次对话只允许提交一次；AP、承诺、证据与信任规则层未改。

## V3-13 / V3-14 模型边界

- 固定模型 `deepseek-v4-flash`，OpenAI-compatible Chat Completions envelope。
- 意图结果要求精确三字段 schema、四类意图白名单、三类承诺白名单、置信度门槛；承诺再做原文关键词双检。
- 表达结果沿用事实白名单、响应类型一致性、状态字段拒绝和语义泄漏检查。
- 意图与表达共享每局 10 次硬预算；完整 request / response / outcome / session count 写入 JSONL 审计，不记录授权头。
- 降级次序为在线模型 → 本地意图词典 → 安全轮盘；离线 UI 明确显示当前路径。
- `Tools/Agents/mock_chat_proxy.py` 提供确定性 mock。意图/降级明细见 `docs/INTENT_TEST_REPORT_v0.3.md`，审计样例见 `docs/model_audit_mock_sample_v0.3.jsonl`。

## V3-15 头部 LookAt

- 新增 SkeletalMesh 组件在动画完成后只修正头部分支，不旋转 Actor 身体。
- 距离阈值 850 cm；对话期间保持注视；离开插值回正。
- yaw 限位 ±55°，pitch 限位 -20° 至 +25°；三角度截图中身体方向不随镜头改变，无颈部反折。

## 截图基线

`docs/baseline_v0.3/` 新增 18 张 G2 双分辨率基线：

- `UI_dialogue_{gu_wheel,ye_wheel,promise,free,offline,response}_*`；
- `UI_lookat_{near,side,far}_*`；
- 1280×720 与 1920×1080 各 9 张，文件像素尺寸逐项验证；两次捕获日志均正常退出且无项目 Fatal/Error。

## 门禁结果

| 检查 | 结果 |
|---|---|
| Editor Development 编译 | PASS |
| Python 规则回归 | 17 / 17 PASS |
| UE Automation（无模型） | 6 / 6 PASS |
| 中文意图样例 | 30 / 30，100% |
| 无提供方回调自动化断言 | PASS，本地词典映射正确 |
| 规则冻结 | 5 / 5 MATCH |
| 中文 StringTable | 255 条、210 个当前引用键，PASS |
| 无模型 AutoRoute `medical` | TaskSuccess，76.64，与 v0.2 一致 |
| 无模型 AutoRoute `technical` | TaskSuccess，71.90，与 v0.2 一致 |
| 无模型 AutoRoute `quick` | TaskSuccess，72.06，与 v0.2 一致 |
| mock 意图 HTTP 探针 | `Promise / heat_repair_room / online_model`，PASS |
| mock 技术路线 | TaskSuccess，71.90；1 意图 + 6 表达请求，PASS |
| 当前索引密钥扫描 | PASS |
| 分支/远端/标签历史密钥扫描 | PASS |

本门禁未修改 `Content/Rules`、`Tools/Rules` 或 `Source/*/State`。
