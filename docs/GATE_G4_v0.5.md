# G4 构建与发布

状态：**PASS**

- 构建 worktree：detached、clean
- 源提交：`9bd94fab63f446290fbb5ababf809529a91c1b7c`
- 源树：`bc4ac95f5dd4120fbfa545c4d92719336bfa0ac9`
- Shipping：`Builds/WhiteoutStation-v0.5-Win64-20260725T134938Z-9bd94fab-release`
- Build / Cook 694 / 694 / Stage / Pak / Archive：PASS
- Shipping 烟测：5 / 5
- 源码门禁：PASS
- 发布清单校验：PASS
- 清单：`Validation/gate_manifest.json`

Shipping 覆盖三条成功路线、默认离线、启用但无 Key 和 loopback 断连。断连场景返回 HTTP 502 后使用确定性本地台词，规则路线仍为 TaskSuccess，传输尝试上限保持 2。

首次 UBA 并行链接因系统提交内存不足失败，未生成发布归档；降低并行度并关闭 UBA 后构建成功。两份未通过门禁的归档已可恢复地移至 `G:\CodexQuarantine\WhiteoutStation_v05_failed_artifacts_20260725`。
