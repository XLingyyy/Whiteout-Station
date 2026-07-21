"""Generate the original and CC0-derived audio layers used by the v0.2 demo.

The project keeps this synthesis deterministic so every tracked WAV can be
reproduced without a proprietary DAW or a network service.  The indoor wind
bed is an equalised derivative of the pinned CC0 Freesound ambience; all other
signals are original procedural synthesis made for Whiteout Station.
"""

from __future__ import annotations

import hashlib
import json
import math
import wave
from pathlib import Path

import numpy as np


REPO_ROOT = Path(__file__).resolve().parents[2]
OUTPUT_ROOT = REPO_ROOT / "SourceAssets" / "GeneratedAudio"
WIND_SOURCE = REPO_ROOT / "SourceAssets" / "Freesound" / "344887" / "S_Wind_Strong_CC0.wav"
SAMPLE_RATE = 24_000
SEED = 20_260_721
RNG = np.random.default_rng(SEED)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_wav(path: Path) -> tuple[np.ndarray, int]:
    with wave.open(str(path), "rb") as source:
        if source.getsampwidth() != 2:
            raise RuntimeError(f"Expected PCM16 input: {path}")
        channels = source.getnchannels()
        rate = source.getframerate()
        data = np.frombuffer(source.readframes(source.getnframes()), dtype="<i2").astype(np.float32)
    return data.reshape(-1, channels) / 32768.0, rate


def write_wav(relative_path: str, signal: np.ndarray, peak: float = 0.92) -> Path:
    path = OUTPUT_ROOT / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    data = np.asarray(signal, dtype=np.float32)
    maximum = float(np.max(np.abs(data))) if data.size else 0.0
    if maximum > peak:
        data = data * (peak / maximum)
    pcm = np.round(np.clip(data, -1.0, 1.0) * 32767.0).astype("<i2")
    channels = 1 if pcm.ndim == 1 else pcm.shape[1]
    with wave.open(str(path), "wb") as target:
        target.setnchannels(channels)
        target.setsampwidth(2)
        target.setframerate(SAMPLE_RATE)
        target.writeframes(pcm.tobytes())
    return path


def lowpass(signal: np.ndarray, cutoff: float, order: float = 4.0) -> np.ndarray:
    data = np.asarray(signal, dtype=np.float32)
    spectrum = np.fft.rfft(data, axis=0)
    frequencies = np.fft.rfftfreq(data.shape[0], 1.0 / SAMPLE_RATE)
    gain = 1.0 / np.sqrt(1.0 + np.power(frequencies / max(cutoff, 1.0), 2.0 * order))
    if data.ndim > 1:
        gain = gain[:, None]
    return np.fft.irfft(spectrum * gain, n=data.shape[0], axis=0).astype(np.float32)


def highpass(signal: np.ndarray, cutoff: float) -> np.ndarray:
    return np.asarray(signal, dtype=np.float32) - lowpass(signal, cutoff)


def smooth_noise(samples: int, cutoff: float, channels: int = 1) -> np.ndarray:
    shape = (samples,) if channels == 1 else (samples, channels)
    return lowpass(RNG.normal(0.0, 1.0, shape).astype(np.float32), cutoff)


def fade(signal: np.ndarray, attack: float, release: float) -> np.ndarray:
    data = np.asarray(signal, dtype=np.float32).copy()
    attack_samples = min(data.shape[0], int(attack * SAMPLE_RATE))
    release_samples = min(data.shape[0], int(release * SAMPLE_RATE))
    if attack_samples:
        data[:attack_samples] *= np.linspace(0.0, 1.0, attack_samples, dtype=np.float32).reshape((-1,) + (1,) * (data.ndim - 1))
    if release_samples:
        data[-release_samples:] *= np.linspace(1.0, 0.0, release_samples, dtype=np.float32).reshape((-1,) + (1,) * (data.ndim - 1))
    return data


