import json
import shutil
from .validate_dialogue_v15 import ROOT, validate


def test_content_is_consistent():
    assert validate() == []


def test_unrendered_fact_and_missing_branch_are_rejected(tmp_path):
    for file in ROOT.glob('*.json'):
        shutil.copy2(file, tmp_path / file.name)
    path = tmp_path / 'AuthoredLines.json'
    lines = json.loads(path.read_text(encoding='utf-8'))
    factual = next(line for line in lines if line['claim_ids'])
    factual['npc_line'] = '我知道，但没有说出来。'
    group = lines[0]['line_group_id']
    lines = [line for line in lines if line['line_group_id'] != group or line['when'] or line['claim_ids']]
    path.write_text(json.dumps(lines, ensure_ascii=False), encoding='utf-8')
    errors = validate(tmp_path)
    assert any('claim tokens disagree' in error for error in errors)
    assert any('missing safe authored branch' in error for error in errors)
