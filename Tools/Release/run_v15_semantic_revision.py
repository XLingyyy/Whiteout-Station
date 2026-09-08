"""Run real-provider semantic revision sequences through the UE game pipeline."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--key-file', type=Path, required=True)
    parser.add_argument('--only', nargs='+', help='Run selected case IDs')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    rows = json.loads((root / 'docs/QA/v1.5_review120_confirmed.json').read_text(encoding='utf-8'))['cases']
    byid = {row['id']: row for row in rows}
    cases = []
    for number in [33, 34, 35, 36, 88, 90, 91, 92, 95, 96]:
        row = byid[f'V15-{number:03d}']
        cases.append(dict(id=row['id'], text=row['text'], action='talk_ye_cheng' if '交谈对象：叶澄' in row['context'] else 'talk_gu_heng',
                          steps=[row['text']], fixture='morning_medical_heating_no_player_diagnosis_or_evidence'))
    for name, steps, setup in [
        ('amend_zone_phase', [byid['V15-081']['text'], byid['V15-094']['text'], byid['V15-093']['text'], '确认，我同意当前这一版安排。'], 0),
        ('third_slot_proposal', [byid['V15-098']['text'], byid['V15-099']['text'], '确认。'], 2),
        ('third_answer_and_proposal', [byid['V15-088']['text'], '你为什么留在站里？', '确认当前待确认的安排。', '确认。'], 2),
        ('cancel_proposal', [byid['V15-081']['text'], byid['V15-086']['text'], '确认。'], 0),
        ('missing_deadline', ['我保证给厨房供暖。'], 0),
        ('pressure_reference_048', [byid['V15-033']['text'], byid['V15-048']['text']], 0),
        ('false_evidence', [byid['V15-101']['text']], 0),
    ]:
        cases.append(dict(id=name, action='talk_gu_heng', text=steps[0], steps=steps, setup_turns=setup,
                          fixture='morning_medical_heating; setup_turns are actual committed authored person/generator responses'))
    cases.append(dict(id='V15-090-diagnosed', action='talk_ye_cheng', text=byid['V15-090']['text'],
                      steps=[byid['V15-090']['text']], setup_diagnosis=True,
                      fixture='morning_medical_heating; player diagnosis acquired through actual authored transaction before this message'))
    cases.append(dict(id='V15-090-disclosable', action='talk_ye_cheng', text=byid['V15-090']['text'],
                      steps=[byid['V15-090']['text']], setup_diagnosis=True, setup_cooperation=True,
                      fixture='morning_medical_heating; actual diagnosis and separate reassurance transactions satisfy player knowledge, trust and pressure gates'))
    if args.only:
        cases = [case for case in cases if case['id'] in args.only]
        if not cases:
            parser.error('No matching case IDs')
    corpus = output / 'cases.json'
    corpus.write_text(json.dumps({'cases': cases}, ensure_ascii=False, indent=2), encoding='utf-8')
    match = re.search(r'sk-[A-Za-z0-9_-]+', args.key_file.read_text(encoding='utf-8-sig'))
    if not match:
        raise SystemExit('No test key found')
    env = os.environ.copy()
    env['WHITEOUT_V15_TEST_KEY'] = match.group()
    exe = Path('G:/UnrealEngine/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe')
    with (output / 'engine.log').open('w', encoding='utf-8') as log:
        result = subprocess.run([str(exe), str(root/'WhiteoutStation/WhiteoutStation.uproject'), '-game', '-NullRHI',
                                 '-unattended', '-nop4', '-nosplash', '-stdout', '-FullStdOutLogOutput',
                                 f'-WhiteoutV15Probe={corpus}'], env=env, stdout=log, stderr=subprocess.STDOUT, timeout=600)
    reports = []
    for index, case in enumerate(cases):
        path = Path(f'{corpus}.{index:03d}.result.json')
        data = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'missing': True}
        reports.append(dict(id=case['id'], fixture=case['fixture'], result=data))
        steps = data.get('steps', [])
        print(f"{case['id']}: {len(steps)} messages, {sum(bool(s['parsed']) for s in steps)} parsed, {sum(s['committed'] for s in steps)} committed", flush=True)
    (output/'results.json').write_text(json.dumps({'engine_exit': result.returncode, 'provider':'deepseek', 'model':'deepseek-v4-flash',
        'kind':'real_model_requests; no runtime verdict inferred from safe rejection', 'cases':reports}, ensure_ascii=False, indent=2), encoding='utf-8')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
