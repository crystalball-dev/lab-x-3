#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace labx3;

LabX3AudioProcessor::LabX3AudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LABX3", createParameterLayout())
{
    const auto& specs = paramSpecs();
    for (int i = 0; i < P::count; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (specs[(size_t) i].id);
        jassert (raw[(size_t) i] != nullptr);
        smoothers[(size_t) i].enabled = specs[(size_t) i].smooth != Smooth::none;
        smoothers[(size_t) i].logDomain = specs[(size_t) i].smooth == Smooth::log;
    }

    applyPreset (0);
    specimens.requestChoice ((int) std::lround (raw[P::specSource]->load()));
}

LabX3AudioProcessor::~LabX3AudioProcessor() = default;

//==============================================================================
void LabX3AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    const auto sr = (float) currentSampleRate;

    scratchSize = std::max (samplesPerBlock, 4096);
    scratchL.assign ((size_t) scratchSize, 0.0f);
    scratchR.assign ((size_t) scratchSize, 0.0f);

    for (int n = 0; n <= controlBlock; ++n)
        smoothCoef[(size_t) n] = 1.0f - std::exp (-(float) std::max (n, 1) / (0.015f * sr));

    for (int i = 0; i < P::count; ++i)
    {
        smoothers[(size_t) i].reset (raw[(size_t) i]->load());
        values[(size_t) i] = raw[(size_t) i]->load();
    }

    for (int v = 0; v < maxVoices; ++v)
        voices[(size_t) v].prepare (sr, (uint32_t) (v + 1) * 7919u);

    scrub.reset();
    noosphere.prepare (sr);
    noosphere.setParameters (raw[P::nooSize]->load(), raw[P::nooDecay]->load(), 0.3f, raw[P::nooMix]->load());

    monoCount = 0;
    sustainDown = false;
    bend = bendTarget = 0.0f;
    modWheel = modWheelTarget = 0.0f;
    masterGain = dsp::dbToGain (raw[P::volume]->load());
    dcInL = dcInR = dcOutL = dcOutR = 0.0f;
    clickAccumulator = clickSamples = 0;
    lastSpecimenChoice = -1;

    updateControl (controlBlock);
}

void LabX3AudioProcessor::releaseResources()
{
    for (auto& v : voices)
        v.kill();
}

bool LabX3AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet().isDisabled() || layouts.inputBuses.isEmpty();
}

double LabX3AudioProcessor::getTailLengthSeconds() const
{
    const float release = raw[P::env1R]->load() * 2.5f;
    const float rt60 = raw[P::nooMix]->load() > 0.0f ? 0.3f * std::exp2 (raw[P::nooDecay]->load() * 6.0f) : 0.0f;
    return (double) std::min (30.0f, std::max (release, rt60));
}

