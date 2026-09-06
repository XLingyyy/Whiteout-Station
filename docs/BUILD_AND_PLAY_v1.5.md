# v1.5 构建与验收记录

本轮在 `main` 开发。基线 `ae04a46`；规则修正 `3cdc927`；双模式与 HUD `f0544a1`；真实服务、DPI 和审计修正 `e969728`。

当前按候选版本管理。人工标注语义集、八人盲测和全部视觉／输入验收证据尚不齐全，不能标为完整发布通过。

最新可运行归档：`Artifacts/WhiteoutStation-v1.5-Win64-20260906-8ba299e-candidate/Windows/WhiteoutStation.exe`。包含响应式面板、明确质疑的替代件选项、旧控件移除与窗口失焦清理。下方旧包和失败批次保留为历史验证记录。

## 运行与配置

工程：`WhiteoutStation/WhiteoutStation.uproject`，UE 5.8，VS 14.44.35228，Windows SDK 10.0.22621.0。

默认关闭 AI，靠作者选项完成对话。对话最多三轮，首轮成功提交 1 AP，后续两轮不重复收费。在线模式在设置页填写 provider、BaseURL、model 和仅保留于本进程的 Key 后启用。在线按 A 意图解析、局部规则规划、B 表达生成顺序处理，A 最多 3 秒／256 tokens，B 最多 7 秒／640 tokens，整轮最多 10 秒，无自动重试。DeepSeek 显式关闭默认 thinking，避免解析预算全部被推理占用。

离线不会调用 HTTP。A 失败不提交，B 失败只允许匹配当前语义与状态的作者替代；玩家明确切离线时保留会话与收益上限。Enter 在文本框内换行，点击发送提交，Esc 离开。

运行配置 `Content/Agents/AgentRuntime.v1.5.json`：schema 8、`bounded_roleplay_v5`、运行时 `1.5.0`。九份 `Content/Dialogue/v1.5/*.json` 必须通过 NonUFS 打包。存档槽 `WhiteoutStation_Autosave_v1_5`，向后读取 v1.4、v1.3、v1.2、v1.1；请求和会话不会跨读档恢复。

## 实际命令

在仓库根目录运行：

```powershell
python -X utf8 Tools/Dialogue/validate_dialogue_v15.py
python -X utf8 Tools/Release/validate_source_v15.py --contract-only
python -X utf8 -m pytest Tools/Dialogue/test_dialogue_v15.py -q

& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat' WhiteoutStationEditor Win64 Development 'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' -WaitMutex -NoHotReloadFromIDE

& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests WhiteoutStation;Quit' '-TestExit=Automation Test Queue Empty'
```

作者路线使用 `-game -NullRHI -WhiteoutV15AuthoredRoute -WhiteoutAutoRoute=medical -WhiteoutAutoRouteExit`，路线名为 `medical`、`technical`、`quick`、`wait`、`collapse`。该标记让对话步骤提交真实 ChoiceId，仍执行既有路线动作和结局检查。

真实服务探针使用 `-WhiteoutV15Probe=<绝对 JSON 路径>`。输入可为 `{"text":"…","action":"talk_gu_heng"}`，或 `{"cases":[…]}`。仅显式测试时读取进程环境 `WHITEOUT_V15_TEST_KEY`，使用 DeepSeek 官方端点和 `deepseek-v4-flash`。结果写到输入旁的 `.result.json`，批次带三位序号。该测试会开始新局，不应在个人游玩进程中启用。探针结果保留合成测试台词；普通对话审计仅记录白名单字段，不保存原文或 Key。

