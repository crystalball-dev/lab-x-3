"""Automated offline checks for LAB X-3.

Runs the LabX3Render harness through a battery of renders and verifies them with analyze.py.
Usage:  python Tests/run_tests.py [--library <stalker sounds folder>] [--specimens <STALKER SPECIMENS folder>]

Writes WAVs to Tests/out (ignored by git) and a JSON report to build/test-report.json.
"""

import argparse
import array
import json
import math
import pathlib
import re
import subprocess
import sys
import wave

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


def render(name, extra, libs=()):
    OUT.mkdir(parents=True, exist_ok=True)
    wav = OUT / f"{name}.wav"
    cmd = [str(HARNESS), "--out", str(wav)] + extra + list(libs)
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


def write_sine(path, freq, seconds=2.0, sr=48000):
    samples = array.array("h", (int(16000 * math.sin(2 * math.pi * freq * n / sr)) for n in range(int(seconds * sr))))
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(samples.tobytes())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--library")
    parser.add_argument("--specimens")
    args = parser.parse_args()
    libs = (["--library", args.library] if args.library else []) + (["--specimens", args.specimens] if args.specimens else [])

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
    wav, st = render("pitch_a4", ["--notes", "69", "--seconds", "3", "--hold", "2.5"] + sets(CLEAN), libs)
    opts = A(); opts.pitch = 440.0
    res = analyze.analyse(str(wav), opts)
    record("pitch A4 = 440 Hz", res["pass"], {"f0": res["dominant_hz"], "cents": res.get("pitch_error_cents")})

    # 2. Geiger density 10/s on a silent voice should produce ~10 clicks per second.
    wav, st = render("geiger_10", ["--notes", "60", "--seconds", "6", "--hold", "6", "--velocity", "1"]
                     + sets(CLEAN + ["osc_a_level=0", "geiger_density=10"]), libs)
    opts = A(); opts.onsets = 10.0
    res = analyze.analyse(str(wav), opts)
    record("geiger density 10/s", res["pass"], {"onsets_per_s": res.get("onsets_per_s")})

    # 3. Every factory preset: finite, below full scale, clearly audible.
    for i, name in enumerate(presets):
        mono = name in ("Dark's Programming", "Blowout Siren")
        notes = ["--events", "53@0-1.8,60@1.6-4"] if mono else ["--notes", "48,55,60"]
        wav, st = render(f"preset_{i:02d}", ["--preset", str(i), "--seconds", "7", "--hold", "4"] + notes, libs)
        res = analyze.analyse(str(wav), A())
        ok = st.get("exit") == 0 and res["non_finite"] == 0 and res["peak_db"] <= 0.0 and res["rms_db"] > -45.0
        record(f"preset {i:02d} {name}", ok, {"peak": res["peak_db"], "rms": res["rms_db"], "specimen": st.get("specimen"),
                                              "bands": res["bands_db"], "rt_x": st.get("realtime_x")})

    # 4. Voice stealing: 12 overlapping notes on 8 voices.
    events = ",".join(f"{36 + 3 * k}@{0.2 * k:.1f}-{3.0 + 0.1 * k:.1f}" for k in range(12))
    wav, st = render("stealing", ["--preset", "2", "--events", events, "--seconds", "6"], libs)
    res = analyze.analyse(str(wav), A())
    record("voice stealing 12 on 8", st.get("exit") == 0 and res["non_finite"] == 0 and res["peak_db"] <= 0.0,
           {"peak": res["peak_db"], "jump": res["max_jump"]})

    # 5. Mono legato with glide.
    wav, st = render("legato", ["--events", "48@0-1.2,55@1.0-2.2,60@2.0-3.0,53@2.8-4.0", "--seconds", "5"]
                     + sets(CLEAN + ["voices=1", "glide=0.2"]), libs)
    res = analyze.analyse(str(wav), A())
    record("mono legato glide", st.get("exit") == 0 and res["non_finite"] == 0 and res["max_jump"] < 0.2,
           {"jump": res["max_jump"], "peak": res["peak_db"]})

    # 6. State round trip.
    wav, st = render("roundtrip", ["--preset", "5", "--seconds", "1", "--roundtrip"], libs)
    record("state round trip", st.get("roundtrip") == "PASS", {"state_bytes": st.get("state_bytes")})

    # 7. CPU: 8 voices on the heaviest preset at 48 kHz, 512-sample blocks.
    wav, st = render("bench", ["--preset", "4", "--notes", "36,43,48,55,60,63,67,72", "--seconds", "10", "--hold", "8"],
                     libs)
    # Judged on the render thread's CPU time: wall time also counts whatever else the machine is doing.
    rt = float(st.get("cpu_realtime_x", 0))
    record("cpu 8 voices realtime factor > 20", rt > 20.0, {"cpu_realtime_x": rt, "cpu_ms": st.get("cpu_ms"),
                                                           "wall_realtime_x": st.get("realtime_x")})

    # 8. Each SPECIMEN catalogue entry loads and sounds: FL Studio's pack and the STALKER SPECIMENS library.
    catalog = [line.split("\t") for line in
               subprocess.run([str(HARNESS), "--list-specimens"], capture_output=True, text=True).stdout.splitlines()
               if "\t" in line]
    record("specimen catalogue listed", len(catalog) > 0, {"entries": len(catalog)})
    spec_only = CLEAN + ["osc_a_level=0", "specimen_level=1", "specimen_track=1", "specimen_density=30",
                         "specimen_size=150", "specimen_spray=0.4"]
    for choice, name, _group in catalog:
        choice = int(choice)
        wav, st = render(f"specimen_{choice:02d}", ["--notes", "60", "--seconds", "3", "--hold", "2.5"]
                         + sets(spec_only + [f"specimen_source={choice}"]), libs)
        res = analyze.analyse(str(wav), A())
        ok = st.get("exit") == 0 and res["non_finite"] == 0 and res["rms_db"] > -40.0 and res["peak_db"] <= 0.0
        record(f"specimen {choice:02d} {name}", ok, {"status": st.get("specimen"), "rms": res["rms_db"], "peak": res["peak_db"]})

    # 8b. TUNE moves the grains' pitch: a 440 Hz sine loaded as the user file, untuned, a fifth up and a
    #     fourth down. Grains start at random phases, which blurs a tone over the window's main lobe, so
    #     long grains over ten seconds keep the measured centre within a cent or two.
    sine = OUT / "tune_sine.wav"
    write_sine(sine, 440.0, seconds=4.0)
    for tune in (0, 7, -5):
        wav, st = render(f"specimen_tune_{tune}", ["--notes", "60", "--seconds", "10", "--hold", "10", "--userfile", str(sine)]
                         + sets(spec_only + ["specimen_source=0", "specimen_size=800", "specimen_density=6",
                                             "specimen_spray=0.3", f"specimen_tune={tune}"]), libs)
        opts = A(); opts.pitch = 440.0 * 2 ** (tune / 12)
        res = analyze.analyse(str(wav), opts)
        cents = res.get("centre_error_cents")
        record(f"specimen tune {tune:+d} st", st.get("exit") == 0 and cents is not None and abs(cents) < 5.0,
               {"centre_cents": cents, "status": st.get("specimen")})

    # 9. Browsing presets while a tail rings must not stretch or swell it.
    ev = "48@0-1.9,55@0-1.9,60@0-1.9,63@0-1.9,43@2-3.9,50@2-3.9,58@2-3.9,62@2-3.9"
    wav, st = render("preset_switch_tail", ["--preset", "3", "--events", ev, "--seconds", "14", "--tail-from", "4",
                                            "--program-at", "2@5,11@6.5,1@8"], libs)
    rise = float(st.get("tail_rise_db", "-1000"))
    record("preset switch during tail: no swell", st.get("exit") == 0 and rise <= 3.0, {"tail_rise_db": rise})

    # 10. Pan switch: off gives identical channels with a dry reverb; on gives a stereo spread.
    for pan, want_mono in (("0", True), ("1", False)):
        wav, st = render(f"pan_{pan}", ["--notes", "48,55,60,67", "--seconds", "3", "--hold", "2.5"]
                         + sets(CLEAN + ["osc_a_wave=0", "stereo_width=1", f"voice_pan={pan}"]), libs)
        res = analyze.analyse(str(wav), A())
        ok = (res["max_side"] < 1e-6) if want_mono else (res["max_side"] > 0.01)
        record(f"voice_pan={pan} {'mono' if want_mono else 'stereo'}", ok, {"max_side": res["max_side"]})

    # 11. Reverb gain staging: fully wet never builds up above dry, and stays consistent across settings.
    noise = CLEAN + ["osc_a_level=0", "noise_level=0.8", "noise_color=0.5"]
    dry_wav, _ = render("verb_dry", ["--notes", "60", "--seconds", "5", "--hold", "5"] + sets(noise + ["noo_mix=0"]), libs)
    dry = analyze.analyse(str(dry_wav), A())["rms_db"]
    diffs = []
    for size, decay in ((0, 1), (0, 0), (0.5, 0.5), (1, 1), (1, 0)):
        wav, st = render(f"verb_{size}_{decay}", ["--notes", "60", "--seconds", "5", "--hold", "5"]
                         + sets(noise + ["noo_mix=1", f"noo_size={size}", f"noo_decay={decay}"]), libs)
        diff = analyze.analyse(str(wav), A())["rms_db"] - dry
        diffs.append(diff)
        record(f"reverb size {size} decay {decay}: wet between -10 and +1 dB of dry", -10.0 <= diff <= 1.0,
               {"wet_minus_dry_db": round(diff, 1)})
    record("reverb wet level spread across settings <= 8 dB", max(diffs) - min(diffs) <= 8.0,
           {"spread_db": round(max(diffs) - min(diffs), 1)})

    # 12. Randomised sessions with silent stretches: nothing may rise in silence or go non-finite.
    for seed in (11, 12):
        cmd = [str(HARNESS), "--fuzz", "--seconds", "120", "--seed", str(seed), "--vary-blocks"] + libs
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
        stats = {k: (q if v.startswith('"') else v) for k, v, q in STAT.findall(proc.stdout)}
        ok = (proc.returncode == 0 and stats.get("non_finite") == "0"
              and stats.get("growth_fx_only") == "0" and stats.get("faults") == "0")
        record(f"fuzz seed {seed}", ok, {k: stats.get(k) for k in ("events", "preset_changes", "growth_fx_only",
                                                                    "worst_growth_fx_db", "non_finite", "faults",
                                                                    "block_max_ms")})

    passed = sum(t["pass"] for t in report["tests"])
    report["summary"] = f"{passed}/{len(report['tests'])} passed"
    print(report["summary"])
    (ROOT / "build").mkdir(exist_ok=True)
    (ROOT / "build" / "test-report.json").write_text(json.dumps(report, indent=2))
    return 0 if passed == len(report["tests"]) else 1


if __name__ == "__main__":
    sys.exit(main())
