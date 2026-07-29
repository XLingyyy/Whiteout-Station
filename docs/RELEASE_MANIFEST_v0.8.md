# Whiteout Station v0.8 - 发布清单

日期：2026-07-29

## 标识

| 字段 | 值 |
|---|---|
| 版本 | 0.8.0 |
| Run ID | `20260728T194955Z-6f131d00-release` |
| 分发等级 | `local_review_only` |
| 构建时间 UTC | `2026-07-28T19:49:55.408308Z` |
| 引擎 | Unreal Engine 5.8.0 |
| 源码分支 | `codex/v0.8-evolution` |
| Shipping 源码提交 | `6f131d006a6a8ec825582b8f230ad8d358ed2986` |
| 源码树 | `715e517ecc74eb1ebbfe006f4e87f539db2a2dcf` |
| 源码状态 | clean |

## 物理发布物

根目录：

`G:\WhiteoutStation-v08-worktree\Builds\WhiteoutStation-v0.8-Win64-20260728T194955Z-6f131d00-release`

- 文件数：153；
- 总大小：866,348,292 bytes（826.21 MiB）；
- 启动入口：`Windows/WhiteoutStation.exe`；
- 包内正式清单：`Validation/gate_manifest.json`；
- 正式清单 SHA-256：
  `15d5d6bb4df670396196f6cff0f250c05ff39f7402c16824ac67f0b99a4125f3`。

清单生成时校验 152 个既有文件；清单文件加入后，最终目录共有 153 个文件。

## 关键 SHA-256

| 文件 | SHA-256 |
|---|---|
| `Windows/WhiteoutStation.exe` | `1d2592875b7d9410f9a487e6f4e92c99b08652c475ebc71d02d62037875c1b20` |
| `Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe` | `66b996719d557c73db4ea1c6003d0e7ee5bf4a17035e5119af7033061250c052` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.pak` | `66cb761ceacd0ecfcb58063b72b57d9200abf94af8b60daca11213af815b8537` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.ucas` | `f5fbbcda43517c4849042fbb0caaa065762734a2466e6d0cc33fb5926ca39b5e` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.utoc` | `f0a41e84e09336af3ed329cea9b6e8ec7157b036eed8f2262cdec3d64782f58a` |
| `Windows/WhiteoutStation/Content/Rules/WhiteoutStationRules.v0.8.json` | `bf107a131c7eec9821b6c196ea42a220872dece2e25a5eb9a86c5f998ecc2f87` |
| `Windows/WhiteoutStation/Content/Agents/AgentRuntime.v0.8.json` | `38c9ad436155caf9882579732c9a92ef1944d530860031c3701b20cde8b0ec9e` |
| `Validation/InputSmoke/input_smoke_summary.json` | `3aff45bf5a19b4639476561116fcf8cc797f7f0ef11a75e6416d7fd5919cea6d` |
| `Validation/ShippingSmoke/shipping_smoke_summary.json` | `6960160bbd23c6e35aab913f8535ce68f4a532b26b6fdb95a81a7df8f478162e` |

## 功能内容

- HUD 警报越界、准心中心、交互提示和面板鼠标回中修复；
- 全部人物状态和信任统一为 0—10；
- 十句手动推进黑幕开场和更清晰的合作动机；
- 分阶段意向与中文自由输入对话；
- 顾衡、叶澄眼部材质修复；
- 两个当前精确骨架共 14 个待机、行走和反应动画；
- 严格六字段 AI 表达契约及本地约束的移动与反应；
- v0.8 墙面和地面材质；
- 生存手册、证据板、状态解释和四类结局闭环。

## 验证内容

包内保留：

- 2 条真实键鼠输入全流程及 28 张截图；
- 6 次面板打开/关闭鼠标回中记录；
- 9 条路线与降级场景；
- 1 条连续对话历史探针；
- 2 条 NPC 表演探针和 4 张动作截图；
- 模型与 loopback 审计日志；
- 关键运行事件日志和最终截图。

## 用户内容与许可

用户关卡、顾衡和叶澄源模型及人物绑定保持 v0.8 开工基线。本轮只在两个
`AnimationsV08` 目录新增 14 个允许清单中的动画，运行时修复不改写用户地图。

叶澄 Noanoa 发型尚无可确认的产品嵌入与再分发授权。当前发布物只能留在本机
用于私人开发和评审，禁止上传、转发、公开提供下载或商业分发。公开发布前
必须取得权利方书面许可，或替换该发型并重新构建、测试和生成清单。
