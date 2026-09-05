"""Exercise packaged v1.4 routes and single-call roleplay over real HTTP.

Reuse the established route runner and state comparison. The loopback server
keeps request contents in memory and stores only validation outcomes.
"""

from __future__ import annotations

import argparse
import json
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

try:
    from . import run_v11_shipping_smoke as base
    from . import run_v13_shipping_smoke as previous
except ImportError:
    import run_v11_shipping_smoke as base
    import run_v13_shipping_smoke as previous


class RoleplayEndpoint:
    def __init__(self, mode: str):
        self.mode = mode
        self.attempts = 0
        self.errors: list[str] = []
        owner = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_args):
                pass

            def do_POST(self):
                owner.attempts += 1
                try:
                    body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
                    context = json.loads(body['messages'][-1]['content'])
                    assert context['protocol_version'] == 'bounded_roleplay_v4'
                    assert context['prompt_mode'] == 'subjective_context_single_call'
                    assert 8 <= len(context['available_knowledge']) <= 12
                    assert 'must_realize' not in context
                    assert 'forbidden_fact_ids' not in context
                    assert all(k['max_disclosure'] != 'hidden'
                               for k in context['available_knowledge'])
                    assert body['max_tokens'] <= 320
                except (AssertionError, KeyError, ValueError, TypeError) as exc:
                    owner.errors.append(type(exc).__name__)
                    self.send_error(400)
                    return
                if owner.mode == 'timeout':
                    time.sleep(3)
                reply = {
                    'npc_line': '你具体想问谁，还是哪台设备？说清一点。',
                    'speech_function': 'clarify',
                    'referenced_knowledge_ids': [],
                    'assertions': [],
                    'proposed_action': {'type': 'none', 'action_id': 'none',
                                        'requested_condition_ids': [], 'expires_at_phase': 'none'},
                    'memory_summary': '请玩家明确当前问题。',
                    'emotion': 'guarded',
                    'movement_intent': 'stay',
                    'reaction_action': 'consider',
                }
                if owner.mode == 'valid' and '顾衡的手怎么样' in context['player_line']:
                    knowledge = next(k for k in context['available_knowledge']
                                     if k['knowledge_id'] == 'YE_GU_HAND_DIAGNOSIS')
                    reply['npc_line'] = knowledge['content'] + '。'
                    reply['speech_function'] = 'answer'
                    reply['referenced_knowledge_ids'] = [knowledge['knowledge_id']]
                    reply['assertions'] = [{'knowledge_id': knowledge['knowledge_id'], 'claim_mode': 'stated'}]
                    reply['memory_summary'] = '已说明顾衡的检查结论。'
                elif owner.mode == 'valid' and '继电器还有替代方案' in context['player_line']:
                    knowledge = next(k for k in context['available_knowledge']
                                     if k['knowledge_id'] == 'GU_RELAY_COMPATIBILITY_KNOWLEDGE')
                    reply['npc_line'] = knowledge['content'] + '。'
                    reply['speech_function'] = 'answer'
                    reply['referenced_knowledge_ids'] = [knowledge['knowledge_id']]
                    reply['assertions'] = [{'knowledge_id': knowledge['knowledge_id'], 'claim_mode': 'stated'}]
                    reply['memory_summary'] = '已说明替代继电器的来源和拆取代价。'
                elif owner.mode == 'valid' and context['target_subject_id'] == 'gu_heng':
                    reply['npc_line'] = '你想问顾衡的哪方面？说清一点。'
                if owner.mode == 'forbidden_fact':
                    reply['npc_line'] = '顾衡执行过强制重启。'
                elif owner.mode == 'unknown_knowledge':
                    reply['referenced_knowledge_ids'] = ['UNAVAILABLE_KNOWLEDGE']
                elif owner.mode == 'invalid_proposal':
                    reply['proposed_action'] = {
                        'type': 'suggest_action', 'action_id': 'grant_resources',
                        'requested_condition_ids': [], 'expires_at_phase': 'dusk'}
                content = '{invalid' if owner.mode == 'invalid_json' else json.dumps(reply, ensure_ascii=False)
                payload = json.dumps({
                    'choices': [{'message': {'role': 'assistant', 'content': content},
                                 'finish_reason': 'stop'}],
                    'usage': {'prompt_tokens': 100, 'completion_tokens': 50},
                }, ensure_ascii=False).encode('utf-8')
                try:
                    self.send_response(200)
                    self.send_header('Content-Type', 'application/json')
                    self.send_header('Content-Length', str(len(payload)))
                    self.end_headers()
                    self.wfile.write(payload)
                except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                    pass

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.server.daemon_threads = True
        self.port = self.server.server_port
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *_args):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)


