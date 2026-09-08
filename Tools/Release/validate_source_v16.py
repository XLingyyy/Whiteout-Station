"""Validate v1.6 data references and packaging of its runtime/rules files."""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'Dialogue'))
from validate_dialogue_v15 import validate as validate_dialogue


def validate(content):
    errors = []
    try:
        runtime = json.loads((content / 'Agents/AgentRuntime.v1.6.json').read_text(encoding='utf-8-sig'))
        rules = json.loads((content / 'Rules/WhiteoutStationRules.v1.6.json').read_text(encoding='utf-8-sig'))
        expected = dict(schema_version=9, runtime_version='1.6.0', protocol_version='natural_roleplay_v6',
                        llm_enabled=False, max_session_turns=10, normal_deadline_seconds=10,
                        critical_deadline_seconds=15, normal_request_limit=2, critical_request_limit=3,
                        max_output_tokens=1000, verification_max_output_tokens=700)
        errors += [f'{key}: expected {value!r}' for key, value in expected.items() if runtime.get(key) != value]
        if any(key.lower() in {'api_key', 'authorization', 'bearer_token'} for key in runtime):
            errors.append('credential field in runtime')
        if rules.get('schema_version') != 7 or rules.get('rules_version') != '1.6.0':
            errors.append('wrong rules version')
        errors += validate_dialogue(content / 'Dialogue/v1.6')
    except (OSError, ValueError) as error:
        errors.append(str(error))
    return errors


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package-content', type=Path)
    args = parser.parse_args()
    errors = validate(Path(__file__).resolve().parents[2] / 'WhiteoutStation/Content')
    if args.package_content:
        errors += validate(args.package_content)
    for error in errors:
        print(error)
    print(f'v1.6 content: {len(errors)} errors; model and human review are separate')
    raise SystemExit(bool(errors))
