WHITEOUT STATION: BEFORE THE BLACKOUT — v0.5
风雪站：断电前夜

启动：Windows\WhiteoutStation.exe

操作
  WASD       移动
  鼠标       观察；在菜单、证据板与对话中选择
  F          与人物开始对话；查看/确认物体行动
  Q          在行动预览中切换食物分配或治疗资源
  E          打开/关闭证据板
  Space      跳过开场；跳跃
  Enter      结束本轮；提前失败结算需再次确认
  R          开始新一轮
  Esc        全局返回；游戏态打开暂停菜单
  Alt+F4     直接退出

目标
  从 08:15 开始，在 8 点行动力内修复发电机、校准室外天线并
  发出求救信号；每消耗 1 AP 推进 75 分钟，18:15 暴雪抵达。
  同时照顾队员、物资、证据与承诺。

v0.5 玩法
  对行动物体第一次按 F 打开规则预览，第二次按 F 确认提交。
  食物可在玩家、顾衡、叶澄及两人组合间分配；治疗顾衡时可在
  药品和保温包间选择。顾衡支持询问、质疑、承诺、安抚；叶澄
  支持询问、质疑、安抚。无效意图、条件或重复承诺不会消耗 AP。

界面与证据
  按 E 打开证据板后会释放鼠标，可点击五类过滤与证据卡查看
  完整详情；Esc 或 E 返回游戏。任意全屏面板打开时主 HUD 隐藏。

离线与 DeepSeek 表达
  默认完全离线，NPC 使用本地确定性台词，完整流程不需要 API Key。
  可选模型固定为 deepseek-v4-flash，只负责 NPC 台词表达；AP、
  资源、事实权限、承诺、任务、评分和结局始终由 C++ 规则决定。
  模型回复只接受 npc_line、emotion、used_action_id 和
  referenced_fact_ids 四个字段，整局最多 10 次，任何失败均回退
  本地台词。

  官方服务启用示例：
    设置 WHITEOUT_LLM_API_KEY
    设置 WHITEOUT_LLM_ENABLED=true
    启动 WhiteoutStation.exe

  本地 Mock 示例：
    WhiteoutStation.exe -WhiteoutLLMEnabled=true ^
      -WhiteoutAgentEndpoint=http://127.0.0.1:8765/chat/completions

  密钥只发送给主机严格等于 api.deepseek.com 的 HTTPS 端点；
  loopback 永不携带 Authorization，其他端点拒绝。请勿把密钥写入
  游戏目录、命令行、日志或分发文件。

设置与本地数据
  ESC 设置页可实时调节 75°—105° FOV，以及主音量、氛围、效果和
  反馈音量。本地存档与日志位于：
    %LOCALAPPDATA%\WhiteoutStation\Saved

角色资产范围
  v0.5 未修改顾衡与叶澄的模型、骨骼、材质、动画或动作资产；
  后续可独立替换角色外观，不影响本版规则和 AI 表达接口。

免费、生成及第三方资产的来源、工具、许可和校验值见随包
ASSET_LICENSES.md。
