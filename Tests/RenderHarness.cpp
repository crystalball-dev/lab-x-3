// LabX3Render: drives the LAB X-3 processor offline.
//
//   LabX3Render --out <file.wav> [--preset <n|name>] [--notes 48,55,60] [--events "60@0-2,64@0.5-2"]
//               [--seconds 6] [--hold 4] [--velocity 0.8] [--sr 48000] [--block 512]
//               [--library <dir>] [--specimens <dir>] [--userfile <file>] [--set id=value ...]
//               [--program-at "4@2.5,7@5"] [--tail-from <seconds>]
//               [--png <file.png>] [--roundtrip] [--list] [--list-specimens]
//
//   LabX3Render --fuzz --seconds 300 [--seed 1] [--vary-blocks] [--out <file.wav>]
//       Random session: notes, presets, parameter moves, mod wheel, bend, sustain. Every 30 s
//       the last 10 s are silent (all notes off, no events). Reports any level that rises
//       during those silent stretches, non-finite samples and per-block processing time.
//
// Prints key=value statistics for the analysis scripts.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"
#include <ctime>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

namespace
{
    struct NoteEvent
    {
        int note;
        double start, end;
    };

    struct ProgramChange
    {
        int index;
        double time;
    };

    juce::String option (const juce::StringArray& args, const juce::String& key, const juce::String& fallback = {})
    {
        const int i = args.indexOf (key);
        return (i >= 0 && i + 1 < args.size()) ? args[i + 1] : fallback;
    }

    juce::StringArray allOptions (const juce::StringArray& args, const juce::String& key)
    {
        juce::StringArray out;
        for (int i = 0; i + 1 < args.size(); ++i)
            if (args[i] == key)
                out.add (args[i + 1]);
        return out;
    }

    int presetIndex (const juce::String& text)
    {
        const auto& presets = labx3::factoryPresets();
        if (text.containsOnly ("0123456789"))
            return juce::jlimit (0, (int) presets.size() - 1, text.getIntValue());
        for (int i = 0; i < (int) presets.size(); ++i)
            if (presets[(size_t) i].name.equalsIgnoreCase (text))
                return i;
        return -1;
    }

    bool setParam (LabX3AudioProcessor& proc, const juce::String& assignment)
    {
        const auto id = assignment.upToFirstOccurrenceOf ("=", false, false).trim();
        const auto value = assignment.fromFirstOccurrenceOf ("=", false, false).trim().getFloatValue();
        if (auto* p = proc.apvts.getParameter (id))
        {
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            return true;
        }
        std::cerr << "unknown parameter: " << id << std::endl;
        return false;
    }

    std::vector<float> paramSnapshot (LabX3AudioProcessor& proc)
    {
        std::vector<float> v;
        for (const auto& s : labx3::paramSpecs())
            v.push_back (proc.apvts.getRawParameterValue (s.id)->load());
        return v;
    }

    double ticksToMs (juce::int64 ticks)
    {
        return juce::Time::highResolutionTicksToSeconds (ticks) * 1000.0;
    }

    // CPU time used by the calling thread, in milliseconds. Unlike wall time it does not stretch
    // when other programs keep the machine busy, so the CPU benchmark stays meaningful.
    double threadCpuMs()
    {
       #if JUCE_WINDOWS
        FILETIME created, exited, kernel, user;
        if (GetThreadTimes (GetCurrentThread(), &created, &exited, &kernel, &user))
            return (double) ((((juce::uint64) kernel.dwHighDateTime << 32) | kernel.dwLowDateTime)
                             + (((juce::uint64) user.dwHighDateTime << 32) | user.dwLowDateTime)) / 10000.0;
       #endif
        return 1000.0 * (double) std::clock() / CLOCKS_PER_SEC;
    }

    struct BlockTimer
    {
        std::vector<double> ms;
        std::vector<double> budget;

        void add (double elapsedMs, double budgetMs) { ms.push_back (elapsedMs); budget.push_back (budgetMs); }

        juce::String summary() const
        {
            if (ms.empty())
                return {};
            auto sorted = ms;
            std::sort (sorted.begin(), sorted.end());
            const double p99 = sorted[(size_t) (0.999 * (double) (sorted.size() - 1))];
            int overruns = 0;
            for (size_t i = 0; i < ms.size(); ++i)
                if (ms[i] > 0.5 * budget[i])
                    ++overruns;
            return " block_max_ms=" + juce::String (sorted.back(), 3) + " block_p999_ms=" + juce::String (p99, 3)
                 + " half_budget_overruns=" + juce::String (overruns);
        }
    };

    bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        auto options = juce::AudioFormatWriterOptions().withSampleRate (sampleRate)
                                                       .withNumChannels (buffer.getNumChannels())
                                                       .withBitsPerSample (32)
                                                       .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        if (auto writer = wav.createWriterFor (stream, options))
            return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        return false;
    }

    //==============================================================================
    int runFuzz (LabX3AudioProcessor& proc, double sampleRate, int baseBlock, double seconds, int seed,
                 bool varyBlocks, const juce::File& outFile)
    {
        juce::Random rnd (seed);
        const auto& specs = labx3::paramSpecs();
        const int total = (int) (seconds * sampleRate);
        const double cycle = 30.0, activeSpan = 20.0;

        juce::AudioBuffer<float> block (2, 4096);
        juce::AudioBuffer<float> full;
        if (outFile != juce::File())
        {
            full.setSize (2, total);
            full.clear();
        }

        std::vector<int> held;
        int events = 0, presetChanges = 0, paramChanges = 0;
        long nonFinite = 0;
        float peak = 0.0f;
        BlockTimer timer;

        // Silence analysis: 100 ms RMS windows during each idle stretch.
        const int window = (int) (0.1 * sampleRate);
        double windowSum = 0.0;
        int windowCount = 0, windowIndex = 0, idleStretches = 0;
        bool inIdle = false, voicesWereActive = false;
        float minSoFar = 1000.0f;
        float worstGrowthFx = -1000.0f, worstGrowthVoices = -1000.0f;
        double worstFxTime = 0.0;
        int growthFx = 0, growthVoices = 0;
        double nextEvent = 0.0;

        int pos = 0;
        while (pos < total)
        {
            const int n = varyBlocks ? std::min (total - pos, 16 + rnd.nextInt (1009)) : std::min (baseBlock, total - pos);
            const double t0 = (double) pos / sampleRate;
            const bool idleNow = std::fmod (t0, cycle) >= activeSpan;
            juce::MidiBuffer midi;

            if (idleNow && ! inIdle)
            {
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
                midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                held.clear();
                inIdle = true;
                ++idleStretches;
                windowSum = 0.0;
                windowCount = 0;
                windowIndex = 0;
                minSoFar = 1000.0f;
            }
            else if (! idleNow && inIdle)
            {
                inIdle = false;
            }

            if (idleNow)
            {
                nextEvent = (std::floor (t0 / cycle) + 1.0) * cycle;
            }
            else
            {
                const double blockEnd = t0 + (double) n / sampleRate;
                while (nextEvent < blockEnd)
                {
                    const int offset = juce::jlimit (0, n - 1, (int) ((nextEvent - t0) * sampleRate));
                    const float r = rnd.nextFloat();
                    if (r < 0.40f && held.size() < 10)
                    {
                        const int note = 24 + rnd.nextInt (72);
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) (30 + rnd.nextInt (98))), offset);
                        held.push_back (note);
                    }
                    else if (r < 0.70f && ! held.empty())
                    {
                        const int k = rnd.nextInt ((int) held.size());
                        midi.addEvent (juce::MidiMessage::noteOff (1, held[(size_t) k]), offset);
                        held.erase (held.begin() + k);
                    }
                    else if (r < 0.75f)
                    {
                        proc.setCurrentProgram (rnd.nextInt (proc.getNumPrograms()));
                        ++presetChanges;
                    }
                    else if (r < 0.87f)
                    {
                        const auto& s = specs[(size_t) rnd.nextInt ((int) specs.size())];
                        if (auto* p = proc.apvts.getParameter (s.id))
                            p->setValueNotifyingHost (rnd.nextFloat());
                        ++paramChanges;
                    }
                    else if (r < 0.92f)
                        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, rnd.nextInt (128)), offset);
                    else if (r < 0.96f)
                        midi.addEvent (juce::MidiMessage::pitchWheel (1, rnd.nextInt (16384)), offset);
                    else if (r < 0.99f)
                        midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, rnd.nextBool() ? 127 : 0), offset);
                    else
                    {
                        midi.addEvent (juce::MidiMessage::allNotesOff (1), offset);
                        held.clear();
                    }
                    ++events;
                    nextEvent += 0.05 + rnd.nextDouble() * 0.55;
                }
            }

            juce::AudioBuffer<float> view (block.getArrayOfWritePointers(), 2, 0, n);
            const auto start = juce::Time::getHighResolutionTicks();
            proc.processBlock (view, midi);
            timer.add (ticksToMs (juce::Time::getHighResolutionTicks() - start), 1000.0 * (double) n / sampleRate);

            const bool voicesActive = proc.meters.activeVoices.load() > 0;
            const float* l = view.getReadPointer (0);
            const float* r = view.getReadPointer (1);
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (l[i]) || ! std::isfinite (r[i]))
                {
                    ++nonFinite;
                    continue;
                }
                peak = std::max (peak, std::max (std::abs (l[i]), std::abs (r[i])));

                if (inIdle)
                {
                    windowSum += (double) (l[i] * l[i] + r[i] * r[i]);
                    if (++windowCount >= window)
                    {
                        const float rmsDb = juce::Decibels::gainToDecibels ((float) std::sqrt (windowSum / (2.0 * windowCount)), -240.0f);
                        if (windowIndex >= 5 && rmsDb > -80.0f)
                        {
                            const float growth = rmsDb - minSoFar;
                            if (voicesWereActive)
                            {
                                worstGrowthVoices = std::max (worstGrowthVoices, growth);
                                if (growth > 6.0f)
                                    ++growthVoices;
                            }
                            else
                            {
                                if (growth > worstGrowthFx)
                                {
                                    worstGrowthFx = growth;
                                    worstFxTime = (double) (pos + i) / sampleRate;
                                }
                                if (growth > 6.0f)
                                    ++growthFx;
                            }
                        }
                        minSoFar = std::min (minSoFar, rmsDb);
                        voicesWereActive = voicesActive;
                        windowSum = 0.0;
                        windowCount = 0;
                        ++windowIndex;
                    }
                }
            }

            if (full.getNumSamples() > 0)
                for (int ch = 0; ch < 2; ++ch)
                    full.copyFrom (ch, pos, view, ch, 0, n);

            pos += n;
        }

        std::cout << "fuzz seed=" << seed << " seconds=" << seconds << " events=" << events
                  << " preset_changes=" << presetChanges << " param_changes=" << paramChanges
                  << " idle_stretches=" << idleStretches
                  << " growth_fx_only=" << growthFx << " worst_growth_fx_db=" << juce::String (worstGrowthFx, 1)
                  << " at_s=" << juce::String (worstFxTime, 2)
                  << " growth_with_voices=" << growthVoices << " worst_growth_voices_db=" << juce::String (worstGrowthVoices, 1)
                  << " non_finite=" << nonFinite
                  << " peak_db=" << juce::String (juce::Decibels::gainToDecibels (peak, -120.0f), 2)
                  << " faults=" << proc.meters.faults.load()
                  << timer.summary() << std::endl;

        if (full.getNumSamples() > 0 && ! writeWav (outFile, full, sampleRate))
            std::cerr << "could not write " << outFile.getFullPathName() << std::endl;

        return (nonFinite == 0 && growthFx == 0) ? 0 : 1;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String::fromUTF8 (argv[i]));

    if (args.contains ("--list"))
    {
        const auto& presets = labx3::factoryPresets();
        for (int i = 0; i < (int) presets.size(); ++i)
            std::cout << i << "\t" << presets[(size_t) i].name << std::endl;
        return 0;
    }

    if (args.contains ("--list-specimens"))
    {
        for (int i = 0; i < labx3::specimenCatalogSize; ++i)
            std::cout << i + 1 << "\t" << labx3::specimenCatalog[i].name << "\t" << labx3::specimenCatalog[i].group << std::endl;
        return 0;
    }

    const double sampleRate = option (args, "--sr", "48000").getDoubleValue();
    const int blockSize = option (args, "--block", "512").getIntValue();
    const double seconds = option (args, "--seconds", "6").getDoubleValue();
    const double hold = option (args, "--hold", "4").getDoubleValue();
    const float velocity = option (args, "--velocity", "0.8").getFloatValue();
    const juce::File outFile (option (args, "--out"));

    auto proc = std::make_unique<LabX3AudioProcessor>();

    if (args.contains ("--library"))
        proc->getSpecimenLibrary().setLibraryRoot (juce::File (option (args, "--library")), false);
    if (args.contains ("--specimens"))
        proc->getSpecimenLibrary().setSpecimensRoot (juce::File (option (args, "--specimens")), false);
    if (args.contains ("--userfile"))
        proc->getSpecimenLibrary().setUserFile (juce::File (option (args, "--userfile")));

    if (args.contains ("--preset"))
    {
        const int index = presetIndex (option (args, "--preset"));
        if (index < 0)
        {
            std::cerr << "unknown preset" << std::endl;
            return 2;
        }
        proc->setCurrentProgram (index);
    }

    for (const auto& assignment : allOptions (args, "--set"))
        if (! setParam (*proc, assignment))
            return 2;

    proc->setPlayConfigDetails (0, 2, sampleRate, args.contains ("--vary-blocks") ? 4096 : blockSize);
    proc->prepareToPlay (sampleRate, args.contains ("--vary-blocks") ? 4096 : blockSize);

    // One empty block issues the specimen request; then wait for the loader.
    {
        juce::AudioBuffer<float> warm (2, blockSize);
        juce::MidiBuffer none;
        proc->processBlock (warm, none);
        proc->getSpecimenLibrary().waitUntilSettled (10000);
        proc->prepareToPlay (sampleRate, args.contains ("--vary-blocks") ? 4096 : blockSize);
    }

    if (args.contains ("--fuzz"))
    {
        const int result = runFuzz (*proc, sampleRate, blockSize, seconds, option (args, "--seed", "1").getIntValue(),
                                    args.contains ("--vary-blocks"), outFile);
        proc->releaseResources();
        return result;
    }

    // Note events
    std::vector<NoteEvent> events;
    if (args.contains ("--events"))
    {
        for (const auto& token : juce::StringArray::fromTokens (option (args, "--events"), ",", ""))
        {
            const int note = token.upToFirstOccurrenceOf ("@", false, false).getIntValue();
            const auto times = token.fromFirstOccurrenceOf ("@", false, false);
            events.push_back ({ note, times.upToFirstOccurrenceOf ("-", false, false).getDoubleValue(),
                                times.fromFirstOccurrenceOf ("-", false, false).getDoubleValue() });
        }
    }
    else
    {
        for (const auto& token : juce::StringArray::fromTokens (option (args, "--notes", "60"), ",", ""))
            events.push_back ({ token.getIntValue(), 0.0, hold });
    }

    std::vector<ProgramChange> programChanges;
    for (const auto& token : juce::StringArray::fromTokens (option (args, "--program-at"), ",", ""))
        programChanges.push_back ({ token.upToFirstOccurrenceOf ("@", false, false).getIntValue(),
                                    token.fromFirstOccurrenceOf ("@", false, false).getDoubleValue() });

    const int total = (int) (seconds * sampleRate);
    juce::AudioBuffer<float> output (2, total);
    output.clear();
    BlockTimer timer;

    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    const auto cpu0 = threadCpuMs();
    for (int pos = 0; pos < total; pos += blockSize)
    {
        const int n = std::min (blockSize, total - pos);

        for (const auto& pc : programChanges)
        {
            const int at = (int) (pc.time * sampleRate);
            if (at >= pos && at < pos + n)
                proc->setCurrentProgram (pc.index);
        }

        juce::AudioBuffer<float> view (output.getArrayOfWritePointers(), 2, pos, n);
        juce::MidiBuffer midi;
        for (const auto& e : events)
        {
            const int on = (int) (e.start * sampleRate);
            const int off = (int) (e.end * sampleRate);
            if (on >= pos && on < pos + n)
                midi.addEvent (juce::MidiMessage::noteOn (1, e.note, velocity), on - pos);
            if (off >= pos && off < pos + n)
                midi.addEvent (juce::MidiMessage::noteOff (1, e.note), off - pos);
        }
        const auto start = juce::Time::getHighResolutionTicks();
        proc->processBlock (view, midi);
        timer.add (ticksToMs (juce::Time::getHighResolutionTicks() - start), 1000.0 * (double) n / sampleRate);
    }
    const double elapsedMs = juce::Time::getMillisecondCounterHiRes() - t0;
    const double cpuMs = threadCpuMs() - cpu0;

    // Statistics
    int nonFinite = 0;
    float peak = 0.0f, maxJump = 0.0f;
    double sumSquares = 0.0;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* d = output.getReadPointer (ch);
        for (int i = 0; i < total; ++i)
        {
            if (! std::isfinite (d[i]))
            {
                ++nonFinite;
                continue;
            }
            peak = std::max (peak, std::abs (d[i]));
            sumSquares += (double) d[i] * d[i];
            if (i > 0)
                maxJump = std::max (maxJump, std::abs (d[i] - d[i - 1]));
        }
    }
    const double rms = std::sqrt (sumSquares / std::max (1, total * 2));
    const double realtimeFactor = (seconds * 1000.0) / std::max (0.001, elapsedMs);
    // Thread CPU time advances in steps of about 16 ms on Windows, so short renders get no CPU figure.
    const bool cpuMeasurable = cpuMs >= 50.0;

    std::cout << "preset=\"" << proc->getProgramName (proc->getCurrentProgram()) << "\""
              << " specimen=\"" << proc->getSpecimenLibrary().getStatusText() << "\""
              << " seconds=" << seconds
              << " peak_db=" << juce::String (juce::Decibels::gainToDecibels (peak, -120.0f), 2)
              << " rms_db=" << juce::String (juce::Decibels::gainToDecibels ((float) rms, -120.0f), 2)
              << " max_jump=" << juce::String (maxJump, 4)
              << " non_finite=" << nonFinite
              << " render_ms=" << juce::String (elapsedMs, 1)
              << " realtime_x=" << juce::String (realtimeFactor, 1)
              << " cpu_ms=" << juce::String (cpuMs, 1)
              << (cpuMeasurable ? " cpu_realtime_x=" + juce::String ((seconds * 1000.0) / cpuMs, 1) : juce::String())
              << timer.summary();

    if (args.contains ("--tail-from"))
    {
        // Level contour after the given time: does anything rise once playing has stopped?
        const int from = juce::jlimit (0, total, (int) (option (args, "--tail-from").getDoubleValue() * sampleRate));
        const int window = (int) (0.1 * sampleRate);
        float tailPeak = 0.0f, minSoFar = 1000.0f, worstRise = -1000.0f;
        int clipped = 0, index = 0;
        for (int w = from; w + window <= total; w += window, ++index)
        {
            double sum = 0.0;
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* d = output.getReadPointer (ch);
                for (int i = w; i < w + window; ++i)
                {
                    sum += (double) d[i] * d[i];
                    tailPeak = std::max (tailPeak, std::abs (d[i]));
                    if (std::abs (d[i]) > 0.8f)
                        ++clipped;
                }
            }
            const float rmsDb = juce::Decibels::gainToDecibels ((float) std::sqrt (sum / (2.0 * window)), -240.0f);
            if (index >= 5 && rmsDb > -80.0f)
                worstRise = std::max (worstRise, rmsDb - minSoFar);
            minSoFar = std::min (minSoFar, rmsDb);
        }
        std::cout << " tail_peak_db=" << juce::String (juce::Decibels::gainToDecibels (tailPeak, -120.0f), 2)
                  << " tail_rise_db=" << juce::String (worstRise, 1)
                  << " tail_clipped=" << clipped;
    }
    std::cout << std::endl;

    if (outFile != juce::File() && ! writeWav (outFile, output, sampleRate))
    {
        std::cerr << "could not write " << outFile.getFullPathName() << std::endl;
        return 3;
    }

    if (args.contains ("--roundtrip"))
    {
        juce::MemoryBlock state;
        proc->getStateInformation (state);
        const auto before = paramSnapshot (*proc);

        auto other = std::make_unique<LabX3AudioProcessor>();
        other->setCurrentProgram ((proc->getCurrentProgram() + 5) % other->getNumPrograms());
        other->setStateInformation (state.getData(), (int) state.getSize());
        const auto after = paramSnapshot (*other);

        int mismatches = 0;
        for (size_t i = 0; i < before.size(); ++i)
            if (std::abs (before[i] - after[i]) > 1.0e-5f)
            {
                ++mismatches;
                std::cout << "roundtrip mismatch: " << labx3::paramSpecs()[i].id << " " << before[i] << " vs " << after[i] << std::endl;
            }
        std::cout << "roundtrip=" << (mismatches == 0 ? "PASS" : "FAIL") << " state_bytes=" << (int) state.getSize() << std::endl;
        if (mismatches != 0)
            return 4;
    }

    if (args.contains ("--png"))
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
        if (auto* labEditor = dynamic_cast<LabX3AudioProcessorEditor*> (editor.get()))
            for (int i = 0; i < 12; ++i)
                labEditor->refreshFromProcessor();

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const juce::File pngFile (option (args, "--png"));
        pngFile.deleteFile();
        juce::FileOutputStream stream (pngFile);
        juce::PNGImageFormat png;
        if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
        {
            std::cerr << "could not write " << pngFile.getFullPathName() << std::endl;
            return 5;
        }
        std::cout << "png=" << pngFile.getFullPathName() << " size=" << image.getWidth() << "x" << image.getHeight() << std::endl;
        editor.reset();
    }

    proc->releaseResources();
    return nonFinite == 0 ? 0 : 1;
}
