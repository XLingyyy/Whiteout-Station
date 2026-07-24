"""Materialize the procedural station as independently editable level actors."""

import unreal


MAP_PATH = "/Game/WindStation/World/MVP_StationMap"
MIN_EXPECTED_ACTORS = 40
EXPECTED_HOTSPOTS = 13
EDITABLE_TAG = "WSEditableStation"


def has_tag(actor: unreal.Actor, tag: str) -> bool:
    return any(str(actor_tag) == tag for actor_tag in actor.tags)


def main() -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not level_subsystem.load_level(MAP_PATH):
        raise RuntimeError(f"Unable to load {MAP_PATH}")

    builder_class = unreal.load_class(None, "/Script/WhiteoutStation.WhiteoutStationBuilder")
    if not builder_class:
        raise RuntimeError("WhiteoutStationBuilder class is unavailable; build the Editor target first")

    actors = list(actor_subsystem.get_all_level_actors())
    builders = [actor for actor in actors if actor.get_class() == builder_class]
    if builders:
        builder = builders[0]
        for duplicate in builders[1:]:
            actor_subsystem.destroy_actor(duplicate)
    else:
        builder = actor_subsystem.spawn_actor_from_class(
            builder_class,
            unreal.Vector(0.0, 0.0, 0.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
    if not builder:
        raise RuntimeError("Unable to create the station layout builder")

    builder.set_actor_label("WS Station Layout Builder")
    builder.generate_editable_station_layout()

    actors = list(actor_subsystem.get_all_level_actors())
    editable = [actor for actor in actors if has_tag(actor, EDITABLE_TAG)]
    hotspots = [actor for actor in editable if has_tag(actor, "WSRuntimeHotspot")]
    if len(editable) < MIN_EXPECTED_ACTORS:
        raise RuntimeError(
            f"Editable layout is incomplete: found {len(editable)}, expected at least {MIN_EXPECTED_ACTORS}"
        )
    if len(hotspots) != EXPECTED_HOTSPOTS:
        raise RuntimeError(
            f"Editable hotspot count is {len(hotspots)}, expected {EXPECTED_HOTSPOTS}"
        )
    if not level_subsystem.save_current_level():
        raise RuntimeError(f"Unable to save {MAP_PATH}")

    actor_subsystem.set_selected_level_actors([builder])
    unreal.log(
        f"WhiteoutStation: saved {len(editable)} editable actors "
        f"({len(hotspots)} hotspots) to {MAP_PATH}"
    )


if __name__ == "__main__":
    main()
