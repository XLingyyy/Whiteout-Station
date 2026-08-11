"""Editor smoke test for non-destructive v1.1 hotspot synchronization."""

import unreal


SOURCE_MAP_PATH = "/Game/WindStation/World/MVP_StationMap"
TEST_MAP_PATH = "/Temp/WS_HotspotSyncTest"
EXPECTED_ACTION_IDS = {
    "investigate_generator_log",
    "send_signal",
    "heat_control_room",
    "inspect_control_cabinet",
    "heat_repair_room",
    "repair_generator",
    "forced_self_repair",
    "talk_gu_heng",
    "heat_medical_room",
    "treat_character",
    "talk_ye_cheng",
    "distribute_food",
    "heat_kitchen",
    "rest",
    "dismantle_kitchen_heater",
    "calibrate_antenna",
}


def action_id(actor: unreal.Actor) -> str:
    return str(actor.get_editor_property("action_id"))


def has_tag(actor: unreal.Actor, tag: str) -> bool:
    return any(str(actor_tag) == tag for actor_tag in actor.tags)


def actor_location(actor: unreal.Actor) -> tuple[float, float, float]:
    location = actor.get_actor_location()
    return location.x, location.y, location.z


def find_hotspots(actor_subsystem: unreal.EditorActorSubsystem) -> dict[str, unreal.Actor]:
    hotspots: dict[str, unreal.Actor] = {}
    for actor in actor_subsystem.get_all_level_actors():
        if not has_tag(actor, "WSRuntimeHotspot"):
            continue
        try:
            candidate_action_id = action_id(actor)
        except Exception:
            continue
        if candidate_action_id in hotspots:
            raise RuntimeError(f"Duplicate hotspot action id: {candidate_action_id}")
        hotspots[candidate_action_id] = actor
    return hotspots


def main() -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    builder_class = unreal.load_class(
        None,
        "/Script/WhiteoutStation.WhiteoutStationBuilder",
    )
    if not builder_class:
        raise RuntimeError("WhiteoutStationBuilder class is unavailable")

    if unreal.EditorAssetLibrary.does_asset_exist(TEST_MAP_PATH):
        if not unreal.EditorAssetLibrary.delete_asset(TEST_MAP_PATH):
            raise RuntimeError(f"Unable to remove stale test map {TEST_MAP_PATH}")

    try:
        if not level_subsystem.new_level_from_template(TEST_MAP_PATH, SOURCE_MAP_PATH):
            raise RuntimeError("Unable to create hotspot synchronization test map")

        builders = [
            actor
            for actor in actor_subsystem.get_all_level_actors()
            if actor.get_class() == builder_class
        ]
        if len(builders) != 1:
            raise RuntimeError(f"Expected one station builder, found {len(builders)}")
        builder = builders[0]

        before = find_hotspots(actor_subsystem)
        antenna = before.get("calibrate_antenna")
        if not antenna:
            raise RuntimeError("Source map is missing the saved antenna hotspot")
        original_location = actor_location(antenna)

        builder.sync_missing_editable_hotspots()
        after_first_sync = find_hotspots(actor_subsystem)
        if set(after_first_sync) != EXPECTED_ACTION_IDS:
            missing = sorted(EXPECTED_ACTION_IDS - set(after_first_sync))
            raise RuntimeError(f"Hotspot synchronization is incomplete: {missing}")
        if actor_location(after_first_sync["calibrate_antenna"]) != original_location:
            raise RuntimeError("First synchronization moved the saved antenna hotspot")

        custom_location = unreal.Vector(3333.0, -777.0, 222.0)
        antenna.set_actor_location(custom_location, False, False)
        builder.sync_missing_editable_hotspots()
        after_second_sync = find_hotspots(actor_subsystem)
        if len(after_second_sync) != len(EXPECTED_ACTION_IDS):
            raise RuntimeError("Second synchronization created duplicate hotspots")
        if actor_location(after_second_sync["calibrate_antenna"]) != (
            custom_location.x,
            custom_location.y,
            custom_location.z,
        ):
            raise RuntimeError("Second synchronization replaced a manual antenna transform")

        unreal.log(
            "WhiteoutStation HotspotSyncTest: PASS "
            f"before={len(before)} after={len(after_second_sync)}"
        )
    finally:
        level_subsystem.load_level(SOURCE_MAP_PATH)
        if unreal.EditorAssetLibrary.does_asset_exist(TEST_MAP_PATH):
            if not unreal.EditorAssetLibrary.delete_asset(TEST_MAP_PATH):
                unreal.log_error(f"Unable to delete test map {TEST_MAP_PATH}")


if __name__ == "__main__":
    main()
