# Whiteout Station / 风雪站：断电前夜

Unreal Engine 5.8 C++ 项目，目标是一局 10—15 分钟的社会生存与轻推理体验。

## 固定开发环境

- Unreal Engine 5.8
- Windows 64-bit
- C++ 权威规则层，蓝图 / UMG 表现层
- Enhanced Input
- 官方 Model Context Protocol 插件（仅开发期）

## 当前开发版本

v0.5 发布版已完成受控 AI 表达和玩法选择：

- DeepSeek `deepseek-v4-flash` 仅改写 NPC 台词；AP、资源、事实权限、承诺、任务、评分和结局始终由 C++ 决定。
- 询问、质疑、安抚和承诺以结构化意图进入规则；无效或重复承诺不会消耗 AP。
- 行动预览中按 Q 可循环口粮分配方案，或在药品与已披露的保温包之间选择。
- HUD 统一显示“健 / 温 / 精 / 饱 / 稳”，并修正连续分数评级边界。
- 顾衡与叶澄的人物模型、骨骼、材质、动作、动画及 LookAt 表现均未修改。用户工作树中的 `MVP_StationMap.umap` 既有改动保持原样。

已确认 Editor Development 与 Win64 Shipping 构建成功，Python 回归 81 / 81、UE Automation 7 / 7、Shipping 烟测 5 / 5，源码与发布门禁均为 PASS。当前证据见 `docs/QA_REPORT_v0.5.md`，发布清单见 `docs/RELEASE_MANIFEST_v0.5.md`。

最终 Demo：

- 目录：`Builds/WhiteoutStation-v0.5-Win64-20260725T134938Z-9bd94fab-release`
- 启动：`Windows/WhiteoutStation.exe`
- 源提交：`9bd94fab63f446290fbb5ababf809529a91c1b7c`
- 源树：`bc4ac95f5dd4120fbfa545c4d92719336bfa0ac9`

## 规则回归

在仓库根目录运行：

```powershell
Push-Location Tools/Rules
python -X utf8 -m pytest test_whiteout_rules.py -q
python -X utf8 run_routes.py
Pop-Location
```

`WhiteoutStation/Content/Rules/WhiteoutStationRules.v0.5.json` 记录 v0.5 的版本化规则与平衡规格，并提供 Python 工具和 C++ 运行时启动配置。`FWhiteoutRulesEngine` 是游戏运行时唯一提交入口；Python/C++ 的边界与三条路线由双端回归锁定。

## 编辑器游玩

默认地图为 `MVP_StationMap`。WASD / 鼠标移动观察；看向物体后按 F 打开预览、再次按 F 确认；看向 NPC 按 F 开始对话；行动预览中按 Q 切换资源方案；E 打开或关闭证据板；Space 跳过开场；Enter 结算，提前接受失败结局需要再次确认；R 重开；C 读取自动存档；Esc 返回上一层或打开暂停。

完整构建、启动、操作、本地数据与 AI 开关说明见 `docs/BUILD_AND_PLAY_v0.5.md`。关卡中的站体、陈设、灯光、NPC 与交互点可在编辑器里直接选择、拖动和替换，操作说明见 `docs/LEVEL_EDITING.md`。

运行态回归可传入 `-WhiteoutAutoRoute=medical|technical|quick -WhiteoutAutoCapture`，自动走完指定路线、结算、导出事件日志，并保存 `Saved/WhiteoutRuntimeSmoke.png`。

## DeepSeek V4 表达合同

在线表达默认关闭。启用时使用 `deepseek-v4-flash`，请求显式设置非思考模式、非流式输出和 JSON Object 响应。正常游戏直接采用玩家选择的规则意图，不调用模型判定意图。

模型正文只接受以下四个必填字段，额外或缺失字段均会触发本地降级：

- `npc_line`
- `emotion`
- `used_action_id`
- `referenced_fact_ids`

`used_action_id` 必须匹配已提交动作；事实引用必须位于 C++ 投影的白名单内。网络失败、429 / 5xx、非 `stop` 结束原因、空正文、截断、schema 错误或事实越权均返回确定性本地台词。

启用官方服务需要在启动进程环境中同时设置 `WHITEOUT_LLM_ENABLED=true` 与 `WHITEOUT_LLM_API_KEY`。密钥只会发送给 HTTPS 且主机严格等于 `api.deepseek.com` 的端点；loopback mock 永不携带 Authorization，其他端点会被拒绝。不要把密钥放入仓库、命令行、日志或分发文件。

本地合同 mock：

```powershell
python -X utf8 Tools/Agents/mock_agent_server.py --host 127.0.0.1 --port 8765
```

随后给游戏进程传入：

```text
-WhiteoutLLMEnabled=true -WhiteoutAgentEndpoint=http://127.0.0.1:8765/chat/completions
```

## 工程目录

- `WhiteoutStation/Source/WhiteoutStation`：UE C++ 运行时代码
- `WhiteoutStation/Content/WindStation`：场景、数据、UI、角色、音频和测试资产
- `WhiteoutStation/Content/Rules`：可复现的规则配置
- `WhiteoutStation/Content/Agents`：表达层运行配置
- `Tools/Agents`：协议校验、mock 与 DeepSeek 探针
- `Tools/Rules`：脱离编辑器运行的规则模拟与回归
- `Tools/Release`：源码与发布门禁
- `docs`：设计、实施清单、范围、进度与 QA 记录

## 版本管理

`.uasset`、`.umap` 与大型媒体通过 Git LFS 管理；`Binaries`、`Intermediate`、`Saved`、`DerivedDataCache`、本机密钥配置和 IDE 临时文件不入库。
