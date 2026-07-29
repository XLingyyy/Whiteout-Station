import unreal


MAP_PATH = "/Game/WindStation/World/MVP_StationMap"
DATA_ASSET_PATH = (
    "/Game/WindStation/Presentation/Characters/"
    "DA_WS_YeCheng_AnimeNPC.DA_WS_YeCheng_AnimeNPC"
)
TARGET_ROOT = "/Game/WindStation/Art/AnimeNPC/YeChengV10"
TARGET_MESH_PATH = (
    f"{TARGET_ROOT}/SK_YeCheng_NoanoaHair_RefinedFace_v10."
    "SK_YeCheng_NoanoaHair_RefinedFace_v10"
)
RETARGETER_PATH = (
    f"{TARGET_ROOT}/RTG_UE4_YeCheng_NoanoaHair_RefinedFace_v10."
    "RTG_UE4_YeCheng_NoanoaHair_RefinedFace_v10"
)
SOURCE_MESH_PATH = (
    "/Game/WindStation/Art/Characters/Doctor/"
    "SK_WS_Doctor.SK_WS_Doctor"
)
SOURCE_ANIMATIONS = (
    "/Game/WindStation/Art/Characters/Doctor/AN_WS_Doctor_Idle.AN_WS_Doctor_Idle",
    "/Game/WindStation/Art/Characters/Doctor/AN_WS_Doctor_Gesture.AN_WS_Doctor_Gesture",
    "/Game/WindStation/Art/Characters/Doctor/AN_WS_Doctor_Guarded.AN_WS_Doctor_Guarded",
    "/Game/WindStation/Art/Characters/Doctor/AN_WS_Doctor_Work.AN_WS_Doctor_Work",
)
TARGET_ANIMATION_ROOT = f"{TARGET_ROOT}/Animations"
TARGET_ANIMATIONS = {
    state: (
        f"{TARGET_ANIMATION_ROOT}/AN_YeCheng_V10_{state.capitalize()}."
        f"AN_YeCheng_V10_{state.capitalize()}"
    )
    for state in ("idle", "gesture", "guarded", "work")
}


def load_required(path: str):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Required asset is missing: {path}")
    return asset


unreal.EditorAssetLibrary.make_directory(TARGET_ANIMATION_ROOT)
source_asset_data = [
    unreal.EditorAssetLibrary.find_asset_data(path)
    for path in SOURCE_ANIMATIONS
]
if any(not data.is_valid() for data in source_asset_data):
    raise RuntimeError("One or more Ye Cheng source animations are missing")

retarget_inputs = unreal.IKRetargetBatchOperationInputs()
retarget_inputs.set_editor_property("assets_to_retarget", source_asset_data)
retarget_inputs.set_editor_property("source_mesh", load_required(SOURCE_MESH_PATH))
retarget_inputs.set_editor_property("target_mesh", load_required(TARGET_MESH_PATH))
retarget_inputs.set_editor_property(
    "ik_retarget_asset", load_required(RETARGETER_PATH)
)
retarget_inputs.set_editor_property("search", "AN_WS_Doctor")
retarget_inputs.set_editor_property("replace", "AN_YeCheng_V10")
retarget_inputs.set_editor_property("target_path", TARGET_ANIMATION_ROOT)
retarget_inputs.set_editor_property("include_referenced_assets", False)
retarget_inputs.set_editor_property("overwrite_existing_files", True)

retargeted = unreal.IKRetargetBatchOperation.run_batch_retarget(retarget_inputs)
if len(retargeted) != len(source_asset_data):
    raise RuntimeError(
        f"Expected {len(source_asset_data)} retargeted animations, "
        f"got {len(retargeted)}"
    )
unreal.EditorAssetLibrary.save_directory(
    TARGET_ANIMATION_ROOT, only_if_is_dirty=False, recursive=True
)

target_mesh = load_required(TARGET_MESH_PATH)
data_asset = load_required(DATA_ASSET_PATH)
data_asset.modify(True)
data_asset.set_editor_property("skeletal_mesh", target_mesh)
data_asset.set_editor_property("anim_blueprint", None)
data_asset.set_editor_property("eye_material_slot_name", unreal.Name(""))
data_asset.set_editor_property("eye_material", None)
data_asset.set_editor_property("mesh_location", unreal.Vector(0.0, 0.0, 0.0))
data_asset.set_editor_property("mesh_rotation", unreal.Rotator(0.0, 0.0, 0.0))
data_asset.set_editor_property("mesh_scale", unreal.Vector(1.0, 1.0, 1.0))
data_asset.set_editor_property("actor_scale", unreal.Vector(1.0, 1.0, 1.0))

animation_config = unreal.WSAnimSetConfig()
for state, path in TARGET_ANIMATIONS.items():
    animation_config.set_editor_property(state, load_required(path))
data_asset.set_editor_property("animations", animation_config)

injury_wrap = unreal.WSInjuryWrapConfig()
injury_wrap.set_editor_property("mesh", None)
data_asset.set_editor_property("injury_wrap", injury_wrap)
unreal.EditorAssetLibrary.save_loaded_asset(data_asset, False)

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if world is None:
    raise RuntimeError(f"Unable to load map: {MAP_PATH}")

matching_actors = []
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actor_subsystem.get_all_level_actors():
    if (
        isinstance(actor, unreal.WSInteractableActor)
        and str(actor.get_editor_property("action_id")) == "talk_ye_cheng"
    ):
        matching_actors.append(actor)

if len(matching_actors) != 1:
    raise RuntimeError(
        f"Expected exactly one Ye Cheng actor, got {len(matching_actors)}"
    )

actor = matching_actors[0]
actor.modify(True)
actor.set_editor_property("character_asset", data_asset)
actor.configure(
    actor.get_editor_property("action_id"),
    actor.get_editor_property("display_name"),
    actor.get_editor_property("accent_color"),
)
component = actor.get_editor_property("character_mesh")
if component.get_skeletal_mesh_asset() != target_mesh:
    raise RuntimeError("Ye Cheng actor did not adopt the V10 skeletal mesh")

if not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True):
    raise RuntimeError("Failed to save the Ye Cheng data asset or MVP map")

unreal.log(
    "WS_YECHENG_V10_INSTALL_COMPLETE "
    f"mesh={target_mesh.get_path_name()} "
    f"animations={[asset.package_name for asset in retargeted]}"
)
