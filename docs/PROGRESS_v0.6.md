# v0.6 实施进度

更新日期：2026-07-26
当前状态：**v0.6 Demo 已完成并发布**

## 本轮结果

v0.6 已形成“玩家读懂背景 → 获得当前建议 → 调查/交涉/行动 → 状态与关系变化 → 路线与承诺结算 → 四类结局复盘”的完整闭环。

### 对话

- 意向由 NPC、证据、人物需求、压力、关系和危机阶段决定。
- 初始顾衡只显示询问/安抚，叶澄只显示询问。
- 顾衡承诺绑定保留药品、维修间供暖、保存维修记录三个具体语境；已承诺项移除。
- 叶澄没有承诺入口；非法、无语境和重复请求由规则层拒绝且不扣 AP。
- 玩家先选意向，再自由输入说法；同会话最多携带最近 4 轮只读历史。

### 开场与教学

- 7 句纯黑幕中央白字按点击/Space 逐句推进，没有自动超时。
- 文字逐句淡入淡出，最后黑幕渐隐到站内第一人称视角。
- HUD 增加动态“当前建议”；H 打开生存手册。
- 手册解释核心目标、健/温/精/饱/稳、信任、AP、阈值、影响和改变方式。

### 产品与反馈

- 暂停菜单补齐保存、读取、帮助、设置、重开和退出。
- 设置支持 FOV、四路音量、90%—120% 字号和减少动态效果。
- 结算页识别医疗协作、证据替代、直接抢修和强行自修，显示关键代价、承诺、评分、最终状态、时间线和建议。
- `TaskSuccess`、`SurvivalWait`、`CostUncontrolled`、`TotalCollapse` 全部可达。
- 行动提交、提前结算、AI 等待/取消/降级和结局过渡都有可见反馈。

### AI 边界

- 官方表达模型为 `deepseek-v4-flash`，默认离线。
- C++ 是 AP、资源、事实、承诺、状态、评分和结局的唯一提交入口。
- 官方凭据仅发往 `api.deepseek.com`；loopback 不带 Authorization。
- 活动请求可取消；网络、HTTP、finish、envelope、schema 或事实验证失败会有界回退。
- 官方 DeepSeek 探针、loopback 在线、无 Key、断连和两轮历史合同均通过。

## 验证结果

| 层级 | 结果 |
|---|---:|
| UE 5.8 Editor Development | PASS |
| UE Automation | 8 / 8，0 warning / failed / not run |
| Python Agents | 43 passed |
| Python Rules | 30 passed |
| Python Release | 15 passed |
| 真实输入 | 2 / 2 |
| Shipping 矩阵 | 10 / 10 |
| 发布清单 | PASS |

## 发布

- 功能源码提交：`4326f3ba268de4846f1b7889ca84858daf70984b`
- 源树：`d75ae49a6fb2fe127dad3da43863dd30003c3d8e`
- Shipping：`Builds/WhiteoutStation-v0.6-Win64-20260726T120458Z-4326f3ba-release`
- 包内文件：90
- 总大小：788,753,256 字节
- 真实输入证据：`Validation/InputSmoke`
- Shipping 证据：`Validation/ShippingSmoke`
- 清单：`Validation/gate_manifest.json`

## 范围保护

- `WhiteoutStation/Content/WindStation/Art/Characters/**` 未改。
- `SourceAssets/MakeHuman/Characters/**` 未改。
- 顾衡与叶澄的 SkeletalMesh、Skeleton、材质、动作、Animation、AnimBP 与 LookAt 未改。
- 用户主工作树和其中既有地图改动未被清理、覆盖或暂存。

保护基线记录于 `PROTECTED_CHARACTER_ASSETS_v0.6.json`。详细构建、QA、试玩和发布信息分别见 `BUILD_AND_PLAY_v0.6.md`、`QA_REPORT_v0.6.md`、`PLAYTEST_v0.6.md`、`RELEASE_CHECKLIST_v0.6.md` 和 `RELEASE_MANIFEST_v0.6.md`。
