# Whiteout Station v0.8 - QA 报告

日期：2026-07-29

结论：PASS（限本机评审）

## 验证对象

- 引擎：Unreal Engine 5.8.0
- 平台：Windows Win64 Shipping
- 源码提交：`6f131d006a6a8ec825582b8f230ad8d358ed2986`
- 源码树：`715e517ecc74eb1ebbfe006f4e87f539db2a2dcf`
- 候选包：
  `Builds/WhiteoutStation-v0.8-Win64-20260728T194955Z-6f131d00-release`
- 包内门禁清单：`Validation/gate_manifest.json`

## 自动化结果

| 层级 | 命令或证据 | 结果 |
|---|---|---:|
| Agent 契约 | `python -X utf8 -m pytest Tools/Agents -q` | 48 passed / 3.82s |
| 规则与闭环 | `python -X utf8 -m pytest Tools/Rules -q` | 30 passed / 0.32s |
| 发布门禁 | `python -X utf8 -m pytest Tools/Release/test_v08_release_gates.py -q` | 18 passed / 32.76s |
| 敏感信息规则 | `python -X utf8 -m unittest Tools.Release.test_scan_secrets -v` | 4 passed |
| UE 自动化 | `WhiteoutStation` Automation 集 | 8/8 passed |
| 动画审计 | `Tools/Editor/audit_v08_npc_animations.py` | 14/14 passed |
| UI 像素审计 | `Tools/Capture/audit_v08_ui.py` | 4/4 passed |
| 真实输入烟测 | `Validation/InputSmoke/input_smoke_summary.json` | 2/2 passed |
| Shipping 烟测 | `Validation/ShippingSmoke/shipping_smoke_summary.json` | 12/12 passed |
| 源码门禁 | `Tools/Release/validate_source_v08.py` | PASS |
| 发布包门禁 | `Tools/Release/validate_release_v08.py` | PASS |
| Git 历史敏感信息扫描 | `Tools/Release/scan_secrets.py --history` | PASS |

UE 自动化覆盖 `DialogueBoundary`、`ModelBudget`、`APFlow`、
`DialogueAndResourceChoices`、`DialogueStages`、
`KnowledgeAndAgentValidation`、`Routes`、`Transactions`。

## UI、鼠标与真实输入

- 顶部警报完整收纳在 124 px 高且启用 `ClipToBounds` 的面板内；
- 首要目标面板与顶部警报没有重叠；
- 空心圆和手形准心在 1280×720、1920×1080 下最大中心误差 1.5 px；
- 交互提示文字与墨刷背景共用同一布局中心；
- 手册、证据板和对话各自打开/关闭时，鼠标均回到视口中心；
- 六次实机鼠标回中检查的 X/Y 误差全部为 0 px；
- `Tab`、`Enter`、Unicode 中文输入和 `Esc` 可完成完整对话；
- 开局顾衡意向只有“询问”和“安抚”，没有提前显示“承诺”；
- 人物详情和生存手册全部使用 0—10 数值。

证据：

- `Validation/InputSmoke/survival_controls_game.png`
- `Validation/InputSmoke/survival_controls_focus.png`
- `Validation/InputSmoke/survival_controls_guide.png`
- `Validation/InputSmoke/dialogue_free_text_intent.png`
- `Validation/InputSmoke/dialogue_free_text_text_entry.png`
- `Validation/InputSmoke/dialogue_free_text_closed.png`

## 开场与美术

- 十句开场必须由 Space 或鼠标点击逐句推进；
- 文本说明站点任务、停电危机、顾衡的工程职责、叶澄的医疗职责、三人依赖
  关系、内部矛盾和求救目标；
- 最后一页推进后黑幕正常淡出；
- 新墙面和地面材质已在 Shipping 真实输入截图和 NPC 表演截图中出现；
- 只升级运行时几何上的旧默认材质，用户自定义材质覆盖保持优先。

## NPC 材质与动画

旧关卡序列化的 `M_WS_Eye` 覆盖只在命中该确切遗留材质时恢复为模型自身
导入材质。发布截图确认叶澄没有眼部黑框，顾衡没有右眼异色。

14 个动画全部满足：

- 绑定当前 NPC 精确骨架；
- 时长和轨道集合符合契约；
- 所有关键帧有限；
- 平移和缩放保持参考姿势；
- 待机和行走循环闭合；
- 不写入面部、眼睛、头发、手指和衣物骨骼。

走动和回应截图复核没有手臂穿体或身体扭曲。

## AI 契约与降级

- 响应必须精确包含六个字段；
- 缺字段、增字段、非法枚举和越权事实引用均被拒绝；
- DeepSeek V4-Flash 官方端点脱敏探针返回 HTTP 200；
- 缺少密钥时不发请求，显式离线时模型调用数为 0；
- 不可达端点触发确定性降级，技术路线仍完成 `TaskSuccess`；
- 连续两轮请求的消息数为 `[2, 4]`，历史轮数为 `[0, 1]`；
- Loopback 请求没有 `Authorization`；
- API key 未写入证据、日志、包或 Git 历史。

两名 NPC 的表演探针：

| NPC | 模型移动 | 本地实际距离 | 反应 | 结果 |
|---|---|---:|---|---:|
| 顾衡 | `step_closer` | 85.0 cm | `acknowledge` | PASS |
| 叶澄 | `step_closer` | 85.0 cm | `acknowledge` | PASS |

## 路线和结局

| 场景 | 实际结局 | 模型调用 | 结果 |
|---|---|---:|---:|
| missing_key_medical | TaskSuccess | 0 | PASS |
| missing_key_technical | TaskSuccess | 0 | PASS |
| missing_key_quick | CostUncontrolled | 0 | PASS |
| missing_key_wait | SurvivalWait | 0 | PASS |
| missing_key_cost | CostUncontrolled | 0 | PASS |
| missing_key_collapse | TotalCollapse | 0 | PASS |
| explicit_offline_medical | TaskSuccess | 0 | PASS |
| loopback_online_technical | TaskSuccess | 1 | PASS |
| unreachable_endpoint_technical | TaskSuccess | 1 | PASS |

## 内容保护和安全

- 用户关卡与开工基线 Git 对象一致；
- 顾衡、叶澄源模型与人物绑定保持开工基线；
- 保护目录只增加 14 个允许清单中的动画；
- 667 个已跟踪文本或容器文件的敏感信息扫描通过；
- Git 索引、完整历史和 Git LFS 完整性检查通过；
- 包内规则、Agent 配置、输入证据、Shipping 证据和二进制均有 SHA-256。

## 已知限制

- QA 使用本机自动化和开发者视觉复核，尚无独立玩家样本；
- DeepSeek 服务端质量和可用性超出项目控制范围；
- 可执行文件未签名，Windows 可能显示未知发布者；
- Noanoa 发型许可将当前包限制为 `local_review_only`。
