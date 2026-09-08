"""Exercise the real UE dialogue pipeline; keys stay in child process memory."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--key-file', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cases', type=Path)
    parser.add_argument('--exe', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    cases = json.loads(args.cases.read_text(encoding='utf-8-sig'))['cases'] if args.cases else [
        dict(id='medical_continuity', action='talk_ye_cheng', steps=[
            '你和顾衡两个人有人受伤吗？', '你已经处理过了？', '怎么处理？我怎么做？',
            '所以只是说了怎么做，还没有真的治疗，对吗？', '顾衡的手现在还是受伤状态吗？']),
        dict(id='gu_multi_and_history', action='talk_gu_heng', steps=[
            '你为什么留在这里？另外发电机修到哪了？', '你的手怎么了？',
            '你愿意先接受治疗，再修发电机吗？', '叶澄说你已经接受治疗了，是真的吗？', '我刚才问的留在这里的原因，你还记得吗？']),
    ]
    for case in cases:
        case.setdefault('text', case['steps'][0])
    corpus = output / 'cases.json'
    corpus.write_text(json.dumps({'cases': cases}, ensure_ascii=False, indent=2), encoding='utf-8')
    data = args.key_file.read_bytes()
    credential = None
    for encoding in ('utf-8-sig', 'utf-16', 'gb18030'):
        try:
            match = re.search(r'sk-[A-Za-z0-9_-]+', data.decode(encoding))
        except UnicodeError:
            continue
        if match:
            credential = match.group()
            break
    if not credential:
        raise SystemExit('No test credential found')
    env = os.environ.copy()
    env['WHITEOUT_V15_TEST_KEY'] = credential
    exe = args.exe.resolve() if args.exe else Path('G:/UnrealEngine/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe')
    command = [str(exe)]
    if not args.exe:
        command += [str(root / 'WhiteoutStation/WhiteoutStation.uproject'), '-game']
    command += ['-NullRHI', f'-UserDir={output / "runtime"}', '-unattended', '-nop4', '-nosplash',
                '-stdout', '-FullStdOutLogOutput', '-WhiteoutDialogueDebug', f'-WhiteoutV15Probe={corpus}']
    with (output / 'engine.log').open('w', encoding='utf-8') as log:
        process = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=max(300, len(cases) * 180))
    reports = []
    for index, case in enumerate(cases):
        path = Path(f'{corpus}.{index:03d}.result.json')
        result = json.loads(path.read_text(encoding='utf-8-sig')) if path.exists() else {'missing': True}
        reports.append(dict(id=case['id'], result=result))
        rows = result.get('steps', [])
        print(f"{case['id']}: {len(rows)} messages, {sum(bool(s['parsed']) for s in rows)} parsed, {sum(s['committed'] for s in rows)} committed", flush=True)
    summary = dict(engine_exit=process.returncode, provider='deepseek', model='deepseek-v4-flash',
                   protocol='natural_roleplay_v6', cases=reports,
                   evidence_kind='Real provider scripted conversations. Human naturalness review recorded separately.')
    (output / 'results.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    return process.returncode


if __name__ == '__main__':
    raise SystemExit(main())