def run(artifact_root: Path, selected_scenario: str | None = None) -> Path:
    base.ARTIFACT_PREFIX = 'WhiteoutStation-v1.4-Win64-'
    base.AGENT_RUNTIME_REL = Path('Windows/WhiteoutStation/Content/Agents/AgentRuntime.v1.4.json')
    base.AGENT_RUNTIME_VERSION = '1.4.0'
    base.AGENT_SCHEMA_VERSION = 7
    suffix = '-' + selected_scenario if selected_scenario else ''
    base.OUTPUT_REL = Path('Validation/ShippingSmokeV14' + suffix)
    root, executable = base.validate_artifact_root(artifact_root)
    content = root / 'Windows/WhiteoutStation/Content/Dialogue/v1.4'
    for name in ('WorldKnowledge', 'NPC_GuHeng', 'NPC_YeCheng',
                 'Relationship_GuHeng_YeCheng', 'DialoguePolicy', 'SafeFallbacks'):
        base.load_json(content / (name + '.json'))
    output = root / base.OUTPUT_REL
    output.mkdir(parents=True)
    base.force_empty_credential_inputs()
    scenarios = [(route, 'offline') for route in base.EXPECTED_ROUTES]
    scenarios += [('medical', 'valid'), ('technical', 'valid')]
    scenarios += [('quick', mode) for mode in (
        'valid', 'forbidden_fact', 'unknown_knowledge', 'invalid_proposal', 'invalid_json', 'timeout')]
    baseline = {}
    summaries = []
    for route, mode in scenarios:
        if selected_scenario and route + '_' + mode != selected_scenario:
            continue
        runtime = output / (route + '_' + mode)
        runtime.mkdir()
        online = mode != 'offline'
        expected = base.EXPECTED_ROUTES[route]
        expected_count = sum(action in {'talk_gu_heng', 'talk_ye_cheng'} for action in expected['actions'])
        expected_calls = expected_count if online else 0
        scenario = base.Scenario(runtime.name, route, 'loopback_mock' if online else 'explicit_offline', int(online))
        with RoleplayEndpoint(mode) as endpoint:
            command = base.build_command(executable, runtime, scenario, endpoint.port if online else None)
            if mode == 'timeout':
                command.append('-WhiteoutAgentTimeoutSeconds=1')
            code = base.run_process(command, executable.parent, 120)
        if code:
            raise base.SmokeError(f'{runtime.name}: process exit {code}')
        if endpoint.errors or endpoint.attempts != expected_calls:
            raise base.SmokeError(f'{runtime.name}: HTTP contract/attempt count failed: {endpoint.errors}, {endpoint.attempts}')
        event = base.load_json(runtime / base.EVENT_LOG_REL)
        if ([e['action_id'] for e in event['events']] != expected['actions']
                or any(e['reason_code'] != 'Committed' for e in event['events'])
                or event['ending'] != expected['ending']
                or event['signal_sent'] != expected['signal_sent']
                or event['remaining_ap'] != expected['remaining_ap']
                or event['model_calls'] != expected_calls):
            raise base.SmokeError(f'{runtime.name}: route or authoritative result mismatch: '
                                 f"ending={event['ending']}, AP={event['remaining_ap']}, calls={event['model_calls']}")
        if base.png_size(runtime / base.SCREENSHOT_REL) != (1280, 720):
            raise base.SmokeError(f'{runtime.name}: runtime screenshot missing or invalid')
        state = previous.extract_gameplay_state(event)
        if not online:
            baseline[route] = state
        elif route in baseline:
            previous.validate_ai_ab_equivalence(baseline[route], state)
        audit_path = runtime / previous.DIALOGUE_AUDIT_REL
        records = base.load_json_lines(audit_path, runtime.name) if audit_path.exists() else []
        fields = previous.DIALOGUE_AUDIT_FIELDS | {
            'referenced_knowledge_ids', 'speech_function', 'turn_index', 'proposal_type'}
        if len(records) != expected_count or any(set(record) != fields for record in records):
            raise base.SmokeError(f'{runtime.name}: dialogue audit whitelist/count mismatch')
        for index, record in enumerate(records):
            previous._validate_audit_string_values(record, index)
        if online:
            wanted = 'online_roleplay' if mode == 'valid' else 'local_natural_fallback'
            if any(record['answer_source'] != wanted for record in records):
                raise base.SmokeError(f'{runtime.name}: expected {wanted}')
            model_records = base.load_json_lines(runtime / base.MODEL_AUDIT_REL, runtime.name)
            if len(model_records) != expected_calls:
                raise base.SmokeError(f'{runtime.name}: model audit count mismatch')
        summaries.append({'scenario': runtime.name, 'passed': True, 'http_attempts': endpoint.attempts,
                          'ending': event['ending'], 'remaining_ap': event['remaining_ap'],
                          'score': event['score'], 'dialogue_audits': len(records),
                          'ai_ab_equal': True if online and route in baseline else None})
        print(f'{runtime.name}: PASS', flush=True)
    report = output / 'summary.json'
    report.write_text(json.dumps({'schema': 'whiteout.v1.4.shipping-smoke.v1', 'passed': True,
                                  'scenarios': summaries}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return report


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('--artifact-root', type=Path, required=True)
    parser.add_argument('--scenario', choices=[
        'medical_offline', 'technical_offline', 'quick_offline', 'wait_offline', 'collapse_offline',
        'medical_valid', 'technical_valid', 'quick_valid', 'quick_forbidden_fact',
        'quick_unknown_knowledge', 'quick_invalid_proposal', 'quick_invalid_json', 'quick_timeout'])
    args = parser.parse_args()
    try:
        print('SHIPPING SMOKE v1.4: PASS', run(args.artifact_root, args.scenario))
    except (base.SmokeError, OSError, ValueError, KeyError) as error:
        print(f'SHIPPING SMOKE v1.4: FAIL: {error}')
        raise SystemExit(1)
