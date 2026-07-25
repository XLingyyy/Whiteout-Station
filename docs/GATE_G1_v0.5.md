# G1 AI 协议与安全

状态：**PASS**

- DeepSeek `deepseek-v4-flash` 请求显式关闭 thinking，使用非流式 JSON Object 输出。
- 模型正文只接受四个必填字段；动作、事实、envelope、finish reason 和大小均严格校验。
- API Key 只允许发送到 HTTPS `api.deepseek.com`；loopback 不携带 Authorization，未知远端拒绝启用。
- 连接、429、500、503 使用统一的最多两次传输尝试；失败均回退本地台词。
- 请求绑定 session、事务和 generation；新局、读档和销毁会取消旧请求。
- 审计不保存 Key、Authorization、玩家原文或完整模型回复，并在 2 MiB 轮转。
- 官方 DeepSeek 探针及顾衡、叶澄各一条 UE 在线表达均通过，证据报告 `secret_present=false`。