def make_loopable(signal: np.ndarray, crossfade_seconds: float = 0.65) -> np.ndarray:
    data = np.asarray(signal, dtype=np.float32).copy()
    count = min(data.shape[0] // 3, int(crossfade_seconds * SAMPLE_RATE))
    alpha = np.linspace(0.0, 1.0, count, dtype=np.float32).reshape((-1,) + (1,) * (data.ndim - 1))
    data[-count:] = data[-count:] * (1.0 - alpha) + data[:count] * alpha
    return data


def tone(duration: float, frequencies: list[float], amplitudes: list[float] | None = None) -> np.ndarray:
    samples = int(duration * SAMPLE_RATE)
    t = np.arange(samples, dtype=np.float32) / SAMPLE_RATE
    if amplitudes is None:
        amplitudes = [1.0 / len(frequencies)] * len(frequencies)
    result = np.zeros(samples, dtype=np.float32)
    for frequency, amplitude in zip(frequencies, amplitudes, strict=True):
        result += amplitude * np.sin(2.0 * np.pi * frequency * t)
    return result


def stereo(signal: np.ndarray, width: float = 0.12) -> np.ndarray:
    mono = np.asarray(signal, dtype=np.float32)
    delay = max(1, int(width * 0.001 * SAMPLE_RATE))
    right = np.roll(mono, delay)
    right[:delay] = 0.0
    return np.column_stack((mono, right))


def build_indoor_wind() -> np.ndarray:
    source, rate = read_wav(WIND_SOURCE)
    if rate != SAMPLE_RATE:
        raise RuntimeError(f"Pinned wind sample rate changed: {rate}")
    filtered = lowpass(source, 820.0)
    rumble = lowpass(source, 120.0) * 0.85
    modulation = 0.72 + 0.16 * np.sin(2.0 * np.pi * 0.11 * np.arange(source.shape[0]) / SAMPLE_RATE)
    return make_loopable((filtered * 0.42 + rumble) * modulation[:, None])


def build_generator() -> np.ndarray:
    duration = 8.0
    samples = int(duration * SAMPLE_RATE)
    t = np.arange(samples, dtype=np.float32) / SAMPLE_RATE
    wobble = 1.0 + 0.035 * np.sin(2.0 * np.pi * 1.75 * t) + 0.018 * np.sin(2.0 * np.pi * 3.5 * t)
    engine = (
        0.52 * np.sin(2.0 * np.pi * 30.0 * t + 0.10 * np.sin(2.0 * np.pi * 2.0 * t))
        + 0.28 * np.sin(2.0 * np.pi * 60.0 * t)
        + 0.14 * np.sin(2.0 * np.pi * 90.0 * t)
        + 0.08 * np.sin(2.0 * np.pi * 120.0 * t)
    )
    mechanical = smooth_noise(samples, 430.0) * 0.28 + highpass(smooth_noise(samples, 2400.0), 700.0) * 0.035
    return make_loopable(stereo((engine * wobble + mechanical) * 0.55, 0.45), 0.35)


def build_footstep(kind: str) -> np.ndarray:
    duration = {"snow": 0.58, "metal": 0.44, "concrete": 0.46}[kind]
    samples = int(duration * SAMPLE_RATE)
    t = np.arange(samples, dtype=np.float32) / SAMPLE_RATE
    envelope = np.exp(-t * {"snow": 7.0, "metal": 9.0, "concrete": 10.0}[kind])
    if kind == "snow":
        crunch = highpass(RNG.normal(0.0, 1.0, samples).astype(np.float32), 760.0)
        grains = np.zeros(samples, dtype=np.float32)
        for offset in (0.045, 0.075, 0.115, 0.17, 0.235):
            start = int(offset * SAMPLE_RATE)
            length = min(int(0.055 * SAMPLE_RATE), samples - start)
            grains[start : start + length] += RNG.normal(0.0, 1.0, length) * np.hanning(length)
        thump = np.sin(2.0 * np.pi * 78.0 * t) * np.exp(-t * 18.0)
        return fade(0.24 * crunch * envelope + 0.42 * grains + 0.32 * thump, 0.004, 0.06)
    if kind == "metal":
        impact = RNG.normal(0.0, 1.0, samples).astype(np.float32) * np.exp(-t * 45.0)
        ring = (
            0.58 * np.sin(2.0 * np.pi * 238.0 * t)
            + 0.29 * np.sin(2.0 * np.pi * 417.0 * t)
            + 0.16 * np.sin(2.0 * np.pi * 713.0 * t)
        ) * np.exp(-t * 8.5)
        return fade(0.38 * impact + 0.42 * ring, 0.002, 0.04)
    grit = highpass(RNG.normal(0.0, 1.0, samples).astype(np.float32), 420.0) * np.exp(-t * 15.0)
    thump = (np.sin(2.0 * np.pi * 72.0 * t) + 0.35 * np.sin(2.0 * np.pi * 133.0 * t)) * np.exp(-t * 19.0)
    return fade(0.27 * grit + 0.44 * thump, 0.003, 0.05)


def build_ui(kind: str) -> np.ndarray:
    if kind == "hover":
        return fade(tone(0.11, [660.0, 990.0], [0.55, 0.22]), 0.003, 0.07)
    if kind == "confirm":
        base = tone(0.42, [392.0, 587.33, 783.99], [0.35, 0.27, 0.18])
        return fade(base, 0.005, 0.24)
    if kind == "reject":
        samples = int(0.46 * SAMPLE_RATE)
        t = np.arange(samples, dtype=np.float32) / SAMPLE_RATE
        sweep = np.sin(2.0 * np.pi * (230.0 * t - 105.0 * t * t))
        return fade(sweep * 0.52 + tone(0.46, [117.0], [0.28]), 0.003, 0.20)
    base = tone(0.82, [523.25, 659.25, 783.99, 1046.5], [0.23, 0.21, 0.18, 0.13])
    return fade(base, 0.008, 0.42)


def build_crisis() -> np.ndarray:
    duration = 2.8
    samples = int(duration * SAMPLE_RATE)
    t = np.arange(samples, dtype=np.float32) / SAMPLE_RATE
    impact = lowpass(RNG.normal(0.0, 1.0, samples).astype(np.float32), 170.0) * np.exp(-t * 4.2)
    drop = np.sin(2.0 * np.pi * (92.0 * t - 11.5 * t * t)) * np.exp(-t * 0.62)
    alarm = np.sin(2.0 * np.pi * 740.0 * t) * (np.sin(2.0 * np.pi * 3.2 * t) > 0.15) * np.exp(-t * 0.42)
    return fade(stereo(0.52 * impact + 0.46 * drop + 0.19 * alarm, 1.2), 0.004, 0.30)


def chord_bed(duration: float, frequencies: list[float], pulse: float = 0.0) -> np.ndarray:
    samples = int(duration * SAMPLE_RATE)
    t = np.arange(samples, dtype=np.float32) / SAMPLE_RATE
    result = np.zeros(samples, dtype=np.float32)
    for index, frequency in enumerate(frequencies):
        phase = index * 0.73
        result += (0.18 / (1.0 + index * 0.22)) * np.sin(2.0 * np.pi * frequency * t + phase)
        result += (0.08 / (1.0 + index * 0.22)) * np.sin(2.0 * np.pi * frequency * 2.0 * t + phase * 0.5)
    if pulse:
        result *= 0.74 + 0.26 * np.sin(2.0 * np.pi * pulse * t) ** 2
    result += lowpass(RNG.normal(0.0, 1.0, samples).astype(np.float32), 220.0) * 0.045
    return fade(stereo(result, 1.6), 1.1, 2.2)


def build_radio_reply() -> np.ndarray:
    duration = 3.6
    samples = int(duration * SAMPLE_RATE)
    t = np.arange(samples, dtype=np.float32) / SAMPLE_RATE
    static = highpass(RNG.normal(0.0, 1.0, samples).astype(np.float32), 520.0) * 0.12
    carrier = np.zeros(samples, dtype=np.float32)
    for start, length, frequency in ((0.42, 0.18, 880.0), (0.76, 0.10, 880.0), (1.02, 0.28, 660.0), (1.48, 0.18, 990.0), (1.82, 0.34, 783.99), (2.38, 0.22, 1046.5)):
        begin = int(start * SAMPLE_RATE)
        end = min(samples, begin + int(length * SAMPLE_RATE))
        local_t = np.arange(end - begin, dtype=np.float32) / SAMPLE_RATE
        carrier[begin:end] += np.sin(2.0 * np.pi * frequency * local_t) * np.hanning(end - begin)
    squelch = highpass(RNG.normal(0.0, 1.0, samples).astype(np.float32), 1600.0) * np.exp(-np.maximum(t - 2.95, 0.0) * 7.0)
    squelch[t < 2.95] = 0.0
    return fade(stereo(static + carrier * 0.42 + squelch * 0.24, 0.9), 0.02, 0.18)


def main() -> int:
    assets: dict[str, dict[str, object]] = {}

    generated = {
        "Ambience/S_WindIndoor_CC0_Derivative.wav": (build_indoor_wind(), True, "CC0 derivative"),
        "Machinery/S_GeneratorLoop_Original.wav": (build_generator(), True, "Original procedural"),
        "Foley/S_FootstepSnow_Original.wav": (build_footstep("snow"), False, "Original procedural"),
        "Foley/S_FootstepMetal_Original.wav": (build_footstep("metal"), False, "Original procedural"),
        "Foley/S_FootstepConcrete_Original.wav": (build_footstep("concrete"), False, "Original procedural"),
        "UI/S_UIHover_Original.wav": (build_ui("hover"), False, "Original procedural"),
        "UI/S_UIConfirm_Original.wav": (build_ui("confirm"), False, "Original procedural"),
        "UI/S_UIReject_Original.wav": (build_ui("reject"), False, "Original procedural"),
        "UI/S_UIPromise_Original.wav": (build_ui("promise"), False, "Original procedural"),
        "Events/S_CrisisStinger_Original.wav": (build_crisis(), False, "Original procedural"),
        "Events/S_RadioReply_Original.wav": (build_radio_reply(), False, "Original procedural"),
        "Music/S_EndingSuccess_Original.wav": (chord_bed(12.0, [65.41, 98.0, 130.81, 196.0], 0.18), False, "Original procedural"),
        "Music/S_EndingSurvival_Original.wav": (chord_bed(12.0, [55.0, 82.41, 110.0, 146.83], 0.13), False, "Original procedural"),
        "Music/S_EndingCost_Original.wav": (chord_bed(12.0, [58.27, 87.31, 116.54, 123.47], 0.31), False, "Original procedural"),
        "Music/S_EndingCollapse_Original.wav": (chord_bed(12.0, [41.20, 61.74, 77.78, 92.50], 0.42), False, "Original procedural"),
    }

    for relative_path, (signal, looping, provenance) in generated.items():
        path = write_wav(relative_path, signal)
        with wave.open(str(path), "rb") as stream:
            assets[relative_path] = {
                "sha256": sha256(path),
                "sample_rate": stream.getframerate(),
                "channels": stream.getnchannels(),
                "frames": stream.getnframes(),
                "duration_seconds": round(stream.getnframes() / stream.getframerate(), 4),
                "looping": looping,
                "provenance": provenance,
            }

    manifest = {
        "generator": "Tools/Assets/generate_v02_audio.py",
        "generator_version": 1,
        "seed": SEED,
        "source_wind": {
            "path": str(WIND_SOURCE.relative_to(REPO_ROOT)).replace("\\", "/"),
            "sha256": sha256(WIND_SOURCE),
            "license": "CC0 1.0",
            "source_page": "https://freesound.org/people/lextrack/sounds/344887/",
        },
        "license_note": "All non-derivative files are original procedural audio authored for this repository.",
        "assets": assets,
    }
    (OUTPUT_ROOT / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(f"generated {len(assets)} audio layers in {OUTPUT_ROOT.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
