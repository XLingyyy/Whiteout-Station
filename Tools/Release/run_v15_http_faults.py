"""Exercise the real UE two-stage HTTP path against a loopback fault server."""
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
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    requests = defaultdict(list)

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            payload = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            scenario = payload['model']
            stage = 'a' if 'player_text' in json.loads(payload['messages'][-1]['content']) else 'b'
            context = json.loads(payload['messages'][-1]['content'])
            requests[scenario].append(dict(stage=stage, max_tokens=payload['max_tokens'],
                temperature=payload['temperature'], authorization_present='Authorization' in self.headers))
            if scenario == f'{stage}_timeout':
                time.sleep(4 if stage == 'a' else 8)
            if scenario in (f'cancel_{stage}', f'new_game_{stage}'):
                time.sleep(1.5)
            status = 500 if scenario == f'{stage}_http500' else 200
            if stage == 'a':
                content = dict(speaker_id='ye_cheng', topic_id='medical', speech_act='ask', query_type='status',
                    target_action_id='repair_generator', target_character='gu_heng', polarity='affirmative',
                    commitment='none', promise_condition='', confidence=0.99, needs_clarification=False,
                    evidence_spans=[context['player_text']], resolved_from_turn=0)
                if scenario == 'b_no_fallback':
                    content.update(topic_id='rescue', query_type='requirements', target_action_id='send_signal', target_character='player')
            else:
                content = dict(segments=[{'kind': 'text', 'text': '我会认真回答。'}] +
                    [{'kind': 'claim', 'claim_id': k} for k in context['required_claims']],
                    referenced_knowledge_ids=[], proposal_id='', memory_summary='', emotion='neutral', reaction_action='consider')
                if scenario == 'b_missing_claim':
                    content['segments'] = [{'kind': 'text', 'text': '我听到了。'}]
                if scenario == 'b_unknown_claim':
                    content['segments'].append({'kind': 'claim', 'claim_id': 'unlisted_secret'})
                if scenario == 'b_fact_text':
                    content['segments'].append({'kind': 'text', 'text': '右手撕裂加失温。'})
            encoded = '{broken' if scenario in (f'{stage}_bad_json', 'b_no_fallback') and (stage == 'b' or scenario.startswith('a_')) else json.dumps(content, ensure_ascii=False)
            response = json.dumps({'choices': [{'message': {'content': encoded}, 'finish_reason': 'stop'}]}, ensure_ascii=False).encode('utf-8')
            try:
                self.send_response(status)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(response)))
                self.end_headers()
                self.wfile.write(response)
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    names = ['success', 'a_bad_json', 'a_http500', 'a_timeout', 'b_bad_json', 'b_http500', 'b_timeout',
             'b_missing_claim', 'b_unknown_claim', 'b_fact_text', 'b_no_fallback',
             'cancel_a', 'cancel_b', 'new_game_a', 'new_game_b']
    cases = []
    for name in names:
        case = dict(text='顾衡现在还能做精细维修吗？', action='talk_ye_cheng', provider='loopback',
                    base_url=f'http://127.0.0.1:{server.server_port}', model=name)
        if name.startswith('cancel_'):
            case['lifecycle'] = 'cancel'
        if name.startswith('new_game_'):
            case['lifecycle'] = 'new_game'
        cases.append(case)
    corpus = output/'cases.json'
    corpus.write_text(json.dumps({'cases': cases}, ensure_ascii=False, indent=2), encoding='utf-8')
    env = os.environ.copy()
    env.pop('WHITEOUT_V15_TEST_KEY', None)
    with (output/'engine.log').open('w', encoding='utf-8') as log:
        process = subprocess.run([str(args.exe.resolve()), str(args.project.resolve()), '-game', '-NullRHI',
            '-unattended', '-nosplash', '-nop4', '-stdout', '-FullStdOutLogOutput', f'-WhiteoutV15Probe={corpus}'],
            env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    server.shutdown()
    results = []
    for index, name in enumerate(names):
        path = Path(f'{corpus}.{index:03d}.result.json')
        data = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}
        lifecycle = name.startswith(('cancel_', 'new_game_'))
        expected_commit = name == 'success' or (name.startswith('b_') and name != 'b_no_fallback')
        expected_stages = ['a'] if name.startswith('a_') or name.endswith('_a') else ['a', 'b']
        observed = requests[name]
        passed = (process.returncode == 0 and bool(data) and data['committed'] == expected_commit
                  and data['ap_after'] == (3 if expected_commit else 4)
                  and data['promises'] == 0 and data['diagnosed'] == expected_commit and not data['pending']
                  and [r['stage'] for r in observed] == expected_stages
                  and all(r['max_tokens'] == (1600 if r['stage'] == 'a' else 640)
                          and r['temperature'] == (0 if r['stage'] == 'a' else 0.45) for r in observed)
                  and not any(r['authorization_present'] for r in observed)
                  and data['elapsed_seconds'] < 10.5)
        if expected_commit:
            passed &= data['source'] == ('controlled_roleplay_v15' if name == 'success' else 'authored_recovery_v15')
        if lifecycle:
            passed &= data['status'] == 'lifecycle_settled'
        result = dict(scenario=name, success=passed, expected_commit=expected_commit, result=data, requests=observed)
        results.append(result)
        print(f'{name}: {"PASS" if passed else "FAIL"}', flush=True)
    (output/'summary.json').write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
    return not all(r['success'] for r in results)


if __name__ == '__main__':
    raise SystemExit(main())
