#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "SpecimenCatalog.h"

namespace labx3
{
    enum class Kind { real, integer, choice };
    enum class Smooth { none, linear, log };

    struct ParamSpec
    {
        const char* id;
        const char* name;
        Kind kind;
        float minValue, maxValue, defaultValue, skewCentre;   // skewCentre 0 = linear range
        Smooth smooth;
        const char* unit;      // "%", "Hz", "s", "ms", "dB", "ct", "st", "x", "/s", "bit", "oct", ""
        const char* choices;   // '|' separated; "@specimen" builds the specimen list
    };

    namespace P
    {
        enum Index : int
        {
            oscAWave, oscAOctave, oscADetune, oscAShape, oscALevel,
            oscBWave, oscBRatio, oscBFm, oscBLevel,
            subLevel, noiseLevel, noiseColor,
            geigerDensity, geigerTone,
            specSource, specLevel, specPosition, specSpray, specSize, specDensity, specTrack,
            filterType, cutoff, resonance, drive, filterEnv, keytrack,
            env1A, env1D, env1S, env1R,
            env2A, env2D, env2S, env2R,
            progRate, progDepth, progTarget,
            whisperLevel, whisperFormant, whisperKeytrack,
            presenceLevel, presenceFreq,
            scrubBits, scrubRate,
            nooSize, nooDecay, nooMix,
            dark, volume, voices, glide, width, voicePan,
            specTune,   // 0.2.0; new parameters are appended so hosts keep their indices
            count
        };
    }

