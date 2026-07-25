# v0.5 发布清单

## 标识

- 版本：`0.5.0`
- 运行 ID：`20260725T134938Z-9bd94fab-release`
- Shipping 目录：`Builds/WhiteoutStation-v0.5-Win64-20260725T134938Z-9bd94fab-release`
- 源提交：`9bd94fab63f446290fbb5ababf809529a91c1b7c`
- 源树：`bc4ac95f5dd4120fbfa545c4d92719336bfa0ac9`
- 构建时间：`2026-07-25T13:51:56Z`
- Unreal Engine：`5.8.0`
- Python：`3.13.5`

## 内容

- 清单校验文件：45
- 总文件数（含清单）：46
- 总字节数：774,964,207
- 清单：`Validation/gate_manifest.json`
- Shipping 证据：`Validation/ShippingSmoke`

## 关键 SHA-256

| 文件 | SHA-256 |
|---|---|
| `Windows/WhiteoutStation.exe` | `1d2592875b7d9410f9a487e6f4e92c99b08652c475ebc71d02d62037875c1b20` |
| `Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe` | `c67e019e995c1656c42814e7cf9e3bfdc3431d5c103dc2446e24f6b7ba23d37b` |
| `Windows/WhiteoutStation/Content/Rules/WhiteoutStationRules.v0.5.json` | `c44caa45bc97b8d47795b84d50b300288d588cbdbe41127ad976242f909efaad` |
| `Windows/WhiteoutStation/Content/Agents/AgentRuntime.v0.5.json` | `b784d83a697ee9d2bba776fe73d1921b519a97a8eb2c0565c7005e334b8d7b79` |
| `Validation/ShippingSmoke/shipping_smoke_summary.json` | `d39e4b9ddbf48075c797f3c26c068f27adbd55013af3032bf6d5e56f10f0256a` |

## 验收

- `validate_source_v05.py --final`：PASS
- Win64 Shipping Build/Cook/Stage/Pak/Archive：PASS
- Shipping 烟测：5 / 5
- `validate_release_v05.py`：PASS
- 顾衡与叶澄受保护角色资产树：与基线一致
- 用户 `MVP_StationMap.umap` 工作树改动：保持未暂存
