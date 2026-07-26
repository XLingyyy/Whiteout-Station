# v0.6 发布检查表

日期：2026-07-26

最终候选：
`Builds/WhiteoutStation-v0.6-Win64-20260726T120458Z-4326f3ba-release`

包体源码：
`4326f3ba268de4846f1b7889ca84858daf70984b`

## 源码与范围

- [x] 独立 `codex/v0.6-polish` 工作树实施，用户主工作树未清理或覆盖。
- [x] `ProjectVersion`、规则、Agent Runtime 和存档槽均为 `0.6.0`。
- [x] 运行时只加载 `WhiteoutStationRules.v0.6.json` 与 `AgentRuntime.v0.6.json`。
- [x] 凭据扫描通过；源码、日志、截图和分发文件没有 API Key。
- [x] 5 个冻结对象 Git OID 与 `PROTECTED_CHARACTER_ASSETS_v0.6.json` 一致。
- [x] 顾衡与叶澄模型、骨骼、材质、动作、动画、AnimBP 和 LookAt 未修改。

## 功能

- [x] 阶段化意向与顾衡上下文化承诺完成。
- [x] 叶澄所有阶段无承诺，规则层拒绝直接 Promise。
- [x] 7 句开场由 Space/点击手动推进并最终淡出。
- [x] HUD 动态建议与 H 生存手册完成。
- [x] 全部人物状态、信任、AP 的影响与改变方式可查。
- [x] 保存、读取、帮助、设置、重开、退出流程可用。
- [x] FOV、四路音量、字号和减少动态效果可用。
- [x] AI 最近 4 轮、等待、取消、降级与凭据边界完成。
- [x] 四类结局与路线/代价/承诺复盘完成。

## 自动化

- [x] Python Agents：43 passed。
- [x] Python Rules：30 passed。
- [x] Python Release：15 passed。
- [x] UE Automation：8 / 8，0 warning，0 failed，0 not run。
- [x] 官方 `deepseek-v4-flash` 探针：HTTP 200、JSON 合同通过、凭据未回显。
- [x] 两轮对话历史：2 → 4 条消息，0 → 1 个历史轮次。
- [x] 真实输入：2 / 2。
- [x] Shipping 矩阵：10 / 10。

## 打包与清单

- [x] UE 5.8 Win64 Shipping Build。
- [x] Cook。
- [x] Stage。
- [x] Pak 与 IoStore。
- [x] Archive 到唯一 run-id 目录。
- [x] `README_v0.6.txt` 与 `ASSET_LICENSES.md` 随包。
- [x] 包内规则、Agent Runtime、启动器、Shipping EXE、PAK、UCAS、UTOC 存在且达到最小尺寸。
- [x] `Validation/gate_manifest.json` 已生成，覆盖 89 个文件。
- [x] `validate_release_v06.py --expected-source-ref 4326f3ba...`：PASS。
- [x] 分支已推送到 `origin/codex/v0.6-polish`。

## 复现命令

```powershell
python -X utf8 Tools/Release/run_v06_input_smoke.py `
  --artifact-root 'G:\WhiteoutStation-v06-worktree\Builds\WhiteoutStation-v0.6-Win64-20260726T120458Z-4326f3ba-release'

python -X utf8 Tools/Release/run_v06_shipping_smoke.py `
  --artifact-root 'G:\WhiteoutStation-v06-worktree\Builds\WhiteoutStation-v0.6-Win64-20260726T120458Z-4326f3ba-release' `
  --timeout-seconds 120

python -X utf8 Tools/Release/validate_release_v06.py `
  --repo-root 'G:\WhiteoutStation-v06-worktree' `
  --artifact-root 'G:\WhiteoutStation-v06-worktree\Builds\WhiteoutStation-v0.6-Win64-20260726T120458Z-4326f3ba-release' `
  --expected-source-ref '4326f3ba268de4846f1b7889ca84858daf70984b'
```

输入与 Shipping 烟测必须在 manifest 创建前运行；清单创建后再次执行会被工具拒绝，防止证据与校验和失配。
