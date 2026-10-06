"""Automated offline checks for LAB X-3.

Runs the LabX3Render harness through a battery of renders and verifies them with analyze.py.
Usage:  python Tests/run_tests.py [--library <stalker sounds folder>] [--keep]

Writes WAVs to Tests/out (ignored by git) and a JSON report to build/test-report.json.
"""

import argparse
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
HARNESS = ROOT / "build" / "LabX3Render_artefacts" / "Release" / "LabX3Render.exe"
OUT = ROOT / "Tests" / "out"
sys.path.insert(0, str(ROOT / "Tests"))
import analyze  # noqa: E402

# A neutral patch: one pure sine, no drift, no effects. Used for pitch and click-detector tests.
CLEAN = ["osc_a_wave=2", "osc_a_shape=0", "osc_a_level=1", "osc_b_level=0", "sub_level=0", "noise_level=0",
         "specimen_level=0", "whisper_level=0", "presence_level=0", "geiger_density=0",
         "filter_cutoff=20000", "filter_res=0", "filter_env=0", "filter_keytrack=0", "filter_drive=0",
         "env1_attack=0.001", "env1_sustain=1", "env1_release=0.05",
         "prog_depth=0", "dark=0", "noo_mix=0", "scrub_bits=16", "scrub_rate=1", "stereo_width=0",
         "master_volume=-6"]

STAT = re.compile(r'(\w+)=("([^"]*)"|\S+)')


def render(name, extra, library=None):
    OUT.mkdir(parents=True, exist_ok=True)
    wav = OUT / f"{name}.wav"
    cmd = [str(HARNESS), "--out", str(wav)] + extra
    if library:
        cmd += ["--library", library]
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    stats = {}
    for line in proc.stdout.splitlines():
        for key, value, quoted in STAT.findall(line):
            stats[key] = quoted if value.startswith('"') else value
    stats["exit"] = proc.returncode
    if proc.returncode != 0:
        stats["stderr"] = proc.stderr.strip()[-400:]
    return wav, stats


def sets(values):
    out = []
    for v in values:
        out += ["--set", v]
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--library")
    args = parser.parse_args()

    if not HARNESS.exists():
        print(f"harness not built: {HARNESS}")
        return 2

    presets = [line.split("\t")[1] for line in
               subprocess.run([str(HARNESS), "--list"], capture_output=True, text=True).stdout.splitlines() if "\t" in line]

    report = {"tests": []}

    def record(name, ok, detail):
        report["tests"].append({"name": name, "pass": bool(ok), **detail})
        print(f"{'PASS' if ok else 'FAIL'}  {name}  {json.dumps(detail)[:300]}")

    class A:  # analyzer options
        pitch = None
        onsets = None

    # 1. Pitch accuracy: A4 must render at 440 Hz within 1 cent.
    wav, st = render("pitch_a4", ["--notes", "69", "--seconds", "3", "--hold", "2.5"] + sets(CLEAN), args.library)
    opts = A(); opts.pitch = 440.0
    res = analyze.analyse(str(wav), opts)
    record("pitch A4 = 440 Hz", res["pass"], {"f0": res["dominant_hz"], "cents": res.get("pitch_error_cents")})

    # 2. Geiger density 10/s on a silent voice should produce ~10 clicks per second.
    wav, st = render("geiger_10", ["--notes", "60", "--seconds", "6", "--hold", "6", "--velocity", "1"]
                     + sets(CLEAN + ["osc_a_level=0", "geiger_density=10"]), args.library)
    opts = A(); opts.onsets = 10.0
    res = analyze.analyse(str(wav), opts)
    record("geiger density 10/s", res["pass"], {"onsets_per_s": res.get("onsets_per_s")})

    # 3. Every factory preset: finite, below full scale, clearly audible.
    for i, name in enumerate(presets):
        mono = name in ("Dark's Programming", "Blowout Siren")
        notes = ["--events", "53@0-1.8,60@1.6-4"] if mono else ["--notes", "48,55,60"]
        wav, st = render(f"preset_{i:02d}", ["--preset", str(i), "--seconds", "7", "--hold", "4"] + notes, args.library)
        res = analyze.analyse(str(wav), A())
        ok = st.get("exit") == 0 and res["non_finite"] == 0 and res["peak_db"] <= 0.0 and res["rms_db"] > -45.0
        record(f"preset {i:02d} {name}", ok, {"peak": res["peak_db"], "rms": res["rms_db"], "specimen": st.get("specimen"),
                                              "bands": res["bands_db"], "rt_x": st.get("realtime_x")})

    # 4. Voice stealing: 12 overlapping notes on 8 voices.
    events = ",".join(f"{36 + 3 * k}@{0.2 * k:.1f}-{3.0 + 0.1 * k:.1f}" for k in range(12))
    wav, st = render("stealing", ["--preset", "2", "--events", events, "--seconds", "6"], args.library)
    res = analyze.analyse(str(wav), A())
    record("voice stealing 12 on 8", st.get("exit") == 0 and res["non_finite"] == 0 and res["peak_db"] <= 0.0,
           {"peak": res["peak_db"], "jump": res["max_jump"]})

    # 5. Mono legato with glide.
    wav, st = render("legato", ["--events", "48@0-1.2,55@1.0-2.2,60@2.0-3.0,53@2.8-4.0", "--seconds", "5"]
                     + sets(CLEAN + ["voices=1", "glide=0.2"]), args.library)
    res = analyze.analyse(str(wav), A())
    record("mono legato glide", st.get("exit") == 0 and res["non_finite"] == 0 and res["max_jump"] < 0.2,
           {"jump": res["max_jump"], "peak": res["peak_db"]})

    # 6. State round trip.
    wav, st = render("roundtrip", ["--preset", "5", "--seconds", "1", "--roundtrip"], args.library)
    record("state round trip", st.get("roundtrip") == "PASS", {"state_bytes": st.get("state_bytes")})

    # 7. CPU: 8 voices on the heaviest preset at 48 kHz, 512-sample blocks.
    wav, st = render("bench", ["--preset", "4", "--notes", "36,43,48,55,60,63,67,72", "--seconds", "10", "--hold", "8"],
                     args.library)
    rt = float(st.get("realtime_x", 0))
    record("cpu 8 voices realtime factor > 20", rt > 20.0, {"realtime_x": rt, "render_ms": st.get("render_ms")})

    # 8. Each SPECIMEN catalogue entry loads and sounds.
    spec_only = CLEAN + ["osc_a_level=0", "specimen_level=1", "specimen_track=1", "specimen_density=30",
                         "specimen_size=150", "specimen_spray=0.4"]
    for choice in range(1, 19):
        wav, st = render(f"specimen_{choice:02d}", ["--notes", "60", "--seconds", "3", "--hold", "2.5"]
                         + sets(spec_only + [f"specimen_source={choice}"]), args.library)
        res = analyze.analyse(str(wav), A())
        ok = st.get("exit") == 0 and res["non_finite"] == 0 and res["rms_db"] > -40.0 and res["peak_db"] <= 0.0
        record(f"specimen {choice:02d}", ok, {"status": st.get("specimen"), "rms": res["rms_db"], "peak": res["peak_db"]})

    passed = sum(t["pass"] for t in report["tests"])
    report["summary"] = f"{passed}/{len(report['tests'])} passed"
    print(report["summary"])
    (ROOT / "build").mkdir(exist_ok=True)
    (ROOT / "build" / "test-report.json").write_text(json.dumps(report, indent=2))
    return 0 if passed == len(report["tests"]) else 1


if __name__ == "__main__":
    sys.exit(main())
