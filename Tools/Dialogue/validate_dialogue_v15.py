"""Validate the authored v1.5 content and its knowledge references."""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[2] / 'WhiteoutStation/Content/Dialogue/v1.5'
PREDICATES = {'always', 'actual_evidence', 'future_phase', 'generator_unstarted',
              'generator_partial', 'generator_complete', 'gu_diagnosed', 'gu_treated',
              'heating_locked', 'dialogue_turn_available'}


def validate(root=ROOT):
    load = lambda name: json.loads((root / name).read_text(encoding='utf-8-sig'))
    choices, lines, claims = (load(name) for name in
                             ('AuthoredChoices.json', 'AuthoredLines.json', 'DialogueClaims.json'))
    knowledge = {k['knowledge_id']: k for name in
                 ('WorldKnowledge.json', 'NPC_GuHeng.json', 'NPC_YeCheng.json',
                  'Relationship_GuHeng_YeCheng.json')
                 for k in load(name).get('knowledge', [])}
    errors = []
    def require(condition, message):
        if not condition:
            errors.append(message)
    for rows, field in ((choices, 'choice_id'), (lines, 'line_id'), (claims, 'claim_id')):
        ids = [row[field] for row in rows]
        require(len(ids) == len(set(ids)), f'duplicate {field}')
    claim_map = {c['claim_id']: c for c in claims}
    for claim in claims:
        source = knowledge.get(claim['knowledge_id'])
        require(source is not None, f"unknown knowledge: {claim['claim_id']}")
        if source:
            require(claim['text'] == source['content'], f"claim changed source meaning: {claim['claim_id']}")
            require(source['epistemic_status'] == 'known' and source['max_disclosure'] == 'explicit',
                    f"claim cannot be stated: {claim['claim_id']}")
    priorities = set()
    for line in lines:
        key = line['line_group_id'], line['priority']
        require(key not in priorities, f'priority conflict: {key}')
        priorities.add(key)
        require(set(line['when']) <= PREDICATES, f"unknown predicate: {line['line_id']}")
        tokens = re.findall(r'\{claim:([^}]+)\}', line['npc_line'])
        require(sorted(tokens) == sorted(line['claim_ids']) and len(tokens) == len(set(tokens)),
                f"claim tokens disagree: {line['line_id']}")
        require(set(tokens) <= claim_map.keys(), f"unknown claim: {line['line_id']}")
        text = line['npc_line']
        for cid in tokens:
            text = text.replace('{claim:'+cid+'}', claim_map.get(cid, {}).get('text', ''))
        require(len(text) <= 240, f"line too long: {line['line_id']}")
    for choice in choices:
        require(set(choice['visible_if'] + choice['enabled_if']) <= PREDICATES,
                f"unknown predicate: {choice['choice_id']}")
        require(choice['speaker_id'] == choice['intent']['speaker_id'], 'speaker mismatch')
        require(choice['topic_id'] == choice['intent']['topic_id'], 'topic mismatch')
        require(choice['repeat_policy'] == 'recap_no_bonus', 'unsupported repeat policy')
        variants = [x for x in lines if x['line_group_id'] == choice['line_group_id']]
        require(any(not x['when'] and not x['claim_ids'] for x in variants),
                f"missing safe authored branch: {choice['choice_id']}")
    return errors


if __name__ == '__main__':
    failures = validate()
    for failure in failures:
        print(failure)
    print(f'v1.5 content: {len(failures)} errors')
    raise SystemExit(bool(failures))
