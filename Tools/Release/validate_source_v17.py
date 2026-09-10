"""Validate v1.7 product/save/UI references while retaining v1.6 gameplay data."""
import argparse
from pathlib import Path
import sys
from validate_source_v16 import validate as validate_v16_content

ROOT = Path(__file__).resolve().parents[2]

def validate(root=ROOT):
    errors = validate_v16_content(root / 'WhiteoutStation/Content')
    source = root / 'WhiteoutStation/Source/WhiteoutStation'
    checks = {
        root / 'WhiteoutStation/Config/DefaultGame.ini': ['ProjectVersion=1.7.0', '/Game/WindStation/UI'],
        source / 'Public/Save/WindStationSaveGame.h': ['1.7.0'],
        source / 'Private/State/WindStationStateSubsystem.cpp': ['WhiteoutStation_Autosave_v1_7', 'LegacySaveSlotV16', '1.6.0'],
        source / 'Private/HUD/WhiteoutHUDV17.cpp': ['State.PhaseActionPoints', 'ActionPointsPerPhase', 'IsPresentationModalSafe'],
        source / 'Private/Settings/WhiteoutSettingsSubsystem.cpp': ['TutorialLastPageId', 'TutorialStatus', 'TutorialUpgradeHintShownVersion'],
    }
    for path, tokens in checks.items():
        text = path.read_text(encoding='utf-8-sig')
        errors += [f'{path.name}: missing {token}' for token in tokens if token not in text]
    for relative in ['Private/Flow/WSTutorialFlow.cpp', 'Private/HUD/WSTutorialWidget.cpp']:
        text = (source / relative).read_text(encoding='utf-8-sig')
        errors += [f'{relative}: world mutation {token}' for token in ['CommitAction(', 'NewGame(', 'BeginDayPhase(', 'SaveSnapshot(', 'SubmitDialogue'] if token in text]
    return errors

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package-content', type=Path)
    args = parser.parse_args()
    errors = validate()
    if args.package_content:
        errors += validate_v16_content(args.package_content)
    for error in errors: print(error)
    print(f'v1.7 source: {len(errors)} errors; tutorial assets and rendered acceptance checked separately')
    sys.exit(bool(errors))
