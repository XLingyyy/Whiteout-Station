# G0 安全与联调基线

更新日期：2026-07-22

## 结论

密钥读取、忽略与扫描管线已建立。原始真人反馈 DOCX 检出 2 处 `sk-…`
形式的明文密钥，已从当前 Git 索引隔离；密钥值未进入日志。对应密钥应视为已泄露并立即轮换。

指定模型 `deepseek-v4-flash` 的探针固定使用 DeepSeek Chat Completions 端点。当前开发环境未注入轮换后的安全密钥，因此联调结论为
`SKIPPED_NO_SAFE_CREDENTIAL`；不会擅自使用反馈文档中的旧密钥，也不会静默更换模型。该结论不阻塞无模型降级链、Mock 路径和其余 v0.3 开发。

## 配置链

1. `WHITEOUT_LLM_API_KEY` 环境变量；
2. 被 Git 忽略的 `WhiteoutStation/LocalConfig/WhiteoutLLM.ini`；
3. 离线确定性台词与本地意图词典。

在线路径另需 `WHITEOUT_LLM_ENABLED=true`、本地 `Enabled=true` 或
`-WhiteoutLLMEnabled=true` 显式开启。AutoRoute 与发布烟测不依赖密钥。

## 验证命令

```powershell
python -X utf8 Tools/Release/scan_secrets.py
python -X utf8 Tools/Release/scan_secrets.py --history
python -X utf8 Tools/Agents/probe_deepseek.py
```

扫描器只输出命中文件/对象与模式类别，永不输出密钥正文；同时解压扫描 DOCX/XLSX/PPTX 内部 XML，避免 OOXML 压缩掩盖凭据。历史模式覆盖所有可推送分支、远端跟踪分支与标签；Codex 为任务恢复创建的非提交内部引用不属于发布历史，也不会推送。

## 回归记录

| 检查 | 结果 |
|---|---|
| 当前索引密钥扫描 | PASS |
| DeepSeek 探针 | `SKIPPED_NO_SAFE_CREDENTIAL`；模型名未替换 |
| Python 规则回归 | 17 / 17 通过 |
| UE Automation | 6 / 6 通过 |
| 规则冻结 | 5 / 5 MATCH |
| 中文 StringTable | 236 条、210 引用键，通过 |
| 无模型 AutoRoute medical | TaskSuccess，76.64，0 AP，0 次模型调用 |
| 无模型 AutoRoute technical | TaskSuccess，71.90，0 AP，0 次模型调用 |
| 无模型 AutoRoute quick | TaskSuccess，72.06，2 AP，0 次模型调用 |
| Editor Development 编译 | 通过 |

历史扫描在净化前准确检出旧提交中的反馈 DOCX。G0 提交后将仅移除该文件的所有历史版本，再复跑 `--history` 并同步远端。
