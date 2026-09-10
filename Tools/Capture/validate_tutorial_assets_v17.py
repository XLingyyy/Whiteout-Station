"""Reject missing, stale, unapproved or untraceable tutorial captures."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
ROLES = {'T01': {'Dialogue'}, 'T02': {'Interactable'}, 'T03': {'AP', 'CostPreview'}, 'T04': {'Player'}, 'T05': {'NPC'}}

def validate(manifest, evidence_root, content=None):
    if not manifest.is_file(): return ['尚未建立教程采集清单']
    errors = []
    try:
        data = json.loads(manifest.read_text(encoding='utf-8-sig'))
        if data.get('ui_revision') != 1 or data.get('tutorial_version') != 1: errors.append('stale UI/tutorial revision')
        entries = data['captures']
        ids = set()
        for c in entries:
            ident = c['capture_id']
            if ident in ids: errors.append(f'duplicate capture {ident}')
            ids.add(ident)
            if c['image_role'] not in ROLES.get(c['page_id'], set()): errors.append(f'{ident}: wrong role')
            if c['ui_revision'] != data['ui_revision']: errors.append(f'{ident}: stale UI')
            if c['resource_status'] != 'Approved' or not c['approved_by'] or not c['approved_at']: errors.append(f'{ident}: unapproved')
            for field in ['source_commit', 'build_id', 'map_package', 'fixture_id', 'language', 'mode']:
                if not c.get(field): errors.append(f'{ident}: missing {field}')
            from PIL import Image
            source = evidence_root / c['source_frame']
            crop = evidence_root / c['cropped_file']
            for path, key in [(source, 'source_sha256'), (crop, 'cropped_sha256')]:
                if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != c[key]: errors.append(f'{ident}: invalid {key}')
            if source.is_file() and crop.is_file():
                x,y,w,h = c['crop_px']; size = Image.open(source).size
                if [*size] != c['viewport_px'] or min(x,y) < 0 or min(w,h) <= 0 or x+w > size[0] or y+h > size[1]: errors.append(f'{ident}: crop outside frame')
                with Image.open(source) as full, Image.open(crop) as part:
                    if part.size != (w,h) or full.crop((x,y,x+w,y+h)).convert('RGB').tobytes() != part.convert('RGB').tobytes(): errors.append(f'{ident}: crop pixels changed')
            if content and not (content / c['asset_file']).is_file(): errors.append(f'{ident}: texture asset missing')
        for page, roles in ROLES.items():
            for role in roles:
                if not any(c['page_id']==page and c['image_role']==role for c in entries): errors.append(f'{page}: missing {role}')
        if not all(any(c['page_id']=='T01' and c['mode']==mode for c in entries) for mode in ['offline','online']): errors.append('T01 requires both modes')
        ap_sources = {c['source_frame'] for c in entries if c['page_id']=='T03'}
        if len(ap_sources) != 1: errors.append('AP and cost preview must share one frame')
    except (KeyError, OSError, ValueError, TypeError) as exc:
        errors.append(str(exc))
    return errors

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=ROOT/'docs/QA/v1.7_tutorial_assets.json')
    parser.add_argument('--evidence-root', type=Path, default=ROOT/'Artifacts/v1.7-evidence')
    parser.add_argument('--content', type=Path, default=ROOT/'WhiteoutStation/Content')
    args=parser.parse_args()
    errors=validate(args.manifest,args.evidence_root,args.content)
    for error in errors: print(error)
    print(f'v1.7 tutorial assets: {len(errors)} errors')
    sys.exit(bool(errors))
