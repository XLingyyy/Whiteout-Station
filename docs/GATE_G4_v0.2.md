# G4 反馈与演出验收记录

验收日期：2026-07-21

## 结论

G4 通过。13 个热点均具备注视高亮、准星变化、名称与行动力成本；提交、拒绝和承诺建立形成独立视听反馈链。室内外环境床、三种表面脚步、发电机、UI、危机与四结局音频已分层接入。开场、危机和四结局演出均只读权威状态，不改变结算结果。

## 交互与演出证据

- `/Game/WindStation/Art/Materials/M_WS_InteractionOverlay`：半透明 Unlit 双面 Overlay；热点聚焦时启用覆盖材质与 Custom Depth，失焦即清除。
- `baseline_v0.2/UI_focus_available_1280x720.png`、`UI_focus_blocked_1280x720.png`：可执行/不可执行热点的高亮、菱形准星、名称和 AP 浮标对照。
- `UI_toast_commit_1280x720.png`、`UI_toast_promise_1280x720.png`：确认与承诺专用 toast、AP 前后值和颜色反馈。
- `UI_opening_{title,establishing,objective,controls}_1280x720.png`：标题、暴雪站房远景、三项目标与 8 AP 预算、操作说明四段连拍；空格可跳过并交还控制。
- `UI_crisis_{flash,blackout,emergency}_1280x720.png`：电压骤降、备用电池离线、应急负载接管三拍连拍；运行态灯光闪烁序列只触发一次。
- `UI_ending_{task,survival,cost,collapse}_cinematic_1280x720.png`：成功、等待、失控代价、全面崩溃四种差异化色调与文案，随后过渡至既有复盘页。

## 音频与许可

- `AWhiteoutAudioDirector` 按玩家区域连续混合室外风啸和室内闷风；发电机修复后开启设备循环，危机触发 stinger，成功结局播放无线电应答，四结局各有独立音乐床。
- 玩家位移按站区切换雪地、金属和混凝土脚步；HUD 接入悬停、确认、拒绝和承诺四种 UI 声音。
- `/Game/WindStation/Audio/Mix` 含 Master、Ambience、Foley、UI、Cinematic、Music 六级 SoundClass；Master 直接管理五个子类。
- 15 个 WAV 由 `Tools/Assets/generate_v02_audio.py` 确定性生成；其中室内风为已登记 CC0 风声的衍生，其余 14 个为本仓库原创程序音频。哈希、声道、采样率与时长见 `SourceAssets/GeneratedAudio/manifest.json`，许可见 `SourceAssets/ASSET_LICENSES.md`。

## 规则边界与运行态

- 高亮、灯光、暴雪和音频导演只读取预览、行动事件或状态快照；所有 AP、资源、任务与结局变更仍只由 `WindStationStateSubsystem` 提交管线执行。
- 运行态 PIE 发现 13 个 `WSRuntimeHotspot`、1 个 `WSRuntimeAudioDirector` 和 1 个 `WSRuntimeNiagaraBlizzard`；开场镜头在 14 秒后按设计交回玩家。
- 三条 AutoRoute 均在含新增音频、材质和演出的实际游戏进程中成功结算，未发现 `/Game/WindStation` 资源加载错误。

## 回归证据

| 检查 | 结果 |
|---|---|
| `python -X utf8 -m unittest discover -s Tools/Rules -p "test_*.py" -v` | 17/17 通过 |
| `python -X utf8 Tools/Release/validate_v02_rule_freeze.py` | 5 个冻结文件校验通过 |
| `python -X utf8 Tools/Release/validate_v02_strings.py` | 通过：236 条目，210 个引用键 |
| UE Automation `WhiteoutStation` | 6/6 通过，无项目错误与警告 |
| 运行态 `-WhiteoutAutoRoute=medical` | 成功，TaskSuccess，76.64 |
| 运行态 `-WhiteoutAutoRoute=technical` | 成功，TaskSuccess，71.90 |
| 运行态 `-WhiteoutAutoRoute=quick` | 成功，TaskSuccess，72.06 |
| `WhiteoutStationEditor Win64 Development` | 编译通过 |
