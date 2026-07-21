# G3 角色呈现验收记录

验收日期：2026-07-21

## 结论

G3 通过。工程师顾衡与医生叶澄已由灰盒热点替换为写实冬装 SkeletalMesh；两套独立骨架、Animation Blueprint、四类基础动画和状态驱动姿态均已接入，原 ActionId、碰撞与固定点位保持不变。

## 资产与实现证据

- `/Game/WindStation/Art/Characters/Engineer/SK_WS_Engineer`：深蓝工程连体服、右手伤情包扎、独立 Skeleton 与 `ABP_WS_Engineer`。
- `/Game/WindStation/Art/Characters/Doctor/SK_WS_Doctor`：红色急救冬装、独立 Skeleton 与 `ABP_WS_Doctor`。
- 每名角色均有 `Idle`、`Gesture`、`Guarded`、`Work` 四个 30 fps `AnimSequence`；动作由状态快照和行动事件选择。
- NPC 在近距离自动转向玩家；顾衡低信任/受伤时护手，信任改善或接受治疗后使用开放手势；叶澄低信任时采用收拢防御姿态。
- 角色 FBX、纹理、动画源、生成脚本、哈希与 CC0/CC-BY 署名集中在 `SourceAssets/MakeHuman/`、`Tools/Assets/` 和 `SourceAssets/ASSET_LICENSES.md`。

## 视觉证据

- `baseline_v0.2/Character_Engineer_HighTrust.png`：顾衡开放伸手姿态与右手小型包扎。
- `baseline_v0.2/Character_Engineer_LowTrust.png`：顾衡护手、收拢姿态。
- `baseline_v0.2/Character_Doctor_HighTrust.png`：叶澄开放解释手势。
- `baseline_v0.2/Character_Doctor_LowTrust.png`：叶澄抱臂/防御式姿态。
- `baseline_v0.2/Lighting_Crisis.png`：红色应急光下的角色/空间剪影基调已建立；呼吸白气为 M2 氛围项，不作为 G3 门禁阻塞项。

## 契约与回归证据

- `AWSInteractableActor` 仍是统一热点 Actor；玩家提交继续走原预览/确认/事务管线。
- 角色表现只读状态，不直接修改信任、压力、资源、行动点或结局。
- 固定点位与交互提示保持不变，三条运行路线均可完成。

| 检查 | 结果 |
|---|---|
| Python 规则回归 | 17/17 通过 |
| 冻结哈希 | 5/5 通过 |
| UE Automation `WhiteoutStation` | 6/6 通过，无错误与警告 |
| medical / technical / quick 运行态路线 | 3/3 通过，TaskSuccess |
| 角色运行态基线 | 4/4 通过；无 T Pose、悬空、巨型绷带或缺失 SkeletalMesh |
| `WhiteoutStationEditor Win64 Development` | 编译通过 |
