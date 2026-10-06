# LAB X-3

A dark, unstable synthesizer for Windows, built as a VST® 3 instrument and a standalone app. It is inspired by Lab X-3, the "Oasis-3" hallucinogenics facility from the S.T.A.L.K.E.R. games, and is made by OPERATION FAIRWAY as a personal proof of concept.

LAB X-3 is a fan project. It is not affiliated with or endorsed by GSC Game World, and this repository contains no game audio.

![LAB X-3 interface](docs/labx3-ui.png)

## What it does

- **Two oscillators.** OSC A offers a saw with a detuned partner, a pulse, a wavefolded sine, and "Subtle", a feedback-FM sine. OSC B can be heard on its own or phase-modulate OSC A for inharmonic clusters.
- **Sub, noise and Geiger.** A sub-octave sine, colour-variable noise, and a Poisson click generator tuned like a dosimeter.
- **SPECIMEN.** A granular engine that streams sound files from your own disk, by default the `stalker sounds` pack that ships with older FL Studio installs. Grain size, density, spray, position and keytracking turn a recording into playable, unrecognisable texture.
- **WHISPER and PRESENCE.** Noise through three formant resonances at 1:2:3, modelled on the psy-voice recordings, and a high, wavering whine modelled on the controller.
- **Filter and envelopes.** A driven state-variable filter with low-, band- and high-pass modes, plus amp and filter envelopes.
- **PROGRAMMER.** A slow, irregular random walk that keeps rewriting pitch, filter, formant or grain position.
- **SCRUB, NOOSPHERE and DARK.** Bit and sample-rate reduction, an 8-line feedback-delay reverb, and a DARK macro that pushes everything toward instability. The mod wheel adds to DARK.
- **12 factory presets**, named after the lore.

## Sound sources

The SPECIMEN layer reads audio from a folder on your machine. Neither this repository nor its builds include any game audio: `Source/SpecimenCatalog.h` lists relative file paths only. Use **LIBRARY** to point the plugin at your own copy of the pack, or **LOAD FILE** to use any WAV, OGG, FLAC or AIFF file. Without the pack, every layer except SPECIMEN still works.

## Building on Windows

You need Visual Studio 2022 Build Tools with the C++ workload, CMake 3.22 or newer, and a checkout of JUCE 9.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DLABX3_JUCE_PATH=C:/path/to/JUCE
cmake --build build --config Release --target LabX3_VST3 LabX3_Standalone LabX3Render
```

`scripts/build.ps1` runs both steps.

## Installing in FL Studio

Close FL Studio, then copy the plugin file into the standard VST3 folder. `scripts/install.ps1` does this:

```
build/LabX3_artefacts/Release/VST3/LAB X-3.vst3/Contents/x86_64-win/LAB X-3.vst3
  -> C:\Program Files\Common Files\VST3\LAB X-3.vst3
```

Then scan for plugins in FL Studio. LAB X-3 appears under Generators.

## Testing

- `LabX3Render` renders presets offline to WAV and reports level, largest sample jump, non-finite samples and render speed.
- `python Tests/run_tests.py` runs the whole battery: pitch accuracy, Geiger click rate, every preset, voice stealing, legato, state round trip, CPU load and every specimen source.
- `scripts/validate.ps1` runs [pluginval](https://github.com/Tracktion/pluginval).

## Licence

LAB X-3 is free software under the GNU Affero General Public License v3.0; see `LICENSE`. It is built with [JUCE](https://juce.com) under the AGPLv3 and the Steinberg VST 3 SDK under the MIT licence.

VST is a registered trademark of Steinberg Media Technologies GmbH.
