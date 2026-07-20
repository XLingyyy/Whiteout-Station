import unreal


def create_map(asset_path: str) -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    try:
        created = level_subsystem.new_level(asset_path, False)
    except TypeError:
        created = level_subsystem.new_level(asset_path)
    if not created:
        raise RuntimeError(f"Unable to create level {asset_path}")

    player_start = actor_subsystem.spawn_actor_from_class(
        unreal.PlayerStart,
        unreal.Vector(0.0, 200.0, 110.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    player_start.set_actor_label("MVP Player Start")

    game_mode_class = unreal.load_class(None, "/Script/WhiteoutStation.WhiteoutGameMode")
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_settings = world.get_world_settings()
    world_settings.set_editor_property("default_game_mode", game_mode_class)

    if not level_subsystem.save_current_level():
        raise RuntimeError(f"Unable to save level {asset_path}")
    unreal.log(f"Created and saved {asset_path}")


def main() -> None:
    unreal.EditorAssetLibrary.make_directory("/Game/WindStation")
    unreal.EditorAssetLibrary.make_directory("/Game/WindStation/Core")
    unreal.EditorAssetLibrary.make_directory("/Game/WindStation/Data")
    unreal.EditorAssetLibrary.make_directory("/Game/WindStation/World")
    unreal.EditorAssetLibrary.make_directory("/Game/WindStation/Characters")
    unreal.EditorAssetLibrary.make_directory("/Game/WindStation/UI")
    unreal.EditorAssetLibrary.make_directory("/Game/WindStation/AI")
    unreal.EditorAssetLibrary.make_directory("/Game/WindStation/Audio")
    unreal.EditorAssetLibrary.make_directory("/Game/WindStation/Tests")
    unreal.EditorAssetLibrary.make_directory("/Game/WindStation/Dev")

    create_map("/Game/WindStation/World/Dev_TestMap")
    create_map("/Game/WindStation/World/MVP_StationMap")


if __name__ == "__main__":
    main()
