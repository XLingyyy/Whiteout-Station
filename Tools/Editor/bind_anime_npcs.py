import unreal


ASSET_ROOT = "/Game/WindStation/Presentation/Characters"
MAP_PATH = "/Game/WindStation/World/MVP_StationMap"

CHARACTERS = {
    "talk_gu_heng": {
        "asset_name": "DA_WS_GuHeng_AnimeNPC",
        "skeletal_mesh": "/Game/WindStation/Art/AnimeNPC/GuHeng/SK_Male_AvatarSample_C.SK_Male_AvatarSample_C",
        "animations": {
            "idle": "/Game/WindStation/Art/AnimeNPC/GuHeng/Animations/AN_GuHeng_Idle.AN_GuHeng_Idle",
            "gesture": "/Game/WindStation/Art/AnimeNPC/GuHeng/Animations/AN_GuHeng_Gesture.AN_GuHeng_Gesture",
            "guarded": "/Game/WindStation/Art/AnimeNPC/GuHeng/Animations/AN_GuHeng_Guarded.AN_GuHeng_Guarded",
            "work": "/Game/WindStation/Art/AnimeNPC/GuHeng/Animations/AN_GuHeng_Work.AN_GuHeng_Work",
        },
        "injury_wrap": True,
    },
    "talk_ye_cheng": {
        "asset_name": "DA_WS_YeCheng_AnimeNPC",
        "skeletal_mesh": "/Game/WindStation/Art/AnimeNPC/YeChengV10/SK_YeCheng_NoanoaHair_RefinedFace_v10.SK_YeCheng_NoanoaHair_RefinedFace_v10",
        "animations": {
            "idle": "/Game/WindStation/Art/AnimeNPC/YeChengV10/Animations/AN_YeCheng_V10_Idle.AN_YeCheng_V10_Idle",
            "gesture": "/Game/WindStation/Art/AnimeNPC/YeChengV10/Animations/AN_YeCheng_V10_Gesture.AN_YeCheng_V10_Gesture",
            "guarded": "/Game/WindStation/Art/AnimeNPC/YeChengV10/Animations/AN_YeCheng_V10_Guarded.AN_YeCheng_V10_Guarded",
            "work": "/Game/WindStation/Art/AnimeNPC/YeChengV10/Animations/AN_YeCheng_V10_Work.AN_YeCheng_V10_Work",
        },
        "injury_wrap": False,
    },
}


def load_required(path: str):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Required asset is missing: {path}")
    return asset


def create_or_load_character_asset(config: dict):
    asset_path = f"{ASSET_ROOT}/{config['asset_name']}"
    asset = unreal.load_asset(asset_path)
    if asset is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.WSCharacterAssetData)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            config["asset_name"],
            ASSET_ROOT,
            unreal.WSCharacterAssetData,
            factory,
        )
    if asset is None:
        raise RuntimeError(f"Unable to create {asset_path}")

    asset.modify(True)
    asset.set_editor_property("skeletal_mesh", load_required(config["skeletal_mesh"]))
    asset.set_editor_property("anim_blueprint", None)
    asset.set_editor_property("eye_material_slot_name", unreal.Name(""))
    asset.set_editor_property("eye_material", None)
    asset.set_editor_property("mesh_location", unreal.Vector(0.0, 0.0, 0.0))
    asset.set_editor_property("mesh_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    asset.set_editor_property("mesh_scale", unreal.Vector(1.0, 1.0, 1.0))
    asset.set_editor_property("actor_scale", unreal.Vector(1.0, 1.0, 1.0))

    animations = unreal.WSAnimSetConfig()
    for property_name, path in config["animations"].items():
        animations.set_editor_property(property_name, load_required(path))
    asset.set_editor_property("animations", animations)

    injury_wrap = unreal.WSInjuryWrapConfig()
    if config["injury_wrap"]:
        injury_wrap.set_editor_property(
            "mesh",
            load_required("/Engine/BasicShapes/Cylinder.Cylinder"),
        )
        injury_wrap.set_editor_property(
            "material",
            load_required("/Game/WindStation/Art/Materials/M_WS_Snow.M_WS_Snow"),
        )
        injury_wrap.set_editor_property("attach_socket", unreal.Name("J_Bip_R_Hand"))
        injury_wrap.set_editor_property("relative_scale", unreal.Vector(0.06, 0.06, 0.024))
        injury_wrap.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 90.0))
    else:
        injury_wrap.set_editor_property("mesh", None)
    asset.set_editor_property("injury_wrap", injury_wrap)

    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    unreal.log(f"WS_ANIME_NPC_DATA_ASSET path={asset_path}")
    return asset


unreal.EditorAssetLibrary.make_directory(ASSET_ROOT)
character_assets = {
    action_id: create_or_load_character_asset(config)
    for action_id, config in CHARACTERS.items()
}

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if world is None:
    raise RuntimeError(f"Unable to load map: {MAP_PATH}")

bound_counts = {action_id: 0 for action_id in CHARACTERS}
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if not isinstance(actor, unreal.WSInteractableActor):
        continue
    action_id = str(actor.get_editor_property("action_id"))
    if action_id not in character_assets:
        continue

    actor.modify(True)
    actor.set_editor_property("character_asset", character_assets[action_id])
    actor.configure(
        actor.get_editor_property("action_id"),
        actor.get_editor_property("display_name"),
        actor.get_editor_property("accent_color"),
    )
    bound_counts[action_id] += 1

if any(count != 1 for count in bound_counts.values()):
    raise RuntimeError(f"Expected exactly one actor per NPC, got {bound_counts}")

if not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True):
    raise RuntimeError("Failed to save character data assets or MVP map")

unreal.log(f"WS_ANIME_NPC_BIND_COMPLETE counts={bound_counts}")
