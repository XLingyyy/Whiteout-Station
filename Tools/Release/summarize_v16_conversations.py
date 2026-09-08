"""Separate natural replies, local controls, failures and real-action fixtures."""
import argparse
from collections import Counter
import json
from pathlib import Path


def percentile(values, percent):
    if not values:
        return None
    ordered = sorted(values)
    return round(ordered[round((len(ordered) - 1) * percent)], 3)


def summarize(path):
    data = json.loads(path.read_text(encoding='utf-8-sig'))
    counts = Counter()
    latency, critical, ordinary = [], [], []
    failures = []
    for case in data['cases']:
        for row in case['result'].get('steps', []):
            if row.get('kind') == 'fixture_action':
                counts['fixture_actions'] += 1
                counts['fixture_failures'] += not row.get('committed', False)
                continue
            counts['messages'] += 1
            calls = row.get('model_calls', 0)
            counts['model_calls'] += calls
            elapsed = row.get('elapsed_seconds', 0)
            if calls:
                latency.append(elapsed)
                (critical if calls == 3 else ordinary).append(elapsed)
            counts['unexpected_ap_changes'] += row['ap_after'] != row['ap_before']
            counts['call_limit_violations'] += calls > 3
            counts['deadline_violations'] += elapsed > 15.5
            if row.get('source') == 'natural_roleplay_v16':
                counts['natural_replies'] += 1
            elif row.get('committed') or row.get('history_after', 0) > row.get('history_before', 0):
                counts['local_controls_or_clarifications'] += 1
            elif calls == 0:
                counts['local_rejections'] += 1
            else:
                counts['online_failures'] += 1
                failures.append(dict(case=case['id'], text=row['text'], reason=row.get('result'), calls=calls))
    return dict(input=str(path), engine_exit=data['engine_exit'], cases=len(data['cases']), counts=dict(counts),
                request_latency_seconds=dict(p50=percentile(latency, .5), p95=percentile(latency, .95),
                                             maximum=max(latency, default=0),
                                             critical_p95=percentile(critical, .95),
                                             one_or_two_call_p95=percentile(ordinary, .95)),
                online_failures=failures,
                scope='Transport/transaction statistics only. Reply completeness and factual accuracy require separate review.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path, nargs='+')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = [summarize(path) for path in args.results]
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    for item in result:
        print(json.dumps({key: value for key, value in item.items() if key not in ['online_failures', 'scope']}, ensure_ascii=False))
