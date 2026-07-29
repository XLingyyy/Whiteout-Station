# Whiteout Station v0.8 - 进度总结

日期：2026-07-29

状态：已完成

分支：`codex/v0.8-evolution`

## 版本结果

v0.8 已形成可从十句开场、探索、状态管理、行动交互和 AI 对话推进到四类
结局的 Windows Shipping 候选包。本轮完成：

- 修复顶部警报文字越界和首要目标面板错位；
- 缩小空心圆准心，保持手形准心和交互提示居中；
- 所有面板打开、关闭时将系统鼠标重置到视口中心；
- 将健康、体温、精力、饱腹、压力/稳定和信任统一迁移到 0—10；
- 重写十句手动推进开场，明确顾衡、叶澄的职责、分歧、合作原因和求救目标；
- 清除旧 `M_WS_Eye` 材质覆盖，恢复两名 NPC 各自的导入眼部材质；
- 为顾衡、叶澄的当前精确骨架各制作待机、行走和五类反应，共 14 个动画；
- 让模型选择固定枚举内的移动与反应，本地 C++ 继续控制碰撞、距离和权威状态；
- 新增墙面混凝土和地面金属板材质，并只替换运行时几何的旧默认材质；
- 完成 DeepSeek V4-Flash 官方端点实测、离线降级、历史和表演探针。

## 用户内容保护

- 用户调整后的
  `/Game/WindStation/World/MVP_StationMap` 保持开工基线 Git 对象；
- 顾衡、叶澄源模型和人物绑定保持开工基线；
- 保护目录只新增两个 `AnimationsV08` 目录中的 14 个动画；
- 运行时材质修复和场景材质升级没有重写用户地图；
- 基线与保护清单记录在 `docs/USER_BASELINE_v0.8.json` 和
  `docs/PROTECTED_CHARACTER_ASSETS_v0.8.json`。

## 磁盘清理

- 删除干净且不影响当前版本的
  `G:\WhiteoutStation-v05-build-11f2129`；
- 删除干净且不影响当前版本的
  `G:\WhiteoutStation-v06-worktree`；
- 共释放约 14.74 GiB；
- 保留用户主工作树、v0.7 回滚工作树、v0.8 工作树和最终候选包。

## AI 与表演

模型输出固定为：

`npc_line`、`emotion`、`used_action_id`、`referenced_fact_ids`、
`movement_intent`、`reaction_action`。

移动枚举为 `stay`、`step_closer`、`step_back`、`return_to_post`；反应枚举为
`neutral`、`acknowledge`、`consider`、`reassure`、`reject`、`alarmed`。
C++ 独占 AP、资源、事实、承诺、任务、评分、结局和 NPC 最终坐标。

DeepSeek 官方端点以 `deepseek-v4-flash` 完成一次脱敏在线探针，HTTP 200，
输出契约有效。桌面密钥只注入子进程环境，未写入命令行、日志、源码、证据、
发布包或 Git 历史。

## 验证状态

| 验证层 | 结果 |
|---|---:|
| Agent Python 测试 | 48 passed |
| Rules Python 测试 | 30 passed |
| Release Python 测试 | 18 passed |
| 敏感信息规则测试 | 4 passed |
| UE Automation | 8 passed |
| 精确骨架动画审计 | 14/14 passed |
| 720p/1080p 准心状态审计 | 4/4 passed |
| Shipping 真实输入场景 | 2/2 passed |
| Shipping 路线、降级、历史与表演探针 | 12/12 passed |
| 源码门禁、LFS、保护目录、敏感信息扫描 | PASS |
| 最终发布包门禁 | PASS |

准心像素审计最大误差 1.5 px；六次面板打开/关闭后的鼠标回中实测误差均为
0 px。

## 发布候选

- 根目录：
  `G:\WhiteoutStation-v08-worktree\Builds\WhiteoutStation-v0.8-Win64-20260728T194955Z-6f131d00-release`
- Run ID：`20260728T194955Z-6f131d00-release`
- Shipping 源码提交：`6f131d006a6a8ec825582b8f230ad8d358ed2986`
- 源码树：`715e517ecc74eb1ebbfe006f4e87f539db2a2dcf`
- 规模：153 个文件，866,348,292 bytes（826.21 MiB）
- 分发等级：`local_review_only`

## 当前边界

- 尚无独立外部玩家可用性样本；
- 在线回复依赖 DeepSeek 服务，失败时会确定性降级；
- Windows 可执行文件尚未代码签名；
- 叶澄 Noanoa 发型缺少产品嵌入和再分发书面许可，候选包只允许本机私人开发
  与评审，不能上传或转发。
