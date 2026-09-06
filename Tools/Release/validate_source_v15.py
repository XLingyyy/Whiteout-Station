"""Check v1.5 runtime/content contracts, optionally in a staged Windows package."""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'Dialogue'))
from validate_dialogue_v15 import validate as validate_dialogue


def validate_content(content):
    errors = []
    runtime_path = content / 'Agents/AgentRuntime.v1.5.json'
    if not runtime_path.is_file():
        return [f'missing runtime: {runtime_path}']
    runtime = json.loads(runtime_path.read_text(encoding='utf-8-sig'))
    expected = {
        'schema_version': 8, 'runtime_version': '1.5.0',
        'protocol_version': 'bounded_roleplay_v5',
        'prompt_mode': 'canonical_intent_then_controlled_expression',
        'llm_enabled': False, 'max_session_turns': 3,
        'timeout_seconds': 10, 'intent_timeout_seconds': 3,
        'expression_timeout_seconds': 7, 'intent_max_output_tokens': 256,
        'max_output_tokens': 640, 'max_calls_per_turn': 2,
    }
    for key, value in expected.items():
        if runtime.get(key) != value:
            errors.append(f'{runtime_path.name}: {key} must be {value!r}')
    forbidden = {'api_key', 'authorization', 'bearer_token', 'secret'}
    if forbidden.intersection(k.lower() for k in runtime):
        errors.append('runtime contains a credential field')
    dialogue = content / 'Dialogue/v1.5'
    required = ('WorldKnowledge.json', 'NPC_GuHeng.json', 'NPC_YeCheng.json',
                'Relationship_GuHeng_YeCheng.json', 'AuthoredChoices.json',
                'AuthoredLines.json', 'DialogueClaims.json',
                'SafeFallbacks.json', 'DialoguePolicy.json')
    missing = [name for name in required if not (dialogue / name).is_file()]
    errors.extend(f'missing dialogue content: {name}' for name in missing)
    if not missing:
        errors.extend(validate_dialogue(dialogue))
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--contract-only', action='store_true', help='Validate source content without a package.')
    parser.add_argument('--package-content', type=Path, help='Staged Windows/WhiteoutStation/Content directory.')
    args = parser.parse_args()
    errors = validate_content(args.repo_root / 'WhiteoutStation/Content')
    if args.package_content:
        errors.extend(validate_content(args.package_content))
    for error in errors:
        print(error)
    print(f'v1.5 content/runtime contract: {len(errors)} errors; does not certify model, IME, or human QA')
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