    inline const std::array<ParamSpec, P::count>& paramSpecs()
    {
        static const std::array<ParamSpec, P::count> specs {{
            { "osc_a_wave",        "OSC A Wave",          Kind::choice,  0.0f,   3.0f,     0.0f,    0.0f,  Smooth::none,   "",    "Saw|Square|Sine|Subtle" },
            { "osc_a_octave",      "OSC A Octave",        Kind::integer, -3.0f,  2.0f,     0.0f,    0.0f,  Smooth::none,   "oct", "" },
            { "osc_a_detune",      "OSC A Detune",        Kind::real,    -100.0f, 100.0f,  0.0f,    0.0f,  Smooth::linear, "ct",  "" },
            { "osc_a_shape",       "OSC A Shape",         Kind::real,    0.0f,   1.0f,     0.3f,    0.0f,  Smooth::linear, "%",   "" },
            { "osc_a_level",       "OSC A Level",         Kind::real,    0.0f,   1.0f,     0.8f,    0.0f,  Smooth::linear, "%",   "" },

            { "osc_b_wave",        "OSC B Wave",          Kind::choice,  0.0f,   3.0f,     0.0f,    0.0f,  Smooth::none,   "",    "Sine|Triangle|Saw|Square" },
            { "osc_b_ratio",       "OSC B Ratio",         Kind::real,    0.125f, 16.0f,    2.0f,    2.0f,  Smooth::log,    "x",   "" },
            { "osc_b_fm",          "OSC B FM",            Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,  Smooth::linear, "%",   "" },
            { "osc_b_level",       "OSC B Level",         Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,  Smooth::linear, "%",   "" },

            { "sub_level",         "Sub Level",           Kind::real,    0.0f,   1.0f,     0.3f,    0.0f,  Smooth::linear, "%",   "" },
            { "noise_level",       "Noise Level",         Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,  Smooth::linear, "%",   "" },
            { "noise_color",       "Noise Colour",        Kind::real,    0.0f,   1.0f,     0.5f,    0.0f,  Smooth::linear, "%",   "" },

            { "geiger_density",    "Geiger Density",      Kind::real,    0.0f,   60.0f,    0.0f,    8.0f,  Smooth::none,   "/s",  "" },
            { "geiger_tone",       "Geiger Tone",         Kind::real,    500.0f, 8000.0f,  3000.0f, 2500.0f, Smooth::log,  "Hz",  "" },

            { "specimen_source",   "Specimen Source",     Kind::choice,  0.0f,   (float) specimenCatalogSize, 1.0f, 0.0f, Smooth::none, "", "@specimen" },
            { "specimen_level",    "Specimen Level",      Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,  Smooth::linear, "%",   "" },
            { "specimen_position", "Specimen Position",   Kind::real,    0.0f,   1.0f,     0.3f,    0.0f,  Smooth::linear, "%",   "" },
            { "specimen_spray",    "Specimen Spray",      Kind::real,    0.0f,   1.0f,     0.2f,    0.0f,  Smooth::linear, "%",   "" },
            { "specimen_size",     "Specimen Grain Size", Kind::real,    10.0f,  800.0f,   120.0f,  100.0f, Smooth::none,  "ms",  "" },
            { "specimen_density",  "Specimen Density",    Kind::real,    1.0f,   120.0f,   24.0f,   20.0f, Smooth::none,   "/s",  "" },
            { "specimen_track",    "Specimen Keytrack",   Kind::real,    0.0f,   1.0f,     1.0f,    0.0f,  Smooth::linear, "%",   "" },

            { "filter_type",       "Filter Type",         Kind::choice,  0.0f,   2.0f,     0.0f,    0.0f,  Smooth::none,   "",    "Low-pass|Band-pass|High-pass" },
            { "filter_cutoff",     "Filter Cutoff",       Kind::real,    20.0f,  20000.0f, 1200.0f, 1000.0f, Smooth::log,  "Hz",  "" },
            { "filter_res",        "Filter Resonance",    Kind::real,    0.0f,   1.0f,     0.25f,   0.0f,  Smooth::linear, "%",   "" },
            { "filter_drive",      "Filter Drive",        Kind::real,    0.0f,   24.0f,    3.0f,    0.0f,  Smooth::linear, "dB",  "" },
            { "filter_env",        "Filter Env Amount",   Kind::real,    -1.0f,  1.0f,     0.3f,    0.0f,  Smooth::linear, "%",   "" },
            { "filter_keytrack",   "Filter Keytrack",     Kind::real,    0.0f,   1.0f,     0.5f,    0.0f,  Smooth::linear, "%",   "" },

            { "env1_attack",       "Amp Attack",          Kind::real,    0.001f, 10.0f,    0.01f,   0.5f,  Smooth::none,   "s",   "" },
            { "env1_decay",        "Amp Decay",           Kind::real,    0.001f, 10.0f,    0.3f,    0.8f,  Smooth::none,   "s",   "" },
            { "env1_sustain",      "Amp Sustain",         Kind::real,    0.0f,   1.0f,     0.8f,    0.0f,  Smooth::none,   "%",   "" },
            { "env1_release",      "Amp Release",         Kind::real,    0.001f, 20.0f,    0.6f,    1.5f,  Smooth::none,   "s",   "" },

            { "env2_attack",       "Filter Attack",       Kind::real,    0.001f, 10.0f,    0.01f,   0.5f,  Smooth::none,   "s",   "" },
            { "env2_decay",        "Filter Decay",        Kind::real,    0.001f, 10.0f,    0.6f,    0.8f,  Smooth::none,   "s",   "" },
            { "env2_sustain",      "Filter Sustain",      Kind::real,    0.0f,   1.0f,     0.3f,    0.0f,  Smooth::none,   "%",   "" },
            { "env2_release",      "Filter Release",      Kind::real,    0.001f, 20.0f,    0.8f,    1.5f,  Smooth::none,   "s",   "" },

            { "prog_rate",         "Programmer Rate",     Kind::real,    0.01f,  10.0f,    0.15f,   0.6f,  Smooth::none,   "Hz",  "" },
            { "prog_depth",        "Programmer Depth",    Kind::real,    0.0f,   1.0f,     0.2f,    0.0f,  Smooth::linear, "%",   "" },
            { "prog_target",       "Programmer Target",   Kind::choice,  0.0f,   4.0f,     4.0f,    0.0f,  Smooth::none,   "",    "Pitch|Filter|Formant|Specimen|All" },

            { "whisper_level",     "Whisper Level",       Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,  Smooth::linear, "%",   "" },
            { "whisper_formant",   "Whisper Formant",     Kind::real,    60.0f,  600.0f,   127.0f,  180.0f, Smooth::log,   "Hz",  "" },
            { "whisper_keytrack",  "Whisper Keytrack",    Kind::real,    0.0f,   1.0f,     0.5f,    0.0f,  Smooth::linear, "%",   "" },

            { "presence_level",    "Presence Level",      Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,  Smooth::linear, "%",   "" },
            { "presence_freq",     "Presence Frequency",  Kind::real,    1500.0f, 9000.0f, 3100.0f, 3500.0f, Smooth::log,  "Hz",  "" },

            { "scrub_bits",        "Scrub Bits",          Kind::real,    2.0f,   16.0f,    16.0f,   0.0f,  Smooth::none,   "bit", "" },
            { "scrub_rate",        "Scrub Downsample",    Kind::real,    1.0f,   32.0f,    1.0f,    6.0f,  Smooth::none,   "x",   "" },

            { "noo_size",          "Noosphere Size",      Kind::real,    0.0f,   1.0f,     0.5f,    0.0f,  Smooth::none,   "%",   "" },
            { "noo_decay",         "Noosphere Decay",     Kind::real,    0.0f,   1.0f,     0.5f,    0.0f,  Smooth::none,   "%",   "" },
            { "noo_mix",           "Noosphere Mix",       Kind::real,    0.0f,   1.0f,     0.25f,   0.0f,  Smooth::none,   "%",   "" },

            { "dark",              "DARK",                Kind::real,    0.0f,   1.0f,     0.2f,    0.0f,  Smooth::linear, "%",   "" },
            { "master_volume",     "Master Volume",       Kind::real,    -48.0f, 6.0f,     -9.0f,   0.0f,  Smooth::linear, "dB",  "" },
            { "voices",            "Voices",              Kind::integer, 1.0f,   8.0f,     8.0f,    0.0f,  Smooth::none,   "",    "" },
            { "glide",             "Glide",               Kind::real,    0.0f,   2.0f,     0.0f,    0.3f,  Smooth::none,   "s",   "" },
            { "stereo_width",      "Stereo Width",        Kind::real,    0.0f,   1.0f,     0.6f,    0.0f,  Smooth::linear, "%",   "" },
            { "voice_pan",         "Voice Panning",       Kind::choice,  0.0f,   1.0f,     1.0f,    0.0f,  Smooth::none,   "",    "Centre|Random" },
            { "specimen_tune",     "Specimen Tune",       Kind::real,    -24.0f, 24.0f,    0.0f,    0.0f,  Smooth::none,   "st",  "" },
        }};
        return specs;
    }

