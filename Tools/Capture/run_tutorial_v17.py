"""Capture real v1.7 UI or run the rendered tutorial integration probe in isolated user data."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mode', default='all', choices=['all', 'hud', 'interaction', 'ap', 'dialogue', 'dialogue_online', 'probe'])
    p.add_argument('--exe', type=Path, default=Path('G:/UnrealEngine/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'))
    p.add_argument('--width', type=int, default=1920)
    p.add_argument('--height', type=int, default=1080)
    p.add_argument('--scale', type=float, default=1)
    p.add_argument('--label', default='')
    p.add_argument('--keep-open', action='store_true')
    args = p.parse_args()
    evidence = ROOT/'Artifacts/v1.7-evidence'
    evidence.mkdir(parents=True, exist_ok=True)
    label = args.mode + args.label
    command = [str(args.exe)]
    if args.exe.name.startswith('UnrealEditor'):
        command += [str(ROOT/'WhiteoutStation/WhiteoutStation.uproject'), '-game']
    command += ['-windowed', f'-ResX={args.width}', f'-ResY={args.height}', '-ForceRes', '-nosplash', '-nop4', '-nosound',
                '-unattended', '-WhiteoutV17Capture', f'-V17Frame={args.mode}', f'-V17Label={args.label}',
                f'-WhiteoutCaptureScale={args.scale}', f'-UserDir={evidence}/capture-user-{label}', f'-abslog={evidence}/capture-{label}.log']
    if not args.keep_open: command += ['-WhiteoutAutoExit']
    result = subprocess.run(command, timeout=None if args.keep_open else 360)
    print(json.dumps({'mode': args.mode, 'label': args.label, 'exit_code': result.returncode}))
    return result.returncode

if __name__ == '__main__':
    raise SystemExit(main())
