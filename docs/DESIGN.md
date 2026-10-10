# LAB X-3 design notes

## Sonic identity

The lore describes a buried Soviet black site, a localised psi radiation that manufactures hallucinations, and minds being overwritten until they break or obey. Each idea became a group of sounds, tuned against measurements of the reference recordings.

| Pillar | Reference recordings | Measured character | Synth feature |
|---|---|---|---|
| The Facility | underground background, transformer hum, cooling plant, radar | hums at 43 to 120 Hz with sparse partials, very quiet | sine and sub stack, SPECIMEN with keytracking off, slow PROGRAMMER |
| The Radiation | geiger clicks, static, blowout siren | broadband clicks near 3 kHz; static at 1 to 3 kHz; siren triad 350, 705, 1055 Hz | GEIGER generator, SCRUB, "Blowout Siren" preset on F4 |
| The Mind | psy voices, controller aura and presence, psi storm, psi drones | harmonic series on 127 Hz with nothing above 1 kHz; 3.1 and 6.2 kHz whine; inharmonic 560, 754, 883 Hz cluster; 48 Hz root under an inharmonic stack | WHISPER at 1:2:3, PRESENCE, OSC B ratio 2.76 FM, "Subtle" feedback FM |

Guiding rule: nothing holds still. The PROGRAMMER rewrites held notes, DARK pushes every stage toward instability, and loud drones sit against sparse transients, mirroring the 28 dB spread between the loudest and quietest reference recordings.

### STALKER SPECIMENS presets (0.2.0)

Five presets use the extracted trilogy audio. Pitched sources were measured from isolated renders of the SPECIMEN layer and tuned with TUNE; levels were matched to the original twelve by K-weighted loudness (about -15.5 LUFS on a held C3, G3, C4 chord) rather than RMS, because the brighter sources read quiet on RMS.

| Preset | Source | Measured | Treatment |
|---|---|---|---|
| Yantar Dream | SoC intro, Yantar dream | 33 s, a partial near C4 (261 Hz) | keytrack 100 % so that partial follows the key; long, sparse grains |
| X-16 Emitter | SoC X-16 psi emitter | strong 118.7 Hz hum | TUNE +1.68 st puts it on C3, an octave under the played note |
| Emission Front | CS emission idle | rumble at 27 to 58 Hz under a 1.6 to 2 kHz wash | keytrack off; the PROGRAMMER walks grain position |
| Lab X-8 Lament | CoP Lab X-8 crying | a wail spread over 528 to 578 Hz, centred near 547 Hz | TUNE -0.75 st centres it on C5, an octave over the played note |
| Oasis Bloom | CoP Oasis noise | 3.2 s burst, bright only in its first fifth | grains held at 8 % with little spray; high-pass filter |

### S.T.A.L.K.E.R. 2 presets (0.3.0)

Sixteen S.T.A.L.K.E.R. 2 sounds joined the catalogue, chosen for steady energy across their length: lab and powered-lab ambiences, the X-19 alarm, the TV wall from the Strelok X3 cutscene, five anomalies, an emission, and four psy and controller sounds. Recordings that are mostly silence were left out. Three presets use them, matched the same way.