    inline int paramIndexForId (const juce::String& id)
    {
        const auto& specs = paramSpecs();
        for (int i = 0; i < P::count; ++i)
            if (id == specs[(size_t) i].id)
                return i;
        return -1;
    }

    inline juce::StringArray choicesFor (const ParamSpec& s)
    {
        if (juce::String (s.choices) == "@specimen")
        {
            juce::StringArray names { "User File" };
            for (const auto& e : specimenCatalog)
                names.add (e.name);
            return names;
        }
        return juce::StringArray::fromTokens (s.choices, "|", "");
    }

    inline int specimenChoiceForName (const juce::String& name)
    {
        for (int i = 0; i < specimenCatalogSize; ++i)
            if (name == specimenCatalog[i].name)
                return i + 1;
        jassertfalse;
        return 1;
    }

    inline juce::NormalisableRange<float> rangeFor (const ParamSpec& s)
    {
        juce::NormalisableRange<float> range (s.minValue, s.maxValue);
        if (s.kind != Kind::real)
            range.interval = 1.0f;
        else if (s.skewCentre > 0.0f)
            range.setSkewForCentre (s.skewCentre);
        return range;
    }

    inline juce::String formatValue (const ParamSpec& s, float v)
    {
        const juce::String unit (s.unit);
        if (unit == "%")   return juce::String (juce::roundToInt (v * 100.0f)) + "%";
        if (unit == "Hz")  return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                                               : juce::String (v, v < 10.0f ? 2 : (v < 100.0f ? 1 : 0)) + " Hz";
        if (unit == "s")   return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s";
        if (unit == "ms")  return juce::String (juce::roundToInt (v)) + " ms";
        if (unit == "dB")  return juce::String (v, 1) + " dB";
        if (unit == "ct")  return juce::String (juce::roundToInt (v)) + " ct";
        if (unit == "st")  return (v > 0.0f ? "+" : "") + juce::String (v, 2) + " st";
        if (unit == "x")   return juce::String (v, v < 10.0f ? 3 : 2) + "x";
        if (unit == "/s")  return juce::String (v, v < 10.0f ? 1 : 0) + "/s";
        if (unit == "bit") return juce::String (v, 1) + " bit";
        if (unit == "oct") return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v));
        return juce::String (juce::roundToInt (v));
    }

    inline float parseValue (const ParamSpec& s, const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        float v = t.retainCharacters ("0123456789.-+").getFloatValue();
        const juce::String unit (s.unit);
        if (unit == "%")                           v *= 0.01f;
        else if (unit == "Hz" && t.contains ("k")) v *= 1000.0f;
        else if (unit == "s" && t.contains ("ms")) v *= 0.001f;
        return juce::jlimit (s.minValue, s.maxValue, v);
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        for (const auto& s : paramSpecs())
        {
            jassert (s.id != nullptr);
            const juce::ParameterID pid { s.id, 1 };
            const auto* spec = &s;

            switch (s.kind)
            {
                case Kind::choice:
                    layout.add (std::make_unique<juce::AudioParameterChoice> (pid, s.name, choicesFor (s), (int) s.defaultValue));
                    break;

                case Kind::integer:
                    layout.add (std::make_unique<juce::AudioParameterInt> (
                        pid, s.name, (int) s.minValue, (int) s.maxValue, (int) s.defaultValue,
                        juce::AudioParameterIntAttributes()
                            .withStringFromValueFunction ([spec] (int v, int) { return formatValue (*spec, (float) v); })));
                    break;

                case Kind::real:
                    layout.add (std::make_unique<juce::AudioParameterFloat> (
                        pid, s.name, rangeFor (s), s.defaultValue,
                        juce::AudioParameterFloatAttributes()
                            .withStringFromValueFunction ([spec] (float v, int) { return formatValue (*spec, v); })
                            .withValueFromStringFunction ([spec] (const juce::String& text) { return parseValue (*spec, text); })));
                    break;
            }
        }

        return layout;
    }
}
