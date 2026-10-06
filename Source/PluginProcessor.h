#pragma once

#include <array>
#include <atomic>
#include <limits>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Config.h"
#include "Params.h"
#include "SpecimenLibrary.h"
#include "DSP/Noosphere.h"
#include "DSP/Scrub.h"
#include "DSP/Voice.h"

class LabX3AudioProcessor final : public juce::AudioProcessor
{
public:
    LabX3AudioProcessor();
    ~LabX3AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return labx3::pluginName; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    labx3::SpecimenLibrary& getSpecimenLibrary() noexcept { return specimens; }

    // Editor-facing readouts, written on the audio thread.
    struct Meters
    {
        std::atomic<float> rmsDb { -100.0f }, peakDb { -100.0f }, clicksPerSecond { 0.0f }, dark { 0.0f };
        std::atomic<int> activeVoices { 0 };
    };
    Meters meters;

    static constexpr int scopeSize = 2048;
    void copyScope (float* dest, int numSamples) const noexcept;

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Smoother
    {
        bool enabled = false, logDomain = false;
        float current = 0.0f;

        void reset (float v) noexcept { current = logDomain ? std::log2 (std::max (v, 1.0e-4f)) : v; }

        float advance (float target, float coef) noexcept
        {
            const float t = logDomain ? std::log2 (std::max (target, 1.0e-4f)) : target;
            current += (t - current) * coef;
            return logDomain ? std::exp2 (current) : current;
        }
    };

    void renderControlBlock (float* left, float* right, int n, const labx3::dsp::SpecimenData* specimen) noexcept;
    void updateControl (int n) noexcept;
    void applyEffects (float* left, float* right, int n) noexcept;

    void handleMidi (const juce::MidiMessage& m) noexcept;
    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void setSustainPedal (bool down) noexcept;
    void allNotesOff() noexcept;
    void allSoundOff() noexcept;
    int maxActiveVoices() const noexcept;

    void applyPreset (int index);

    labx3::SpecimenLibrary specimens;

    std::array<std::atomic<float>*, labx3::P::count> raw {};
    std::array<Smoother, labx3::P::count> smoothers {};
    std::array<float, labx3::P::count> values {};
    std::array<float, labx3::controlBlock + 1> smoothCoef {};

    std::array<labx3::dsp::Voice, labx3::maxVoices> voices;
    labx3::dsp::VoiceParams voiceParams;
    labx3::dsp::Scrub scrub;
    labx3::dsp::Noosphere noosphere;

    std::vector<float> scratchL, scratchR;
    int scratchSize = 0;
    double currentSampleRate = 48000.0;

    // MIDI state (audio thread)
    std::array<int, 16> monoStack {};
    int monoCount = 0;
    bool sustainDown = false;
    float bendTarget = 0.0f, bend = 0.0f, modWheelTarget = 0.0f, modWheel = 0.0f;
    float lastNotePitch = 60.0f;
    bool anyNotePlayed = false;
    uint64_t voiceClock = 0;
    int lastSpecimenChoice = -1;

    // Output stage
    float masterGain = 0.35f, dcInL = 0.0f, dcInR = 0.0f, dcOutL = 0.0f, dcOutR = 0.0f;
    float darkEffective = 0.0f;

    // Metering
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    int clickAccumulator = 0, clickSamples = 0;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabX3AudioProcessor)
};
