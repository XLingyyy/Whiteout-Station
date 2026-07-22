WHITEOUT STATION: BEFORE THE BLACKOUT — v0.4
风雪站：断电前夜

启动：Windows\WhiteoutStation.exe

操作
  WASD       移动
  鼠标       观察；在菜单、证据板与对话中选择
  F          与人物开始对话；查看/确认物体行动
  E          打开/关闭证据板
  Space      跳过开场；跳跃
  Enter      结束本轮并结算
  R          开始新一轮
  Esc        全局返回；游戏态打开暂停菜单
  Alt+F4     直接退出

目标
  从 08:15 开始，在 8 点行动力内修复发电机、校准室外天线并
  发出求救信号；每消耗 1 AP 推进 75 分钟，18:15 暴雪抵达。
  同时照顾队员、物资、证据与承诺。

界面与证据
  v0.4 使用统一的低遮挡毛玻璃界面。按 E 打开证据板后会释放
  鼠标，可点击五类过滤与证据卡查看完整详情；Esc 或 E 返回游戏。
  任意全屏面板打开时，主 HUD 会自动隐藏。

对话
  对准顾衡或叶澄后按 F，由 NPC 先说开场白。选择询问、质疑、
  承诺或安抚等主要意图；需要时可在输入框补充具体说法再发送。
  没有联网配置时使用本地确定性台词，完整流程不需要 API Key。

设置
  ESC 菜单可实时调节 75°—105° FOV，以及主音量、氛围、效果和
  反馈音量。设置只保存在玩家本机 GameUserSettings.ini。

存档与日志位于：
  %LOCALAPPDATA%\WhiteoutStation\Saved

可选的 DeepSeek 表达服务
  默认关闭。模型固定为 deepseek-v4-flash，只负责 NPC 对玩家
  说法的表达改写；确定性 C++ 规则拥有最终决定权，整局上限 10 次，
  失败会回退到本地路径。请通过 WHITEOUT_LLM_API_KEY 环境变量
  或本机 LocalConfig/WhiteoutLLM.ini 注入密钥，切勿写入游戏目录。

免费与生成资产的来源、工具、许可和校验值见随包
ASSET_LICENSES.md。
