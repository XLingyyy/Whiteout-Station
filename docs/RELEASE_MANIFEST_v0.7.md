# Whiteout Station v0.7 - 发布清单

日期：2026-07-28

## 标识

| 字段 | 值 |
|---|---|
| 版本 | 0.7.0 |
| Run ID | `20260728T150103Z-0e96ad1b-release` |
| 分发等级 | `local_review_only` |
| 构建时间 UTC | `2026-07-28T15:01:03Z` |
| 引擎 | Unreal Engine 5.8.0 |
| 源码分支 | `codex/v0.7-evolution` |
| 源码提交 | `0e96ad1b22c4ab894e5c5f5e7dc9f1c103f38bda` |
| 源码树 | `0678c323d73213f034944c16bb30c686aca7886d` |
| 源码状态 | clean |

## 物理发布物

根目录：

`G:\WhiteoutStation-v07-worktree\Builds\WhiteoutStation-v0.7-Win64-20260728T150103Z-0e96ad1b-release`

- 文件数：151；
- 总大小：868,619,705 bytes（828.38 MiB）；
- 启动入口：`Windows/WhiteoutStation.exe`；
- 包内正式清单：`Validation/gate_manifest.json`；
- 正式清单 SHA-256：
  `070e216ec0262eda94139abf52e466e95800f47808abd8d1428fcdd005675b2a`。

清单生成时校验 150 个既有文件；清单文件加入后，最终目录共有 151 个文件。

## 关键 SHA-256

| 文件 | SHA-256 |
|---|---|
| `Windows/WhiteoutStation.exe` | `1d2592875b7d9410f9a487e6f4e92c99b08652c475ebc71d02d62037875c1b20` |
| `Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe` | `35c4204171e05a98aef31abbdc94467667cf325f7cb8d921b7c946874397e927` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.pak` | `3411d2d2a1c81aba921a84964573fab5b3fed7beb1045761a1a1d7170d48a70b` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.ucas` | `48201082876635eb88621c619b6e75c374b9e351abd2474e62ba2f52f47fad91` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.utoc` | `8380d12102d8cb07828749224fac9ad9493533233bae2def5b49dfbff67017fa` |
| `Windows/WhiteoutStation/Content/Rules/WhiteoutStationRules.v0.7.json` | `aa07a773cfea4e48da41cecba520a757bb694b94ae1230c5d84e851964c56ca0` |
| `Windows/WhiteoutStation/Content/Agents/AgentRuntime.v0.7.json` | `118d38073f632e766e6e5286d3f2e32fdc3e38d636ecd420611dde33fdd6b7b2` |
| `Validation/InputSmoke/input_smoke_summary.json` | `08c76eed753643b9d2fbe3d438e9bdf52ef378b8571c181abd4f6d377d9b7508` |
| `Validation/ShippingSmoke/shipping_smoke_summary.json` | `49e3d1b8bda02cfdfd0bdf09e4d8c80564d079059af0727b51e49606f5f6b083` |

## 功能内容

- UI 溢出、准心中心和交互提示对齐修复；
- 运行时补全所有行动交互入口；
- 分阶段意向与自由输入对话；
- 严格六字段 AI 表达契约；
- 受本地约束的 NPC 移动与反应；
- 顾衡、叶澄共 12 个新动画资产；
- 七句手动推进黑幕开场；
- 生存手册、证据板、状态解释；
- 四类结局和确定性离线闭环。

## 验证内容

包内保留：

- 2 条真实键鼠输入全流程及 26 张截图；
- 9 条路线与降级场景；
- 1 条连续对话历史探针；
- 2 条 NPC 表演探针；
- 模型与 loopback 审计日志；
- 关键运行事件日志和最终截图。

## 用户内容与许可

用户关卡和两名 NPC 模型保持 v0.7 开工基线；本轮只新增允许清单中的动画
资产并接入运行时逻辑。

叶澄 Noanoa 发型尚无可确认的产品嵌入与再分发授权。当前发布物只能留在本机
用于私人开发和评审，禁止上传、转发、公开展示可下载包或商业分发。公开发布
前必须取得权利方书面许可，或替换该发型并重新构建、测试和生成清单。