Shipping：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun '-project=G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' -noP4 -nocompileeditor -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -utf8output '-archivedirectory=G:\Whiteout Station\Artifacts\WhiteoutStation-v1.5-Win64-20260906-e969728-candidate'
```

包内容验证：`python -X utf8 Tools/Release/validate_source_v15.py --package-content '<归档目录>/Windows/WhiteoutStation/Content'`。此门禁验证运行配置与作者内容，不替代真实模型、碰撞、IME 或人工验收。

## 已取得的证据

- Editor Development 编译成功；Python 作者内容测试 2/2。
- UE 全部 64 项：首次 63/64，旧审计白名单未包含三个新增非敏感 ID 字段；修正白名单后定向复测 1/1。报告分别为 `Saved/AutomationReports/V15ReleaseCandidate` 与 `V15AuditFinal`。未删测试。
- Editor 五条真实作者选项路线均成功：医疗 74.70／TaskSuccess，技术 71.44／TaskSuccess，风险 58.94／CostUncontrolled，等待 36.92／SurvivalWait，失控 42.16／TotalCollapse。
- 2026-09-06 真实 DeepSeek 第二批普通输入 100 条：93 条提交，其中 69 条模型表达、24 条合格作者替代；7 条失败计入分母。p95 2.604 秒，最大 2.944 秒，错误 AP 和意外承诺均 0。样本由助手预设，尚未经人工标注；该批低于 95% 门槛。
- 针对第二批的七条失败输入修正人物问题澄清判定后，7/7 定向复测提交。不能用这七条覆盖原批次失败或改写原批次通过率。
- 实机检查离线入口、诊断选项、轮数、退出恢复、暂停隐藏状态卡；100% 与 150% 实际字号已截图。修复旧设置仅缩放 Slate、未缩放游戏 UMG 的问题。截图在 `Artifacts/v1.5-evidence/screenshots`，不当作全分辨率验收。
- 地图及 NPC 模型／材质共 262 个受保护文件，按专门约定记录并复核，未变化。

## Shipping 与最终真实服务结果

归档 `Artifacts/WhiteoutStation-v1.5-Win64-20260906-e969728-candidate` 构建成功。首次 UAT 因仍运行的 Editor 测试进程占用 DLL 而失败；第二次加 `-nocompileeditor`，复用已验证的编辑器模块，完成 Shipping 编译及完整 Cook／Stage。包内 v1.5 运行配置和九份对话内容门禁为零错误。

Shipping 五条路线全部成功，结局和评分与 Editor 一致；所有路线 `model_calls=0`，医疗／技术／风险分别有 3／2／1 条 `authored_v15` 提交。独立 Shipping 的普通 stdout 不包含路线 Display 日志，因此以本次新生成的事件导出及结束状态为依据；初次 stdout 采集未形成有效证据，随后按事件导出重跑并留存。实际入口：

```powershell
python -X utf8 Tools/Release/run_v15_authored_routes.py --exe 'Artifacts/WhiteoutStation-v1.5-Win64-20260906-e969728-candidate/Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe' --output 'Artifacts/v1.5-evidence/shipping-routes'
```

最终 100 条真实服务批次为 **88/100**，其中 71 条模型表达、17 条作者替代，12 条失败。p95 2.650 秒，最大 2.909 秒，错误 AP／意外承诺／意外诊断均 0。包含 10 次表达无合格结果、1 次非法意图字段、1 次澄清；测试期间曾同时尝试 UAT，不据此推断失败原因。定向检查确认普通医疗状态问句可能产生空目标动作，无法匹配以维修能力为对象的作者台词；模型还会将维修进度归到准备条件。此结果低于目标，DeepSeek 在线模式保留实验性质，未认证为达到 95% 有效回复率。完整摘要在 `Artifacts/v1.5-evidence/normal100_final_summary.json`。

## 尚未通过的发布条件

120 条人工标注中文语义集、独立安全集全覆盖、三条完整在线／离线等价轨迹、全部 D/H 实机用例、完整中文 IME 快捷键矩阵、UI CPU 统计及八人交叉盲测，均须按实际证据补齐。多分辨率八类画面已生成，具体范围见后续记录。当前自动化和合成输入不能替代人工项目。

旧构建与失效缓存清理已核对保留范围，但删除操作遭自动审批审查拒绝，返回 `blocked by policy`，未给出进一步理由。本轮未绕过拒绝，旧归档仍在。保留的 v1.4 已验证包为 `Artifacts/WhiteoutStation-v1.4-Win64-20260905T105713Z-71bd524e-final`。


## Shipping 实机补充检查

在 `e969728` 包中，设置页读取用户指定测试密钥并应用后，在线人物闲聊成功回复，界面显示首轮 1 AP、剩余 2 轮。实际中文输入法候选已出现，R 未触发重开，Enter 在无候选时换行，Esc 在有候选时先取消候选且保持对话。该检查不涵盖全部输入法及快捷键组合。

同时复现两项 UI 问题：切离线后角色仍以全局 AI 模式拒绝作者选项；限制多行框高度后发送按钮误读了子控件可见性。`9168212` 修正作者选项提交入口和发送按钮显隐，并修复默认紫字浅底、延迟一帧设置输入焦点。新归档实机确认：在线回复一轮后切离线，两次作者追问都提交成功，三轮结束后 AP 为 3/4；离线发送按钮隐藏，输入文字为深底浅字。延迟一帧仍未解决首次输入焦点，另行修正并验证。证据为 `shipping-online-final.png`、`shipping-offline-recovery-final.png`、`shipping-three-turns-ap.png`。

归档 `Artifacts/WhiteoutStation-v1.5-Win64-20260906-9168212-candidate` 复用前一次已成功且内容未变化的 Cook，仅重新编译 Shipping、Stage 和归档。该包现为历史验证产物；运行请选本文开头的最新包。

## 意图合同与焦点后续修正

原 12 条失败输入重新记录 query／目标字段后，确认安慰句常被解析为 unknown 查询类型，医疗状态目标动作也在空值／治疗／维修之间变化。补充话题合同，作者安慰选项改为 unknown 查询；六字段表达协议的知识上下文过滤掉无法附带 belief／withheld 断言的知识。原事实、引用与关键片段校验保持不变。12/12 定向复测提交。

随后完整 100 条同一合成输入批次 **100/100 提交**：77 条模型表达、23 条合格作者恢复；p95 **2.965 秒**，最大 **3.634 秒**。AP、诊断与承诺均与逐条预期一致。摘要为 `Artifacts/v1.5-evidence/normal100_contract_summary.json`。这证明本批次达到吞吐门槛，不替代未参与调优的人工标注集与玩家体验评价；旧 88/100 记录保留。

自动聚焦使用 UUserWidget 的 DesiredFocusWidget，将父面板的延迟焦点转交输入框；移除无效的 NextTick 焦点重试。Editor 实机首次打开后未点击输入框即可输入。中文候选 Esc 取消后仍保留对话；Space 确认“人”，Enter 在无候选时换行，在候选时确认输入；R/C/E/H 未触发对应游戏动作，未发送退出后 AP 仍为 4/4。文本编辑末尾一次 Esc 可能先清除选区，第二次离开；不能将其记作所有编辑态单次 Esc 验收通过。截图为 `editor-auto-focus-ime.png` 与 `editor-ime-shortcuts.png`。

## 响应式布局与两阶段故障注入

状态卡和对话接入 SafeZone，预留状态卡 320 逻辑单位与间隙；圆角 6、细边框 1，使用局部颜色 token。诊断来源由无法鼠标触达的 Tooltip 改为卡内文字。NPC 卡淡入 0.12 秒；失效目标立即清除，对话淡出 0.10 秒。Reduced Motion 使用即时显示。新增 `WhiteoutV15StatusPresent` CPU trace scope，仅测状态更新函数，尚不代表完整 Slate 开销。

Editor 编译通过，`Saved/AutomationReports/V15LayoutFinal` 的 5 项 v1.5 定向测试全部成功。实际 UE 渲染矩阵为 1280×720、1366×768、1920×1080、2560×1440、3440×1440，各有 100%、125%、150% 字号，每组八类画面，共 120 张；PNG 实际尺寸全部与请求相符。代表性截图检查覆盖五种分辨率和三档字号，未见新面板越界或遮挡发送／离开区。截图在 `Artifacts/v1.5-evidence/layouts`，矩阵索引 `matrix.json`。这些画面通过状态呈现夹具生成，用于布局检查，不作为真实准星、碰撞、在线请求或真人验收证据。

截图命令示例（其余组合替换尺寸与 scale）：

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' -game -RenderOffScreen -windowed -forceres -ResX=1280 -ResY=720 -unattended -nosplash -nop4 -WhiteoutPresentationCapture=v15suite -WhiteoutCaptureScale=1.5
```

