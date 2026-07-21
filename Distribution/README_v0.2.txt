WHITEOUT STATION: BEFORE THE BLACKOUT — v0.2
风雪站：断电前夜

启动：Windows\WhiteoutStation.exe

操作
  WASD       移动
  鼠标       观察
  F          查看行动；再次按 F 确认
  Q          打开/关闭对话方式
  1—6        选择对话方式
  E          打开/关闭证据板
  Space      跳过开场
  Enter      结束本轮并结算
  R          开始新一轮
  Esc        暂停；选择退出，或直接按 Alt+F4

目标
  在 8 点行动力内修复发电机、校准室外天线并发出求救信号，
  同时照顾队员、物资、证据与承诺。

游戏可完全离线运行。存档与事件日志位于：
  %LOCALAPPDATA%\WhiteoutStation\Saved

可选的本地表达服务：
  WhiteoutStation.exe -WhiteoutAgentEndpoint=http://127.0.0.1:8765

模型只能改写对话措辞；确定性 C++ 规则拥有最终决定权，整局模型调用
上限为 10 次，任何失败都会回退到本地台词。仓库不含密钥。

免费资产的来源、作者、许可与校验值见随包 ASSET_LICENSES.md。