| Preset | Source | Measured | Treatment |
|---|---|---|---|
| Zenith Gate | Zenith anomaly loop | a drifting inharmonic cluster, strongest at 358.5 Hz near 30 % of the file | TUNE +1.55 st puts that partial a fifth over the played note; long grains; the PROGRAMMER drifts through the cluster. 1 dB under the others so six-note chords peak like the rest |
| Kaymanov's House | Kaymanov psy house loop | an inharmonic 160 to 330 Hz cluster under air centred near 4 kHz | keytrack 50 %; band-pass and whisper formants. A folded sine rather than feedback FM keeps peaks 3 dB lower at the same loudness |
| X3 Broadcast | TV wall, Strelok X3 cutscene | static over a hum series on 48.5 Hz | keytrack off so the hum stays put; dense 60 ms grains; SCRUB at 9 bits |

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
- **SPECIMEN**: up to 24 Hann-windowed grains per voice with Hermite interpolation. Grains play at 2^((keytrack × (note − 60) + tune) / 12) of the original rate, so C4 at full keytrack is the file's own pitch. Sources come from FL Studio's pack or the user's STALKER SPECIMENS library (`<library>/<game>/sounds/...`); catalogue entries are append-only because hosts save the choice index, and the SOURCE menu's sub-menus list them in that same order. Sources are decoded on a background thread, mixed to mono, capped at 60 seconds and normalised to -12 dBFS RMS. The audio thread receives buffers through an atomic pointer; retired buffers are freed only after two more audio blocks have started.
- **WHISPER**: white noise through three band-passes at Q 12, tuned to 1:2:3 of the formant base.
- **PRESENCE**: a sine plus its octave at 35 %, with 5.3 Hz vibrato and a slight per-voice detune.
- **GEIGER**: Poisson-triggered noise bursts with a 0.35 ms decay through a band-pass at the tone frequency.
- **FILTER**: topology-preserving state-variable filter, cutoff interpolated per sample, driven by a Padé tanh.
- **PROGRAMMER**: per-voice random walk that hops to new targets at roughly the rate and glides toward them.
- **NOOSPHERE**: 8 delay lines, Hadamard feedback matrix, one-pole damping and slow modulation of four lines. Input is high-passed at 80 Hz so sub-bass never accumulates, and trimmed gently with the loop gain so small rooms with long decays stay at or below the dry level. On a preset change the network is flushed and snaps to the new size instead of stretching the old tail. Delay reads split the delay into whole and fractional samples before wrapping, so both indices are integers in range. Up to 0.3.0 a float position was wrapped instead, and `write - delay` landing a hair below zero rounded up to the buffer length: one sample read past the end. At 48 kHz with SIZE at maximum, line 4 did that on every pass, about 290 times a minute, and fed whatever lay in that memory into the reverb. That was the "blowout" a few seconds after pausing.
- **Preset changes**: parameters are written once, straight to their final values; the output fades over 30 ms, voices and reverb are cleared, smoothers snap, then the output fades back in over 5 ms.
- **Safety**: after the voices, SCRUB and NOOSPHERE, the block's peak is checked; a non-finite sample or anything above +48 dBFS (one voice at full resonance stays near +30) silences the block, resets voices and effects, and increments the SYSTEM panel's fault counter. Once no key has been held for a second, a tail that climbs 12 dB over its level at key-up, above -40 dBFS, for 0.3 s is logged but left alone, since a sweeping filter can do that legitimately. Faults and rises go to `%APPDATA%\OPERATION FAIRWAY, LLC\LAB X-3 faults.log` with the transport state, time since the last note, preset and every parameter, written on the message thread. SPECIMEN source changes duck the grain layer until the new file is published, so grains are never cut mid-sound.
- **DARK**: adds noise, drive, PROGRAMMER depth, Geiger density, grain spray, per-voice detune, lowers the cutoff by up to an octave and darkens the reverb.

## Verification

- `Tests/run_tests.py` renders through `LabX3Render` and checks pitch accuracy within 1 cent, Geiger onset rate within 30 %, all presets finite and below full scale, voice stealing, mono legato, state round trip, CPU headroom, every specimen source in both libraries, TUNE moving a sine user file to within 5 cents of the target pitch, every preset's tail decaying after a song stops (64-sample blocks), the runaway guard on injected spikes and rising tails, and `LabX3Render --selftest-delay`, which compares the reverb's delay read with a double-precision reference at every boundary against a marker one past the buffer (the 0.3.0 read hits it; the current one never does).
- `pluginval` at strictness 5 and 10.
- In-host checks in FL Studio 20.6: plugin scan, playback and a headless project render.