`Tools/Release/run_v15_http_faults.py` 启动本地 loopback HTTP 服务并驱动真实 UE A/B 请求。`Artifacts/v1.5-evidence/http-faults-contract/summary.json`：15/15 通过，覆盖成功、A/B 坏 JSON、HTTP 500、超时、缺少／非法 claim、自由文本越权、B 无合格替代，以及 A/B 等待时取消／新局后迟到响应。请求数为 1 或 2，无重试，无 Authorization；预算分别为 256/0 与 640/0.45。A 超时 3.002 秒；B 超时整轮 7.007 秒。取消／新局后无迟到状态变化。首版 mock 漏 `finish_reason`，未形成有效 B 阶段测试；失败输出保留于 `http-faults`，随后按实际提供商响应合同修正 mock，未放宽引擎解析。

```powershell
python -X utf8 Tools/Release/run_v15_http_faults.py --exe 'G:/UnrealEngine/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' --project 'WhiteoutStation/WhiteoutStation.uproject' --output 'Artifacts/v1.5-evidence/http-faults-contract'
```

三条真实在线／离线等价测试入口为 `Tools/Release/run_v15_equivalent_routes.py`，参数同上，另外显式传 `--key-file`。在线只读取作者选项的自然文本，经实际 A/B 链路提交；承诺在第一次提议时检查 AP、承诺数、revision 均不变，再发送明确确认。比较最终完整规则状态及逐步 AP／承诺／披露事件，排除交易 ID、台词和请求计数。

