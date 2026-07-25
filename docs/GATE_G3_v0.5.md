# G3 自动化与集成

状态：**PASS**

| 验证 | 结果 |
|---|---:|
| Python Agents | 39 / 39 |
| Python Rules | 28 / 28 |
| Python Release | 14 / 14 |
| UE Automation | 7 / 7 |
| UE warning / failed / not run | 0 / 0 / 0 |

- UE 报告：`Artifacts/TestResults/v05-final-20260725-212000`
- mock 覆盖 valid、empty、length、content filter、malformed、extra field、401、429→200、持续 429、500、503 和 delay。
- 持续 429 为初次请求加一次重试后本地降级；429→200 为一次重试后 accepted。
- 无 Key、loopback 凭据隔离、官方探针和顾衡/叶澄在线表达均通过。
