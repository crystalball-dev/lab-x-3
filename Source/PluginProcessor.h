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

class LabX3AudioProcessor final : public juce::AudioProcessor,
                                   private juce::Timer
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
        std::atomic<float> midiPerSecond { 0.0f };
        std::atomic<int> activeVoices { 0 }, faults { 0 };
    };
    Meters meters;

    static constexpr int scopeSize = 2048;
    void copyScope (float* dest, int numSamples) const noexcept;

    // Runaway guard log: one line per fault, with the parameters at the time. Written on the message
    // thread to <user app data>\OPERATION FAIRWAY, LLC\LAB X-3 faults.log.
    enum Stage { stageVoices, stageScrub, stageNoosphere, stageOutput, numStages };
    static const char* stageName (int stage) noexcept;
    juce::File getFaultLogFile() const;
    void setFaultLogFile (const juce::File& file) { faultLogOverride = file; }
    int flushFaultLog();   // returns the number of entries written

    juce::AudioProcessorValueTreeState apvts;

   #if LABX3_HARNESS
    // Test hooks: from this sample on, the stage's output gets a 1e6 spike (mode 0) or a tone that grows
    // 6 dB every 100 ms (mode 1, stopped by the first fault it causes).
    int injectStage = -1, injectMode = 0;
    int64_t injectAtSample = -1;
   #endif

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
    void flushForProgramChange() noexcept;
    void recoverFromFault() noexcept;

    bool guardStage (int stage, float* left, float* right, int n) noexcept;
    void watchTail (const float* left, const float* right, int n) noexcept;
    void noteFault (int stage, int kind, float level, float rise) noexcept;
    void timerCallback() override { flushFaultLog(); }
   #if LABX3_HARNESS
    void injectTestSignal (int stage, float* left, float* right, int n) noexcept;
   #endif

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
    std::array<float, labx3::controlBlock + 1> smoothCoef {}, duckCoef {};

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

    // SPECIMEN source changes: the grain layer ducks out until the new file has been published.
    bool specimenAwait = false;
    uint64_t specimenAwaitId = 0;
    float specimenDuck = 1.0f;

    // Preset changes: fade out, clear voices and reverb, fade back in.
    enum class SwitchState { idle, fadingOut, fadingIn };
    std::atomic<bool> programChangePending { false };
    SwitchState switchState = SwitchState::idle;
    int switchFadeRemaining = 0, switchFadeLength = 1440;
    float switchGain = 1.0f, switchFadeInStep = 0.005f;

    // Output stage
    float masterGain = 0.35f, dcInL = 0.0f, dcInR = 0.0f, dcOutL = 0.0f, dcOutR = 0.0f;
    float darkEffective = 0.0f;

    // Metering
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    int clickAccumulator = 0, clickSamples = 0, midiAccumulator = 0;

    int currentProgram = 0;

    // Runaway guard. No stage reaches +48 dBFS legitimately (one voice at full resonance stays near +30):
    // above that, or non-finite, the block is silenced and everything restarts. Once no key has been held
    // for a second, a tail should only fade; one that climbs 12 dB over its level when the keys went up
    // (above -40 dBFS, for 0.3 s) is logged but left alone, since a sweeping filter can do that legitimately.
    static constexpr float stageLimit = 256.0f;
    static constexpr float riseLimitDb = 12.0f, riseFloorDb = -40.0f;
    enum FaultKind { faultRunaway, faultNonFinite, riseNotice };
    struct FaultEntry
    {
        int stage = 0, kind = 0, activeVoices = 0, heldKeys = 0;
        float level = 0.0f, rise = 0.0f;
        double seconds = 0.0, sinceNote = 0.0, bpm = 0.0;
        bool playing = false;
    };
    std::array<FaultEntry, 64> faultRing {};
    std::atomic<int> faultWrite { 0 };
    int faultRead = 0;
    bool faultLogHeaderWritten = false;
    juce::File faultLogOverride;
    int64_t samplesRendered = 0, lastNoteEventSample = 0, lastFaultSample = std::numeric_limits<int64_t>::min() / 2;
    int preparedBlockSize = 0;
    bool hostPlaying = false;
    double hostBpm = 0.0;

    // Tail watch: 100 ms RMS windows after the reverb, before the master volume.
    bool keysHeld = false;
    double tailWindowSum = 0.0;
    int tailWindowCount = 0, keysUpWindows = 0, riseWindows = 0;
    std::array<float, 3> recentDb { -240.0f, -240.0f, -240.0f };
    float releaseDb = -240.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabX3AudioProcessor)
};