替代部件文案修正后的五条 Editor 作者路线再次全部成功，评分仍为 74.70／71.44／58.94／36.92／42.16，每条 `model_calls=0`；结果在 `Artifacts/v1.5-evidence/editor-routes-contract`。作者路线脚本新增可选 `--project`，用于同一个入口运行 Editor 与 Shipping。

首次对照暴露了承诺确认、命令和替代部件语义不一致。补充 A 的字段合同；替代部件作者选项原本是中性问句却标成 challenge，现改为明确质疑台词，保留技术路线既有结算。将该选项临时改成 ask 的尝试导致技术路线无法披露替代件，该改动已撤回。原失败报告保留 `equivalent-routes` 与 `equivalent-routes-contract`。后一次在线请求均在 A 阶段达到 3 秒超时；独立 Python 请求和 curl 同时出现 TLS 握手断开，尚不能归因于提供商整体故障，在线等价仍未通过。没有提高生产超时上限或增加自动重试。

## 最新 Shipping 包验证

`8ba299e` 候选包完成 Shipping 编译、完整 Cook、Stage 和归档，用时 194 秒。包内运行配置与九份作者对话数据门禁零错误。五条打包后作者路线全部成功，结局与既有基线一致，评分为 74.70／71.44／58.94／36.92／42.16，所有路线模型调用为零。报告：`Artifacts/v1.5-evidence/shipping-8ba299e-routes/summary.json`。

该 Shipping 包实机确认缺少进程 Key 时可切换离线，更新后的替代件质疑文案完整显示；首轮作者回复成功、剩余两轮，Esc 离开后 AP 从 4/4 变为 3/4。截图为 `shipping-8ba299e-first-turn.png` 与 `shipping-8ba299e-ap.png`。测试使用已明确标记的开局／定位夹具，交互和提交走实际游戏逻辑。

受保护的 262 个文件最终复核无变化，记录为 `Artifacts/v1.5-evidence/protected-assets-final.json`。Git 在 2026-09-06 14:01 再次推送失败，错误为 `schannel: failed to receive handshake, SSL/TLS connection failed`；远端最后成功推送仍为 `f1e7d41`，后续提交保存在本地主分支。

## 旧控件移除与窗口失焦

新增真实碰撞场景测试 `WhiteoutStation.UI.V15.Focus.CollisionAndLease`，验证碰撞表面 300 cm 获取距离、150 ms 获取延迟、340 cm 保持距离、墙体遮挡立即清除、200 ms 小角度宽限、视线切换、目标销毁及应用失焦状态。首次测试第二个 NPC 的斜向表面距离超出 300 cm，修正夹具位置并补齐临时世界上下文后，`Artifacts/v1.5-evidence/V15FocusGeometryFinal` 1/1 通过，零警告；原报告保留。该修改仅增加开发自动化测试与 friend 声明，不改变 Shipping 行为，因此未重复打包。

移除旧态度轮盘、承诺菜单、单行输入框及其回车提交代码；追问刷新直接转交 v1.5 面板。旧截图入口也使用新面板。原提示词防泄露测试改为检查当前输入框文案，保留原七项禁用事实词断言，另检查回车换行与点击发送说明；`Saved/AutomationReports/V15LegacyCleanup` 1/1 通过。

实机复现 Alt+Tab 后旧对话保持。修正为监听 Slate 应用激活事件，失焦立即取消未完成会话、清除 NPC 卡、预览与交互提示，恢复后按原获取延迟重新注视。编译成功，Editor 实机确认返回后对话已关闭、AP 仍为 4/4，随后重新获取顾衡卡；两轮离线对话完成后 AP 为 3/4，追问刷新正常。证据为 `editor-background-before.png`、`editor-reactivated-after.png`、`editor-focus-reacquired.png`、`editor-followup-cleanup.png`、`editor-followup-ap.png`。截图中的 GUI 窗口实际为 1280×720，系统缩放后捕获约 1922×1128；不混作原生分辨率矩阵。
