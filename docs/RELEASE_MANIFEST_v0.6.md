# v0.6 发布清单

## 标识

- 版本：`0.6.0`
- 运行 ID：`20260726T120458Z-4326f3ba-release`
- Shipping 目录：`Builds/WhiteoutStation-v0.6-Win64-20260726T120458Z-4326f3ba-release`
- 源提交：`4326f3ba268de4846f1b7889ca84858daf70984b`
- 源树：`d75ae49a6fb2fe127dad3da43863dd30003c3d8e`
- 构建时间：`2026-07-26T12:04:58Z`
- Unreal Engine：`5.8.0`
- Python：`3.13.5`

## 内容

- 清单校验文件：89
- 总文件数（含清单）：90
- 总字节数：788,753,256
- 清单：`Validation/gate_manifest.json`
- 清单自身 SHA-256：`71f3b73bb55304202450d28b7fb9ab5d1a1e7ff2b7906441aa48c101fdf21c81`
- 真实输入证据：`Validation/InputSmoke`
- Shipping 证据：`Validation/ShippingSmoke`

## 关键 SHA-256

| 文件 | SHA-256 |
|---|---|
| `Windows/WhiteoutStation.exe` | `1d2592875b7d9410f9a487e6f4e92c99b08652c475ebc71d02d62037875c1b20` |
| `Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe` | `ce7bd061979361efb6b2eedcbf2748b9da5598d5c25f11af99ca6907bce03058` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.pak` | `569114e4ccc549e0db08616d79cb7040dc436a16fd8bff439047707367a979c8` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.ucas` | `fa6d1a023e23961d1197d55ee1c5afeee9eea3676263f0b64e854999f4e345a6` |
| `Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.utoc` | `8c3e45914c1f0732194f23800366c03086b0b14f36b73ff01b59380e05f757a8` |
| `Windows/WhiteoutStation/Content/Rules/WhiteoutStationRules.v0.6.json` | `15252e9bf9701c10fe70af7b4759a6f94269a2c975422ddf0f4c93524b1a041f` |
| `Windows/WhiteoutStation/Content/Agents/AgentRuntime.v0.6.json` | `fd66c2765f53362e1b14bea10237448cc6caedf9777b659bfb3df3f2a1a0d907` |
| `Validation/InputSmoke/input_smoke_summary.json` | `6f4835f45998289ff224f77cb334253a71dfe66f5ef8ee20401ccec518024f93` |
| `Validation/ShippingSmoke/shipping_smoke_summary.json` | `144c9665ee98335ca501c38a9adecc7ae43af14d804efd46dcafffa1b39af787` |

## 验收

- v0.6 源码门禁：PASS；
- UE 5.8 Win64 Shipping Build/Cook/Stage/Pak/IoStore/Archive：PASS；
- 真实输入烟测：2 / 2；
- Shipping 烟测：10 / 10；
- `validate_release_v06.py`：PASS；
- 5 个顾衡/叶澄相关冻结对象：与基线一致；
- 分发包内无 API Key。

发布包绑定功能源码提交 `4326f3ba...`。随后产生的输入工具加固与发布文档提交不改变包内二进制、规则或 Agent Runtime；发布校验始终显式使用该源码提交。