//==============================================================================
void LabX3AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    buffer.clear();

    const auto* specimen = specimens.acquireForBlock();

    const int choice = (int) std::lround (raw[P::specSource]->load());
    if (choice != lastSpecimenChoice)
    {
        specimens.requestChoice (choice);
        lastSpecimenChoice = choice;
    }

    // Voices beyond the current polyphony limit fade out.
    const int limit = maxActiveVoices();
    for (int v = limit; v < maxVoices; ++v)
        if (voices[(size_t) v].isActive() && voices[(size_t) v].isKeyDown())
            voices[(size_t) v].noteOff();

    if (numSamples == 0 || scratchSize == 0)
    {
        for (const auto metadata : midi)
            handleMidi (metadata.getMessage());
        return;
    }

    auto midiIt = midi.cbegin();
    const auto midiEnd = midi.cend();

    double sumSquares = 0.0;
    float peak = 0.0f;
    int processed = 0;

    while (processed < numSamples)
    {
        const int chunk = std::min (numSamples - processed, scratchSize);
        float* left = scratchL.data();
        float* right = scratchR.data();
        std::fill_n (left, chunk, 0.0f);
        std::fill_n (right, chunk, 0.0f);

        int pos = 0;
        while (pos < chunk)
        {
            while (midiIt != midiEnd && (*midiIt).samplePosition <= processed + pos)
            {
                handleMidi ((*midiIt).getMessage());
                ++midiIt;
            }

            int next = chunk;
            if (midiIt != midiEnd)
                next = std::min (next, (*midiIt).samplePosition - processed);

            const int n = std::min (controlBlock, std::max (1, next - pos));
            renderControlBlock (left + pos, right + pos, n, specimen);
            pos += n;
        }

        applyEffects (left, right, chunk);

        if (numChannels >= 2)
        {
            buffer.copyFrom (0, processed, left, chunk);
            buffer.copyFrom (1, processed, right, chunk);
        }
        else if (numChannels == 1)
        {
            buffer.copyFrom (0, processed, left, chunk, 0.5f);
            buffer.addFrom (0, processed, right, chunk, 0.5f);
        }

        // Metering and scope
        int w = scopeWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < chunk; ++i)
        {
            const float l = left[i], r = right[i];
            sumSquares += (double) (l * l + r * r) * 0.5;
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
            scope[(size_t) w] = 0.5f * (l + r);
            w = (w + 1) & (scopeSize - 1);
        }
        scopeWrite.store (w, std::memory_order_release);

        processed += chunk;
    }

    while (midiIt != midiEnd)
    {
        handleMidi ((*midiIt).getMessage());
        ++midiIt;
    }

    // Readouts
    int active = 0;
    for (auto& v : voices)
    {
        if (v.isActive())
            ++active;
        clickAccumulator += v.takeClicks();
    }
    clickSamples += numSamples;
    if (clickSamples >= (int) (currentSampleRate * 0.25))
    {
        meters.clicksPerSecond.store ((float) clickAccumulator * (float) currentSampleRate / (float) clickSamples);
        clickAccumulator = 0;
        clickSamples = 0;
    }

    const float rms = (float) std::sqrt (sumSquares / (double) numSamples);
    meters.rmsDb.store (juce::Decibels::gainToDecibels (rms, -100.0f));
    meters.peakDb.store (juce::Decibels::gainToDecibels (peak, -100.0f));
    meters.activeVoices.store (active);
    meters.dark.store (darkEffective);
}

void LabX3AudioProcessor::renderControlBlock (float* left, float* right, int n, const dsp::SpecimenData* specimen) noexcept
{
    updateControl (n);
    voiceParams.specimen = specimen;

    for (auto& v : voices)
        if (v.isActive())
            v.render (left, right, n, voiceParams);
}

void LabX3AudioProcessor::updateControl (int n) noexcept
{
    const float coef = smoothCoef[(size_t) std::clamp (n, 0, controlBlock)];

    for (int i = 0; i < P::count; ++i)
    {
        const float target = raw[(size_t) i]->load (std::memory_order_relaxed);
        values[(size_t) i] = smoothers[(size_t) i].enabled ? smoothers[(size_t) i].advance (target, coef) : target;
    }

    bend += (bendTarget - bend) * std::min (1.0f, coef * 4.0f);
    modWheel += (modWheelTarget - modWheel) * coef;

    const auto& v = values;
    const float dark = std::clamp (v[P::dark] + modWheel, 0.0f, 1.0f);
    darkEffective = dark;

    auto& p = voiceParams;
    p.oscAWave   = (int) std::lround (v[P::oscAWave]);
    p.oscAOctave = std::round (v[P::oscAOctave]);
    p.oscADetune = v[P::oscADetune];
    p.oscAShape  = v[P::oscAShape];
    p.oscALevel  = v[P::oscALevel];

    p.oscBWave  = (int) std::lround (v[P::oscBWave]);
    p.oscBRatio = v[P::oscBRatio];
    p.oscBFm    = v[P::oscBFm];
    p.oscBLevel = v[P::oscBLevel];

    p.subLevel   = v[P::subLevel];
    p.noiseLevel = std::min (1.0f, v[P::noiseLevel] + dark * 0.12f);
    p.noiseColor = v[P::noiseColor];

    p.geigerDensity = v[P::geigerDensity] + dark * dark * 10.0f;
    p.geigerTone    = v[P::geigerTone];

    p.specLevel    = v[P::specLevel];
    p.specPosition = v[P::specPosition];
    p.specSpray    = v[P::specSpray];
    p.specSize     = v[P::specSize] * 0.001f;
    p.specDensity  = v[P::specDensity];
    p.specTrack    = v[P::specTrack];

    p.filterType = (int) std::lround (v[P::filterType]);
    p.cutoff     = v[P::cutoff];
    p.resonance  = v[P::resonance];
    p.driveDb    = v[P::drive] + dark * 12.0f;
    p.filterEnv  = v[P::filterEnv];
    p.keytrack   = v[P::keytrack];

    p.a1 = v[P::env1A]; p.d1 = v[P::env1D]; p.s1 = v[P::env1S]; p.r1 = v[P::env1R];
    p.a2 = v[P::env2A]; p.d2 = v[P::env2D]; p.s2 = v[P::env2S]; p.r2 = v[P::env2R];

    p.progRate   = v[P::progRate];
    p.progDepth  = std::min (1.0f, v[P::progDepth] + dark * 0.35f);
    p.progTarget = (int) std::lround (v[P::progTarget]);

    p.whisperLevel    = v[P::whisperLevel];
    p.whisperFormant  = v[P::whisperFormant];
    p.whisperKeytrack = v[P::whisperKeytrack];

    p.presenceLevel = v[P::presenceLevel];
    p.presenceFreq  = v[P::presenceFreq];

    p.dark      = dark;
    p.glide     = v[P::glide];
    p.width     = v[P::width];
    p.pitchBend = bend;
}

