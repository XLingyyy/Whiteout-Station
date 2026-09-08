"""Check actual v1.6 HTTP failure/cancellation transactions against localhost."""
import argparse
from collections import defaultdict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import threading
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--project', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    requests = defaultdict(list)

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            payload = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            scenario = payload['model']
            context = json.loads(payload['messages'][-1]['content'])
            stage = 'a' if 'player_text' in context else 'c' if 'candidate_json' in context else 'b'
            requests[scenario].append(dict(stage=stage, max_tokens=payload['max_tokens'],
                                          authorization_present='Authorization' in self.headers))
            if scenario == f'{stage}_timeout':
                time.sleep(dict(a=4, b=8, c=13)[stage])
            if scenario in (f'cancel_{stage}', f'new_game_{stage}'):
                time.sleep(1.5)
            status = 500 if scenario == f'{stage}_http500' else 200
            if stage == 'a':
                content = dict(speaker_id='ye_cheng', topic_id='medical', speech_act='ask', query_type='status',
                               target_action_id='repair_generator', target_character='gu_heng', polarity='affirmative',
                               commitment='none', promise_condition='', confidence=0.99, needs_clarification=False,
                               evidence_spans=[context['player_text']], resolved_from_turn=0,
                               question_purpose='current_condition', requested_actor_id='ye_cheng')
            elif stage == 'b':
                content = dict(npc_line='顾衡的右手受伤，精细操作受限，还没有治疗。',
                               addressed_goal_ids=[g['goal_id'] for g in context['answer_goals']],
                               referenced_fact_ids=[], action_proposal_ids=[], emotion='clinical', reaction_action='consider')
                if scenario == 'c_false_safe_initial':
                    content['npc_line'] = '只做了初步处理，完整治疗还没开始。'
                if scenario == 'b_unknown_fact':
                    content['referenced_fact_ids'] = ['unlisted_secret']
            else:
                authority = json.loads(context['frozen_authority'])
                content = dict(safe=scenario != 'c_rejected', issues=['wrong_patient'] if scenario == 'c_rejected' else [],
                               expressed_fact_ids=['YE_GU_HAND_DIAGNOSIS'],
                               addressed_goal_ids=[g['goal_id'] for g in authority['answer_goals']], corrects_entry_id='', event_claims=[dict(action='treatment', target='gu_heng', method='initial' if scenario == 'c_false_safe_initial' else 'full', status='completed' if scenario == 'c_false_safe_initial' else 'not_completed')])
            encoded = '{broken' if scenario == f'{stage}_bad_json' else json.dumps(content, ensure_ascii=False)
            response = json.dumps({'choices': [{'message': {'content': encoded}, 'finish_reason': 'stop'}]}, ensure_ascii=False).encode('utf-8')
            try:
                self.send_response(status); self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(response))); self.end_headers(); self.wfile.write(response)
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    names = ['success'] + [f'{stage}_{fault}' for stage in 'abc' for fault in ['bad_json', 'http500', 'timeout']]
    names += ['b_unknown_fact', 'c_rejected', 'c_false_safe_initial', 'cancel_a', 'cancel_b', 'cancel_c', 'new_game_c']
    cases = []
    for name in names:
        case = dict(text='顾衡现在还能做精细维修吗？', action='talk_ye_cheng', provider='loopback',
                    base_url=f'http://127.0.0.1:{server.server_port}', model=name)
        if name.startswith('cancel_'): case['lifecycle'] = 'cancel'
        if name.startswith('new_game_'): case['lifecycle'] = 'new_game'
        cases.append(case)
    corpus = output / 'cases.json'
    corpus.write_text(json.dumps(dict(cases=cases), ensure_ascii=False, indent=2), encoding='utf-8')
    env = os.environ.copy(); env.pop('WHITEOUT_V15_TEST_KEY', None)
    command = [str(args.exe.resolve())]
    if args.project: command += [str(args.project.resolve()), '-game']
    command += ['-NullRHI', '-unattended', '-nosplash', '-nop4', '-stdout', '-FullStdOutLogOutput',
                f'-UserDir={output / "runtime"}', f'-WhiteoutV15Probe={corpus}']
    with (output / 'engine.log').open('w', encoding='utf-8') as log:
        process = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240)
    server.shutdown()
    results = []
    for index, name in enumerate(names):
        path = Path(f'{corpus}.{index:03d}.result.json')
        data = json.loads(path.read_text(encoding='utf-8-sig')) if path.exists() else {}
        expected_commit = name == 'success'
        last_stage = name.split('_')[0] if name[0] in 'abc' and not name.startswith('cancel_') else name[-1]
        expected_stages = ['a'] if last_stage == 'a' else ['a', 'b'] if last_stage == 'b' else ['a', 'b', 'c']
        observed = requests[name]
        passed = (process.returncode == 0 and bool(data) and data['committed'] == expected_commit
                  and data['ap_after'] == 4 and data['promises'] == 0
                  and data['diagnosed'] == expected_commit and not data['pending']
                  and [r['stage'] for r in observed] == expected_stages
                  and all(r['max_tokens'] == dict(a=1600, b=1000, c=700)[r['stage']] for r in observed)
                  and not any(r['authorization_present'] for r in observed) and data['elapsed_seconds'] < 15.5)
        results.append(dict(scenario=name, success=passed, result=data, requests=observed))
        print(f'{name}: {"PASS" if passed else "FAIL"}', flush=True)
    (output / 'summary.json').write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
    return not all(r['success'] for r in results)


if __name__ == '__main__':
    raise SystemExit(main())
