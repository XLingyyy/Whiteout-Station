# v0.5 实施进度

更新日期：2026-07-25
当前状态：**功能实现与发布前验证完成；本记录不声明 Shipping 归档或版本发布完成**

## 本轮结果

v0.5 已形成“确定性规则决策 → 可选 DeepSeek 表达 → 严格校验 → 安全降级”的闭环。`FWhiteoutRulesEngine::Commit` 是运行时唯一状态提交入口，模型没有修改 AP、资源、事实、人物状态、承诺、任务、评分或结局的接口。

### AI 接入

- 在线模型固定为 `deepseek-v4-flash`，默认关闭；无 Key 时完整游戏流程使用确定性本地台词。
- 请求显式设置 `thinking.type=disabled`、`stream=false` 与 `response_format.type=json_object`。
- 表达正文只接受 `npc_line`、`emotion`、`used_action_id`、`referenced_fact_ids` 四个必填字段。
- `used_action_id` 必须匹配已提交动作，事实 ID 和台词内容同时受许可事实集合约束。
- 官方凭据只发往 HTTPS `api.deepseek.com`；loopback 不携带 Authorization；未知远程端点拒绝启用。
- 连接失败、429、500、503 最多重试一次；超时、HTTP、envelope、结束原因、schema 或事实校验失败均回退本地台词。
- 活动请求绑定对话 session、事务和 generation；新局、读档和销毁会取消旧请求，旧回调不能写入新会话。
- 审计仅记录模型、请求 ID、动作、意图、HTTP / finish、耗时、字节数、token 数与结果，不保存 Key、Authorization、玩家原文或完整模型回复。

### 玩法逻辑

- 询问、质疑、安抚、承诺由玩家显式选择并进入同一个 `FWSActionRequest`；正常游玩不依赖在线意图分类。
- 顾衡允许三个白名单承诺条件；叶澄的承诺、非法条件和重复承诺均在预览与规则层拒绝，且不消耗 AP。
- 质疑、安抚和承诺对信任与压力产生确定、可测试的不同修正。
- 同一行动请求贯穿预览与提交。按 Q 可循环六种合法口粮分配组合，并可在药品和已披露的保温包之间切换治疗资源。
- HUD 五项状态统一为“健 / 温 / 精 / 饱 / 稳”，精力、饱腹和稳定度均采用越高越好的方向。
- Python 与 C++ 使用一致的连续分数评级边界；三条黄金路线参数同步。
- 未发出求救信号且仍有 AP 时，第一次 Enter 给出明确警告，只有在状态未变化时再次按 Enter 才接受失败结局。

## 已确认验证

| 层级 | 结果 |
|---|---:|
| UE 5.8 Editor Development 构建 | PASS |
| UE Automation `WhiteoutStation` | 7 / 7，0 warning，0 failed，0 not run |
| Python Agents | 39 passed |
| Python Rules | 28 passed |
| Python Release | 14 passed |
| 本地 mock 顾衡 / 叶澄 | 两者均 `validation=ok` |
| 429 持续故障 | 初次请求 + 恰好一次重试，随后本地回退 |
| 429 → 200 | 恰好一次重试后在线响应通过严格校验 |
| 无 Key 路径 | 确定性本地回退 |
| 官方 DeepSeek 探针 | HTTP 与 schema 校验通过，`secret_present=false` |
| 官方 UE 顾衡 / 叶澄表达 | 最终复测两者均 `provider=deepseek`、`fallback=false`、`validation=ok` |

UE Automation 报告位于 `Artifacts/TestResults/v05-final-20260725-212000`。详细矩阵见 `docs/QA_REPORT_v0.5.md`。

## 黄金路线

| 路线 | 结局 | 分数 | 评级 | 剩余 AP |
|---|---|---:|---:|---:|
| medical | TaskSuccess | 76.76 | B | 0 |
| technical | TaskSuccess | 72.02 | B | 0 |
| quick | TaskSuccess | 72.06 | B | 2 |

## 范围保护

- `WhiteoutStation/Content/WindStation/Art/Characters/**` 未改。
- `SourceAssets/MakeHuman/Characters/**` 未改。
- 顾衡与叶澄的 SkeletalMesh、Skeleton、材质、Animation、AnimBP、动作和 LookAt 表现未改。
- `MVP_StationMap.umap` 在本轮开始前已有用户工作树修改；本轮实现未编辑、覆盖、暂存或清理该文件，原改动保持不变。

保护目录的基线对象记录在 `docs/PROTECTED_CHARACTER_ASSETS_v0.5.json`。本文件只记录已经完成的源码、协议、规则和 Editor 级验证，不包含尚未产生的 Shipping 路径、提交标识或发布通过结论。
