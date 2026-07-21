"""Import v0.2 audio layers and assign them to a reproducible SoundClass tree."""

from __future__ import annotations

from pathlib import Path

import unreal


REPO_ROOT = Path(unreal.Paths.project_dir()).resolve().parent
SOURCE_ROOT = REPO_ROOT / "SourceAssets" / "GeneratedAudio"
GAME_ROOT = "/Game/WindStation/Audio"
CLASS_ROOT = f"{GAME_ROOT}/Mix"

ASSETS = {
    "Ambience/S_WindIndoor_CC0_Derivative.wav": ("Ambience", True),
    "Machinery/S_GeneratorLoop_Original.wav": ("Ambience", True),
    "Foley/S_FootstepSnow_Original.wav": ("Foley", False),
    "Foley/S_FootstepMetal_Original.wav": ("Foley", False),
    "Foley/S_FootstepConcrete_Original.wav": ("Foley", False),
    "UI/S_UIHover_Original.wav": ("UI", False),
    "UI/S_UIConfirm_Original.wav": ("UI", False),
    "UI/S_UIReject_Original.wav": ("UI", False),
    "UI/S_UIPromise_Original.wav": ("UI", False),
    "Events/S_CrisisStinger_Original.wav": ("Cinematic", False),
    "Events/S_RadioReply_Original.wav": ("Cinematic", False),
    "Music/S_EndingSuccess_Original.wav": ("Music", False),
    "Music/S_EndingSurvival_Original.wav": ("Music", False),
    "Music/S_EndingCost_Original.wav": ("Music", False),
    "Music/S_EndingCollapse_Original.wav": ("Music", False),
}


def ensure_sound_class(name: str, volume: float) -> unreal.SoundClass:
    path = f"{CLASS_ROOT}/SC_WS_{name}"
    asset = unreal.load_asset(path)
    if asset is None:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            f"SC_WS_{name}", CLASS_ROOT, unreal.SoundClass, unreal.SoundClassFactory()
        )
    if asset is None:
        raise RuntimeError(f"Unable to create {path}")
    properties = asset.get_editor_property("properties")
    properties.set_editor_property("volume", volume)
    asset.set_editor_property("properties", properties)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


def import_wave(relative_path: str, sound_class: unreal.SoundClass, looping: bool) -> unreal.SoundWave:
    source = SOURCE_ROOT / relative_path
    if not source.is_file():
        raise FileNotFoundError(source)
    relative = Path(relative_path)
    destination = f"{GAME_ROOT}/{relative.parent.as_posix()}"
    unreal.EditorAssetLibrary.make_directory(destination)
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = destination
    task.destination_name = relative.stem
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(f"{destination}/{relative.stem}")
    if asset is None or not isinstance(asset, unreal.SoundWave):
        raise RuntimeError(f"Unreal did not import {source}")
    asset.set_editor_property("looping", looping)
    asset.set_editor_property("sound_class_object", sound_class)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


def main() -> None:
    unreal.EditorAssetLibrary.make_directory(CLASS_ROOT)
    classes = {
        "Master": ensure_sound_class("Master", 1.0),
        "Ambience": ensure_sound_class("Ambience", 0.82),
        "Foley": ensure_sound_class("Foley", 0.92),
        "UI": ensure_sound_class("UI", 0.88),
        "Cinematic": ensure_sound_class("Cinematic", 0.94),
        "Music": ensure_sound_class("Music", 0.76),
    }
    classes["Master"].set_editor_property(
        "child_classes",
        [classes[name] for name in ("Ambience", "Foley", "UI", "Cinematic", "Music")],
    )
    unreal.EditorAssetLibrary.save_loaded_asset(classes["Master"], False)

    external_wind = unreal.load_asset(f"{GAME_ROOT}/Ambience/S_WindStrong_CC0")
    if external_wind is None:
        raise RuntimeError("Run Tools/Editor/import_audio_assets.py before importing v0.2 audio")
    external_wind.set_editor_property("looping", True)
    external_wind.set_editor_property("sound_class_object", classes["Ambience"])
    unreal.EditorAssetLibrary.save_loaded_asset(external_wind, False)

    imported = []
    for relative_path, (class_name, looping) in ASSETS.items():
        imported.append(import_wave(relative_path, classes[class_name], looping))
    unreal.log(
        f"WhiteoutStation v0.2: imported {len(imported)} generated audio layers and 6 SoundClasses"
    )


if __name__ == "__main__":
    main()
