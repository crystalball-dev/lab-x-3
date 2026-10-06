# LAB X-3 design notes

## Sonic identity

The lore describes a buried Soviet black site, a localised psi radiation that manufactures hallucinations, and minds being overwritten until they break or obey. Each idea became a group of sounds, tuned against measurements of the reference recordings.

| Pillar | Reference recordings | Measured character | Synth feature |
|---|---|---|---|
| The Facility | underground background, transformer hum, cooling plant, radar | hums at 43 to 120 Hz with sparse partials, very quiet | sine and sub stack, SPECIMEN with keytracking off, slow PROGRAMMER |
| The Radiation | geiger clicks, static, blowout siren | broadband clicks near 3 kHz; static at 1 to 3 kHz; siren triad 350, 705, 1055 Hz | GEIGER generator, SCRUB, "Blowout Siren" preset on F4 |
| The Mind | psy voices, controller aura and presence, psi storm, psi drones | harmonic series on 127 Hz with nothing above 1 kHz; 3.1 and 6.2 kHz whine; inharmonic 560, 754, 883 Hz cluster; 48 Hz root under an inharmonic stack | WHISPER at 1:2:3, PRESENCE, OSC B ratio 2.76 FM, "Subtle" feedback FM |

Guiding rule: nothing holds still. The PROGRAMMER rewrites held notes, DARK pushes every stage toward instability, and loud drones sit against sparse transients, mirroring the 28 dB spread between the loudest and quietest reference recordings.

## Signal flow

Per voice, eight voices:

```
OSC B ──FM──> OSC A ─┐
SUB ─────────────────┤
NOISE ───────────────┤
WHISPER ─────────────┼─> drive ─> SVF filter ─┬─> amp envelope ─> pan
SPECIMEN grains ─────┘                        │
GEIGER clicks + PRESENCE whine ───────────────┘ (post-filter)
```

Global: voices summed ─> SCRUB ─> NOOSPHERE ─> master gain ─> DC blocker ─> soft clip.

## Modules

- **OSC A**: saw with a detuned partner (shape sets detune and blend), pulse (shape narrows the width), wavefolded sine, and "Subtle", a feedback-FM sine. PolyBLEP band-limiting on the saw and pulse.
- **OSC B**: sine, triangle, saw or square at a free ratio of the note. FM is phase modulation of OSC A.
- **SPECIMEN**: up to 24 Hann-windowed grains per voice with Hermite interpolation. Sources are decoded on a background thread, mixed to mono, capped at 60 seconds and normalised to -12 dBFS RMS. The audio thread receives buffers through an atomic pointer; retired buffers are freed only after two more audio blocks have started.
- **WHISPER**: white noise through three band-passes at Q 12, tuned to 1:2:3 of the formant base.
- **PRESENCE**: a sine plus its octave at 35 %, with 5.3 Hz vibrato and a slight per-voice detune.
- **GEIGER**: Poisson-triggered noise bursts with a 0.35 ms decay through a band-pass at the tone frequency.
- **FILTER**: topology-preserving state-variable filter, cutoff interpolated per sample, driven by a Padé tanh.
- **PROGRAMMER**: per-voice random walk that hops to new targets at roughly the rate and glides toward them.
- **NOOSPHERE**: 8 delay lines, Hadamard feedback matrix, one-pole damping and slow modulation of four lines.
- **DARK**: adds noise, drive, PROGRAMMER depth, Geiger density, grain spray, per-voice detune, lowers the cutoff by up to an octave and darkens the reverb.

## Verification

- `Tests/run_tests.py` renders through `LabX3Render` and checks pitch accuracy within 1 cent, Geiger onset rate within 30 %, all presets finite and below full scale, voice stealing, mono legato, state round trip, CPU headroom and every specimen source.
- `pluginval` at strictness 5 and 10.
- In-host checks in FL Studio 20.6: plugin scan, playback and a headless project render.
