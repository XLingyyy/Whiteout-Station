"""Compare three authored routes with real DeepSeek intent/expression routes."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time


def comparable(data):
    # Expression, transaction IDs and network accounting are mode-specific.
    result = {k: v for k, v in data.items() if k not in ('model_calls', 'events')}
    event_fields = ('action_id', 'ap_before', 'ap_after', 'reason_code', 'dialogue_act',
                    'promise_condition', 'promise_recorded', 'crisis_triggered',
                    'speaker', 'final_disclosed_fact_ids')
    result['events'] = [{k: e[k] for k in event_fields} for e in data['events']]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--project', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--key-file', type=Path, required=True)
    parser.add_argument('--event-log', type=Path)
    args = parser.parse_args()
    exe, output = args.exe.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    key = re.search(r'sk-[A-Za-z0-9_-]+', args.key_file.read_text(encoding='utf-8-sig'))
    if not key:
        raise SystemExit('No test key found; no requests sent')
    event_log = args.event_log or ((args.project.resolve().parent if args.project else
        Path.home()/'AppData/Local/WhiteoutStation')/'Saved/Logs/WhiteoutStation_EventLog.json')
    results = []
    for route, ending in [('medical', 'TaskSuccess'), ('technical', 'TaskSuccess'), ('quick', 'CostUncontrolled')]:
        runs = {}
        for mode in ('authored', 'online'):
            env = os.environ.copy()
            env.pop('WHITEOUT_V15_TEST_KEY', None)
            if mode == 'online':
                env['WHITEOUT_V15_TEST_KEY'] = key.group()
            command = [str(exe)]
            if args.project:
                command += [str(args.project.resolve()), '-game']
            command += ['-NullRHI', '-unattended', '-nosplash', '-nop4', '-stdout', '-FullStdOutLogOutput',
                        '-WhiteoutV15AuthoredRoute' if mode == 'authored' else '-WhiteoutV15OnlineRoute',
                        f'-WhiteoutAutoRoute={route}', '-WhiteoutAutoRouteExit']
            started = time.time()
            with (output/f'{route}.{mode}.log').open('w', encoding='utf-8') as log:
                process = subprocess.run(command, env=env, cwd=exe.parent, stdout=log,
                                         stderr=subprocess.STDOUT, timeout=180)
            fresh = event_log.exists() and event_log.stat().st_mtime >= started
            if not fresh:
                raise RuntimeError(f'{route}/{mode}: missing fresh event export')
            data = json.loads(event_log.read_text(encoding='utf-8-sig'))
            shutil.copy2(event_log, output/f'{route}.{mode}.events.json')
            runs[mode] = data
            runs[f'{mode}_success'] = process.returncode == 0 and data['ending'] == ending
            print(f'{route}/{mode}: exit={process.returncode}, ending={data["ending"]}, calls={data["model_calls"]}', flush=True)
        a, b = comparable(runs['authored']), comparable(runs['online'])
        differences = {k: {'authored': a[k], 'online': b.get(k)} for k in a if a[k] != b.get(k)}
        (output/f'{route}.differences.json').write_text(json.dumps(differences, ensure_ascii=False, indent=2), encoding='utf-8')
        result = dict(route=route, equivalent=not differences,
                      success=runs['authored_success'] and runs['online_success'] and not differences
                              and runs['authored']['model_calls'] == 0 and runs['online']['model_calls'] > 0,
                      differing_fields=list(differences), authored_calls=runs['authored']['model_calls'],
                      online_calls=runs['online']['model_calls'], score=runs['online']['score'])
        results.append(result)
        print(json.dumps(result), flush=True)
        (output/'summary.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    return not all(r['success'] for r in results)


if __name__ == '__main__':
    raise SystemExit(main())
