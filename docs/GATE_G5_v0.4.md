# GATE G5 v0.4 — 回归与发布

日期：2026-07-22
门禁结果：**PASS**
整版发布状态：**RELEASED — G0 至 G5 全部门禁关闭**

## 全量回归

| 项目 | 结果 |
|---|---:|
| Python 规则 | 17 / 17 PASS |
| UE Automation | 6 / 6 PASS；0 warning / 0 failed / 0 not run |
| 权威规则冻结 | 5 / 5 PASS |
| Runtime AutoRoute | medical 76.64 / technical 71.90 / quick 72.06 |
| 无密钥 Shipping | 3 / 3 PASS；`model_calls=0` |
| 截图基线 | 70 视角 × 2 分辨率 = 140 / 140 PASS |
| 反馈修复后对照 | 9 / 9 PASS |
| 1080p Shipping 性能 | 109.34 FPS / 98.96 FPS 1% Low |
| 包体 | 42 文件 / 774,388,831 bytes（约 738.5 MiB）/ 2.5 GB 预算内 |
| 凭据扫描 | 当前索引 + 可推送历史 PASS |
| 发布校验 | `RELEASE VALIDATION v0.4: PASS` |

UE Automation 报告位于 `docs/evidence_v0.4/g5_automation/`；Shipping 事件、截图、无密钥汇总和性能数据位于 `docs/evidence_v0.4/g5_shipping/`。

## 基线

`Tools/Release/validate_v04_baseline.py` 验证：

- character 12、lighting 3、lookat 3、scene 10、ui 42；
- 1280×720 共 70 张，1920×1080 共 70 张；
- 9 张 `feedback_after` 全部存在；
- 图片尺寸与命名集合一致。

## Shipping 与无密钥

- 最终归档：`Builds/WhiteoutStation-v0.4-Win64/`。
- 三条路线均由清除 `WHITEOUT_LLM_API_KEY` / `WHITEOUT_LLM_ENABLED` 的独立子进程启动，并显式传入 `-WhiteoutLLMEnabled=false`。
- 三条路线结局均为 `TaskSuccess`，分数与 v0.3 完全一致。
- 默认 Agent 配置保持离线，模型固定 `deepseek-v4-flash`，发布包不含凭据字段。

## 已接受限制

两套指定 Idle 源动画均自带前臂前伸、手掌上翻姿势。根据 V4-10 的停止条件，未擅自换骨架或换动画。用户已于 2026-07-22 接受该姿势为 v0.4 已知限制，G3 已签字，详见 `docs/GATE_G3_v0.4.md`。除此之外没有发布保留项。
