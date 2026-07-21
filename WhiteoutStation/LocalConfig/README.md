# 本地 LLM 配置

`WhiteoutLLM.ini` 仅用于开发机，不会进入 Git、Cook 或发布包。复制
`WhiteoutLLM.ini.example` 后填写轮换后的密钥。运行时读取优先级为：

1. 环境变量 `WHITEOUT_LLM_API_KEY`；
2. 本目录的 `WhiteoutLLM.ini`；
3. 无密钥离线降级。

是否启用在线模型由 `WHITEOUT_LLM_ENABLED`、本地 ini 的 `Enabled` 或命令行
`-WhiteoutLLMEnabled=true` 控制。自动化与 AutoRoute 必须保持禁用。

推荐仅为当前终端临时设置：

```powershell
$env:WHITEOUT_LLM_API_KEY = "<轮换后的密钥>"
$env:WHITEOUT_LLM_ENABLED = "true"
```

不要把密钥写进 `Default*.ini`、Content JSON、文档、截图、日志或命令行。
