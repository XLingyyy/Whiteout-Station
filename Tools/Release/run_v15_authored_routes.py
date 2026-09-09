"""Run authored routes in an Editor or game executable and retain event exports."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--project', type=Path)
    parser.add_argument('--event-log', type=Path)
    parser.add_argument('--rebalance', action='store_true', help='Also require four inefficient routes and a 90-point care route')
    args = parser.parse_args()
    exe, output = args.exe.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    runtime = output / 'runtime'
    event_log = args.event_log or (runtime / 'Saved/Logs/WhiteoutStation_EventLog.json')
    expected = {'medical': 'TaskSuccess', 'technical': 'TaskSuccess',
                'quick': 'CostUncontrolled', 'wait': 'SurvivalWait', 'collapse': 'TotalCollapse'}
    if args.rebalance:
        expected.update({name: 'TaskSuccess' for name in ('medical_slow1', 'medical_slow2', 'technical_slow1', 'technical_slow2', 'care90')})
    results = []
    for route, ending in expected.items():
        started = time.time()
        with (output / f'{route}.log').open('w', encoding='utf-8') as log:
            command = [str(exe)] + ([str(args.project.resolve()), '-game'] if args.project else [])
            process = subprocess.run(command + ['-NullRHI', '-unattended', '-nosplash', f'-UserDir={runtime}',
                                      '-stdout', '-FullStdOutLogOutput', '-WhiteoutV15AuthoredRoute',
                                      f'-WhiteoutAutoRoute={route}', '-WhiteoutAutoRouteExit'],
                                     cwd=exe.parent, stdout=log, stderr=subprocess.STDOUT, timeout=120)
        fresh = event_log.exists() and event_log.stat().st_mtime >= started
        data = json.loads(event_log.read_text(encoding='utf-8-sig')) if fresh else {}
        if fresh:
            shutil.copy2(event_log, output / f'{route}.events.json')
        dialogue = [e for e in data.get('events', []) if e.get('action_id') in ('talk_gu_heng', 'talk_ye_cheng')]
        authored = all(e.get('answer_source') == 'authored_v15' for e in dialogue)
        passed = process.returncode == 0 and fresh and data.get('ending') == ending and data.get('model_calls') == 0 and authored
        paid_ap = sum(e.get('actual_ap', 0) for e in data.get('events', []))
        if route == 'care90':
            passed = passed and data.get('score', 0) >= 90 and paid_ap <= 12
        result = {'route': route, 'success': passed, 'exit_code': process.returncode,
                  'fresh_export': fresh, 'ending': data.get('ending'), 'score': data.get('score'),
                  'model_calls': data.get('model_calls'), 'authored_dialogue_count': len(dialogue),
                  'all_dialogue_authored': authored, 'paid_ap': paid_ap}
        results.append(result)
        print(json.dumps(result), flush=True)
    (output / 'summary.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    return not all(r['success'] for r in results)


if __name__ == '__main__':
    raise SystemExit(main())
