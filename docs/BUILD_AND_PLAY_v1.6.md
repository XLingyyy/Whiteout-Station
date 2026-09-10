# v1.6 构建与验收

UE 5.8 / Windows 64-bit，直接在 main 迭代。默认离线可玩；在线采用 `natural_roleplay_v6`，配置 schema 9，规则 schema 8。当前构建路径与验证结果见 [2026-09-10 发行记录](RELEASE_v1.6_20260910.md)，具体参数见 [编辑器修复与重平衡](Whiteout_Station_v1.6_编辑器修复与重平衡.md)。

## 使用与存档

所有交谈 0 AP；每名 NPC 每局 10 轮，在线、离线、重开会话和跨阶段共享。正常澄清、提出或修改承诺计一轮；有效提议的纯“确认”／“取消”不占新轮，也不请求模型。第十轮后混入新问题或修改条款会被拒绝。重复普通交谈不增加信任，行为效果按本局账本限制。

治疗、包扎、临时支持、维修等实际动作在行动面板选择并确认：F 打开、Q 切换、F 执行、Esc 取消。界面展示准确费用，不提前展示行动结果；费用改变后需要再次确认。对话中的意愿和方案不会消耗药品、修复设备或完成治疗。

保留三阶段各 4 AP；初始食物为 3 份，固定 1 AP 可给 1—3 人各分一份。供暖区主动休息立即体温 +1、体能 +1、压力 −0.4，满体能仍可回温。顾衡协查控制柜可获得一次维修准备。结算采用任务 30、人员 40、有效储备 10、社会稳定 12、信息责任 8 的权重；完成任务后照顾好三人有助于获得高分。旧存档不补发物资、AP 或准备，按新规则重算当前评分并提示。

自动存档槽 `WhiteoutStation_Autosave_v1_6`，依次向后读取 v1.5 至 v1.1。v1.5 以唯一、已提交的原文交流重建已用轮次，不退还已用 AP。历史缺失时只计算可证实交流。读档取消在途请求和未确认提议，已提交历史、额度、承诺与真实动作状态保留。v1.6 存档不降级写回旧槽。

在线在设置页填写 provider、BaseURL、model 与本次进程内存中的 Key，开启模型调用后向下滚动，点击“应用模型设置”。默认模型 `deepseek-v4-flash`，关闭 thinking，温度 0.45；A 意图解析温度 0。常规最多两次请求、10 秒；关键完整台词独立核查温度 0，最多第三次请求、15 秒。输出预算 A/B/C 分别 1600/1000/700 tokens。失败无自动改写或重试，保留草稿，不提交额度或关系效果。

## 协议与来源

模型 B 一次生成完整 `npc_line`，附 `addressed_goal_ids`、`referenced_fact_ids`、`action_proposal_ids`、`emotion`、`reaction_action`。这些标签不构成事实证明。每个回答目标独立筛选知识；关键回复再核查全文与人物、当前状态、已提交事件、提议和历史。披露只取核查器独立识别且本地获准的事实。

核查响应包括 `safe`、`issues`、`expressed_fact_ids`、`addressed_goal_ids`、`corrects_entry_id`、`event_claims`。最后一项提取治疗、检查、维修的对象、方法和完成状态，本地再次与真实记录比较；即使核查器返回 safe=true，只要提取出虚构“初步处理”或正在自主检查的断言，本地仍会拒绝。模型漏提取或误读仍是已知限制，不能保证自由文本无事实错误。动作提议和未来意愿不作为已执行断言。

历史仅传给该 NPC，玩家转述带未核实约束。真实治疗／维修行动记录包括执行者、对象、方法及事务 ID；目前这些站内行动按公开事件处理，其他 NPC 的私聊原文不共享。历史错误保留原文，纠正回复通过 `CorrectsEntryId` 关联。

未提供可自动提交的通用在线回退；玩家可切换作者选项。未支持复杂条件的承诺仍保持安全拒绝，不自动替换条款。八名陌生玩家自然度评分需真人参与，脚本化测试不充当盲测。

## 复现入口

仓库根目录执行：

```powershell
python -X utf8 Tools/Release/validate_source_v16.py
python -X utf8 Tools/Release/scan_secrets.py

& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat' WhiteoutStationEditor Win64 Development 'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' -WaitMutex -NoHotReloadFromIDE

& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests WhiteoutStation;Quit' '-TestExit=Automation Test Queue Empty'

python -X utf8 Tools/Release/run_v16_conversations.py --key-file '<本地密钥文件>' --cases docs/QA/v1.6_conversation_cases.json --output Artifacts/v1.6-evidence/real-conversations
```

真实服务脚本使用隔离 UserDir，密钥只经子进程环境读取；测试会创建新局。`@treat_full`、`@bandage`、`@rest_doctor` 等夹具步骤执行真实规则动作，统计时与消息区分。`setup_incorrect_history` 仅在显式测试入口植入一条旧错误台词以检验纠错，不创建治疗事件。默认对话日志不写原文；显式 `-WhiteoutDialogueDebug` 在隔离测试目录保存无 Authorization 的请求和响应。

Shipping 使用现有 BuildCookRun，添加 `-nocompileeditor` 复用已验证的 Editor 模块：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun '-project=G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' -noP4 -nocompileeditor -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -utf8output '-archivedirectory=<绝对归档目录>'
python -X utf8 Tools/Release/validate_source_v16.py --package-content '<归档目录>/Windows/WhiteoutStation/Content'
python -X utf8 Tools/Release/run_v15_authored_routes.py --exe '<归档目录>/Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe' --output Artifacts/v1.6-evidence/shipping-routes
```

五路线工具名称沿用 v15，实际读取当前包内容，作者台词审计来源仍为 `authored_v15`；在线自然表达来源为 `natural_roleplay_v16`。
