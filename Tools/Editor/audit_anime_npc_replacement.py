import unreal


MAP_PATH = "/Game/WindStation/World/MVP_StationMap"
EXPECTED = {
    "talk_gu_heng": {
        "data_asset": "/Game/WindStation/Presentation/Characters/DA_WS_GuHeng_AnimeNPC.DA_WS_GuHeng_AnimeNPC",
        "mesh": "/Game/WindStation/Art/AnimeNPC/GuHeng/SK_Male_AvatarSample_C.SK_Male_AvatarSample_C",
        "animation_root": "/Game/WindStation/Art/AnimeNPC/GuHeng/Animations",
        "animation_prefix": "AN_GuHeng",
        "wrap_socket": "J_Bip_R_Hand",
    },
    "talk_ye_cheng": {
        "data_asset": "/Game/WindStation/Presentation/Characters/DA_WS_YeCheng_AnimeNPC.DA_WS_YeCheng_AnimeNPC",
        "mesh": "/Game/WindStation/Art/AnimeNPC/YeChengV10/SK_YeCheng_NoanoaHair_RefinedFace_v10.SK_YeCheng_NoanoaHair_RefinedFace_v10",
        "animation_root": "/Game/WindStation/Art/AnimeNPC/YeChengV10/Animations",
        "animation_prefix": "AN_YeCheng_V10",
        "wrap_socket": None,
    },
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def path_name(asset) -> str:
    return asset.get_path_name() if asset is not None else ""


for action_id, expected in EXPECTED.items():
    data_asset = unreal.load_asset(expected["data_asset"])
    mesh = unreal.load_asset(expected["mesh"])
    require(data_asset is not None, f"Missing data asset for {action_id}")
    require(mesh is not None, f"Missing skeletal mesh for {action_id}")
    require(
        data_asset.get_editor_property("skeletal_mesh") == mesh,
        f"Wrong mesh in data asset for {action_id}",
    )
    require(len(mesh.get_editor_property("materials")) >= 10, f"Too few materials for {action_id}")

    animations = data_asset.get_editor_property("animations")
    for state_name in ("idle", "gesture", "guarded", "work"):
        animation = animations.get_editor_property(state_name)
        expected_path = (
            f"{expected['animation_root']}/{expected['animation_prefix']}_{state_name.capitalize()}."
            f"{expected['animation_prefix']}_{state_name.capitalize()}"
        )
        require(path_name(animation) == expected_path, f"Wrong {state_name} animation for {action_id}")
        require(animation.get_play_length() > 0.0, f"Empty {state_name} animation for {action_id}")
        require(
            animation.get_editor_property("skeleton") == mesh.get_editor_property("skeleton"),
            f"Skeleton mismatch in {state_name} animation for {action_id}",
        )

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
require(world is not None, f"Unable to load {MAP_PATH}")

actors_by_action = {action_id: [] for action_id in EXPECTED}
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actor_subsystem.get_all_level_actors():
    if not isinstance(actor, unreal.WSInteractableActor):
        continue
    action_id = str(actor.get_editor_property("action_id"))
    if action_id in actors_by_action:
        actors_by_action[action_id].append(actor)

for action_id, actors in actors_by_action.items():
    require(len(actors) == 1, f"Expected one map actor for {action_id}, got {len(actors)}")
    actor = actors[0]
    expected = EXPECTED[action_id]
    expected_data_asset = unreal.load_asset(expected["data_asset"])
    expected_mesh = unreal.load_asset(expected["mesh"])
    require(
        actor.get_editor_property("character_asset") == expected_data_asset,
        f"Map actor data asset mismatch for {action_id}",
    )
    component = actor.get_editor_property("character_mesh")
    require(component.get_skeletal_mesh_asset() == expected_mesh, f"Map actor mesh mismatch for {action_id}")
    require(component.get_bone_index("J_Bip_C_Head") != -1, f"Head bone missing for {action_id}")
    require(component.get_bone_index("J_Bip_L_Hand") != -1, f"Left hand bone missing for {action_id}")
    require(component.get_bone_index("J_Bip_R_Hand") != -1, f"Right hand bone missing for {action_id}")
    if action_id == "talk_ye_cheng":
        require(component.get_bone_index("J_Adj_L_FaceEye") != -1, "YeCheng left eye bone is missing")
        require(component.get_bone_index("J_Adj_R_FaceEye") != -1, "YeCheng right eye bone is missing")
        hair_bones = [
            str(name)
            for name in component.get_all_socket_names()
            if str(name).startswith("HairJoint-")
        ]
        require(
            len(hair_bones) == 63,
            f"Expected 63 YeCheng V10 hair bones, got {len(hair_bones)}",
        )
        morph_targets = expected_mesh.get_all_morph_target_names()
        unreal.log(
            f"WS_YECHENG_MORPH_TARGETS count={len(morph_targets)} "
            f"names={list(morph_targets)}"
        )
        # Basis plus two zero-delta Neutral keys are not runtime morph targets.
        require(
            len(morph_targets) == 54,
            f"Expected 54 effective YeCheng morph targets, got {len(morph_targets)}",
        )
    idle = actor.get_editor_property("idle_animation")
    require(idle is not None and idle.get_play_length() > 0.0, f"Actor idle is missing for {action_id}")
    require(not component.get_editor_property("hidden_in_game"), f"Character mesh hidden for {action_id}")

    injury_wrap = actor.get_editor_property("injury_wrap")
    if expected["wrap_socket"]:
        require(not injury_wrap.get_editor_property("hidden_in_game"), "GuHeng injury wrap is hidden")
        require(
            str(injury_wrap.get_attach_socket_name()) == expected["wrap_socket"],
            "GuHeng injury wrap socket mismatch",
        )
    else:
        require(injury_wrap.get_editor_property("hidden_in_game"), "YeCheng injury wrap should be hidden")

unreal.log("WS_ANIME_NPC_AUDIT_COMPLETE result=PASS")