void LabX3AudioProcessor::applyEffects (float* left, float* right, int n) noexcept
{
    scrub.process (left, right, n, raw[P::scrubBits]->load(), raw[P::scrubRate]->load());

    noosphere.setParameters (raw[P::nooSize]->load(), raw[P::nooDecay]->load(),
                             0.25f + darkEffective * 0.6f,
                             std::min (1.0f, raw[P::nooMix]->load() + darkEffective * 0.1f));
    noosphere.process (left, right, n);

    const float gainTarget = dsp::dbToGain (values[P::volume]);
    for (int i = 0; i < n; ++i)
    {
        masterGain += (gainTarget - masterGain) * 0.002f;
        const float l = left[i] * masterGain;
        const float r = right[i] * masterGain;

        const float dl = l - dcInL + 0.9995f * dcOutL;
        const float dr = r - dcInR + 0.9995f * dcOutR;
        dcInL = l; dcOutL = dl;
        dcInR = r; dcOutR = dr;

        left[i] = dsp::softClip (dl);
        right[i] = dsp::softClip (dr);
    }
}

//==============================================================================
void LabX3AudioProcessor::handleMidi (const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        bendTarget = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f * 2.0f;
    else if (m.isAllNotesOff())
        allNotesOff();
    else if (m.isAllSoundOff())
        allSoundOff();
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const int val = m.getControllerValue();
        if (cc == 1)
            modWheelTarget = (float) val / 127.0f;
        else if (cc == 64)
            setSustainPedal (val >= 64);
    }
}

int LabX3AudioProcessor::maxActiveVoices() const noexcept
{
    return juce::jlimit (1, maxVoices, (int) std::lround (raw[P::voices]->load()));
}

void LabX3AudioProcessor::noteOn (int note, float velocity) noexcept
{
    const int limit = maxActiveVoices();
    const bool glideOn = raw[P::glide]->load() > 0.0005f;

    if (limit == 1)
    {
        // Monophonic, last-note priority, legato when a key is already held.
        for (int i = 0; i < monoCount; ++i)
            if (monoStack[(size_t) i] == note)
            {
                for (int j = i; j < monoCount - 1; ++j)
                    monoStack[(size_t) j] = monoStack[(size_t) j + 1];
                --monoCount;
                break;
            }
        if (monoCount == (int) monoStack.size())
        {
            for (int j = 0; j < monoCount - 1; ++j)
                monoStack[(size_t) j] = monoStack[(size_t) j + 1];
            --monoCount;
        }
        monoStack[(size_t) monoCount++] = note;

        for (int v = 1; v < maxVoices; ++v)
            if (voices[(size_t) v].isActive())
                voices[(size_t) v].noteOff();

        auto& voice = voices[0];
        if (voice.isActive() && voice.isKeyDown())
            voice.legatoTo (note);
        else
            voice.noteOn (note, velocity, lastNotePitch, glideOn && anyNotePlayed, ++voiceClock);

        lastNotePitch = (float) note;
        anyNotePlayed = true;
        return;
    }

    dsp::Voice* target = nullptr;

    for (int v = 0; v < limit && target == nullptr; ++v)
        if (! voices[(size_t) v].isActive())
            target = &voices[(size_t) v];

    if (target == nullptr)
    {
        // Steal the quietest releasing voice, otherwise the oldest one.
        float quietest = 2.0f;
        for (int v = 0; v < limit; ++v)
        {
            auto& candidate = voices[(size_t) v];
            if (candidate.isReleasing() && candidate.getLevel() < quietest)
            {
                quietest = candidate.getLevel();
                target = &candidate;
            }
        }
    }

    if (target == nullptr)
    {
        uint64_t oldest = std::numeric_limits<uint64_t>::max();
        for (int v = 0; v < limit; ++v)
            if (voices[(size_t) v].getAge() < oldest)
            {
                oldest = voices[(size_t) v].getAge();
                target = &voices[(size_t) v];
            }
    }

    if (target != nullptr)
        target->noteOn (note, velocity, lastNotePitch, glideOn && anyNotePlayed, ++voiceClock);

    lastNotePitch = (float) note;
    anyNotePlayed = true;
}

