"""Spectral and level checks for WAV files rendered by LabX3Render.

Usage:
    python Tests/analyze.py file.wav [file2.wav ...] [--pitch HZ] [--onsets RATE] [--json]

Prints, per file: peak and RMS in dBFS, DC offset, dominant frequency, the share of energy in
the bands used for the reference analysis, the largest sample-to-sample jump, and optional pass or
fail checks for pitch (within 1 cent) and click onset rate (within 30 %).
"""

import argparse
import json
import math
import sys

import numpy as np
import soundfile as sf

BANDS = [("sub<60", 20, 60), ("60-250", 60, 250), ("250-1k", 250, 1000),
         ("1k-4k", 1000, 4000), ("4k-10k", 4000, 10000), (">10k", 10000, 22000)]


def db(x, floor=-120.0):
    return 20.0 * math.log10(x) if x > 0 else floor


def dominant_frequency(mono, sr, fmin=20.0):
    n = 1 << int(math.floor(math.log2(min(len(mono), 1 << 18))))
    seg = mono[:n] * np.hanning(n)
    spec = np.abs(np.fft.rfft(seg))
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    lo = int(fmin * n / sr)
    k = lo + int(np.argmax(spec[lo:]))
    if 0 < k < len(spec) - 1:
        a, b, c = np.log(spec[k - 1] + 1e-12), np.log(spec[k] + 1e-12), np.log(spec[k + 1] + 1e-12)
        shift = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0.0
        return (k + shift) * sr / n
    return freqs[k]


def band_split(mono, sr):
    n = 8192
    if len(mono) < n:
        return {}
    win = np.hanning(n)
    acc = np.zeros(n // 2 + 1)
    for i in range(0, len(mono) - n, n // 2):
        acc += np.abs(np.fft.rfft(mono[i:i + n] * win)) ** 2
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    total = acc.sum() + 1e-20
    return {name: round(10 * math.log10(acc[(freqs >= lo) & (freqs < hi)].sum() / total + 1e-20), 1)
            for name, lo, hi in BANDS}


def onset_rate(mono, sr):
    # Count sharp rises in a 1 ms high-passed energy envelope.
    hp = np.diff(mono, prepend=mono[0])
    frame = max(1, int(sr * 0.001))
    usable = len(hp) // frame * frame
    env = np.sqrt((hp[:usable].reshape(-1, frame) ** 2).mean(axis=1))
    threshold = max(np.median(env) * 6.0, env.max() * 0.08)
    above = env > threshold
    onsets = np.count_nonzero(above[1:] & ~above[:-1])
    return onsets / (len(mono) / sr)


def analyse(path, args):
    data, sr = sf.read(path, always_2d=True)
    finite = np.isfinite(data)
    result = {"file": path, "sr": sr, "non_finite": int((~finite).sum())}
    data = np.where(finite, data, 0.0)
    mono = data.mean(axis=1)

    result["peak_db"] = round(db(np.abs(data).max()), 2)
    result["rms_db"] = round(db(math.sqrt((data ** 2).mean())), 2)
    result["dc"] = round(float(mono.mean()), 6)
    result["max_jump"] = round(float(np.abs(np.diff(data, axis=0)).max()), 4)
    result["max_side"] = round(float(np.abs(data[:, 0] - data[:, -1]).max()), 6) if data.shape[1] > 1 else 0.0
    result["dominant_hz"] = round(dominant_frequency(mono, sr), 2)
    result["bands_db"] = band_split(mono, sr)

    checks = []
    if args.pitch:
        cents = 1200 * math.log2(result["dominant_hz"] / args.pitch)
        result["pitch_error_cents"] = round(cents, 3)
        checks.append(abs(cents) <= 1.0)
    if args.onsets:
        rate = onset_rate(mono, sr)
        result["onsets_per_s"] = round(rate, 2)
        checks.append(abs(rate - args.onsets) <= 0.3 * args.onsets)
    checks.append(result["non_finite"] == 0)
    result["pass"] = all(checks)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="+")
    parser.add_argument("--pitch", type=float)
    parser.add_argument("--onsets", type=float)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    results = [analyse(f, args) for f in args.files]
    if args.json:
        print(json.dumps(results, indent=2))
    else:
        for r in results:
            extras = " ".join(f"{k}={r[k]}" for k in ("pitch_error_cents", "onsets_per_s") if k in r)
            bands = " ".join(f"{k}:{v}" for k, v in r["bands_db"].items())
            print(f"{'PASS' if r['pass'] else 'FAIL'} {r['file']} peak={r['peak_db']} rms={r['rms_db']} "
                  f"dc={r['dc']} jump={r['max_jump']} f0={r['dominant_hz']} {extras}\n      bands {bands}")
    sys.exit(0 if all(r["pass"] for r in results) else 1)


if __name__ == "__main__":
    main()