void LabX3AudioProcessor::noteOff (int note) noexcept
{
    if (maxActiveVoices() == 1)
    {
        for (int i = 0; i < monoCount; ++i)
            if (monoStack[(size_t) i] == note)
            {
                for (int j = i; j < monoCount - 1; ++j)
                    monoStack[(size_t) j] = monoStack[(size_t) j + 1];
                --monoCount;
                break;
            }

        auto& voice = voices[0];
        if (voice.isActive() && voice.isKeyDown() && voice.getNote() == note)
        {
            if (monoCount > 0)
            {
                voice.legatoTo (monoStack[(size_t) monoCount - 1]);
                lastNotePitch = (float) monoStack[(size_t) monoCount - 1];
            }
            else if (sustainDown)
                voice.setSustained (true);
            else
                voice.noteOff();
        }
        return;
    }

    for (auto& v : voices)
        if (v.isActive() && v.isKeyDown() && v.getNote() == note)
        {
            if (sustainDown)
                v.setSustained (true);
            else
                v.noteOff();
        }
}

void LabX3AudioProcessor::setSustainPedal (bool down) noexcept
{
    sustainDown = down;
    if (! down)
        for (auto& v : voices)
            if (v.isSustained())
            {
                v.setSustained (false);
                v.noteOff();
            }
}

void LabX3AudioProcessor::allNotesOff() noexcept
{
    monoCount = 0;
    sustainDown = false;
    for (auto& v : voices)
    {
        v.setSustained (false);
        if (v.isActive())
            v.noteOff();
    }
}

void LabX3AudioProcessor::allSoundOff() noexcept
{
    monoCount = 0;
    sustainDown = false;
    for (auto& v : voices)
        v.kill();
    noosphere.reset();
}

//==============================================================================
void LabX3AudioProcessor::copyScope (float* dest, int numSamples) const noexcept
{
    numSamples = std::min (numSamples, scopeSize);
    int r = (scopeWrite.load (std::memory_order_acquire) - numSamples) & (scopeSize - 1);
    for (int i = 0; i < numSamples; ++i)
    {
        dest[i] = scope[(size_t) r];
        r = (r + 1) & (scopeSize - 1);
    }
}

//==============================================================================
int LabX3AudioProcessor::getNumPrograms()
{
    return (int) factoryPresets().size();
}

int LabX3AudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void LabX3AudioProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms()))
        applyPreset (index);
}

const juce::String LabX3AudioProcessor::getProgramName (int index)
{
    const auto& presets = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String();
}

void LabX3AudioProcessor::applyPreset (int index)
{
    const auto& presets = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    for (const auto& spec : paramSpecs())
        if (auto* param = apvts.getParameter (spec.id))
            param->setValueNotifyingHost (param->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
    {
        auto* param = apvts.getParameter (id);
        jassert (param != nullptr);
        if (param != nullptr)
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    currentProgram = index;
}

//==============================================================================
void LabX3AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("userFile", specimens.getUserFile().getFullPathName(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("version", versionString, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LabX3AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    const auto userPath = tree.getProperty ("userFile").toString();
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));

    apvts.replaceState (tree);
    specimens.setUserFile (juce::File::isAbsolutePath (userPath) ? juce::File (userPath) : juce::File());
}

//==============================================================================
juce::AudioProcessorEditor* LabX3AudioProcessor::createEditor()
{
    return new LabX3AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LabX3AudioProcessor();
}
