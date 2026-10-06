// LabX3Render: drives the LAB X-3 processor offline.
//
//   LabX3Render --out <file.wav> [--preset <n|name>] [--notes 48,55,60] [--events "60@0-2,64@0.5-2"]
//               [--seconds 6] [--hold 4] [--velocity 0.8] [--sr 48000] [--block 512]
//               [--library <dir>] [--userfile <file>] [--set id=value ...]
//               [--png <file.png>] [--roundtrip] [--list]
//
// Prints one line of key=value statistics for the analysis scripts.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

namespace
{
    struct NoteEvent
    {
        int note;
        double start, end;
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

    const double sampleRate = option (args, "--sr", "48000").getDoubleValue();
    const int blockSize = option (args, "--block", "512").getIntValue();
    const double seconds = option (args, "--seconds", "6").getDoubleValue();
    const double hold = option (args, "--hold", "4").getDoubleValue();
    const float velocity = option (args, "--velocity", "0.8").getFloatValue();
    const juce::File outFile (option (args, "--out"));

    auto proc = std::make_unique<LabX3AudioProcessor>();

    if (args.contains ("--library"))
        proc->getSpecimenLibrary().setLibraryRoot (juce::File (option (args, "--library")), false);
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

    proc->setPlayConfigDetails (0, 2, sampleRate, blockSize);
    proc->prepareToPlay (sampleRate, blockSize);

    // One empty block issues the specimen request; then wait for the loader.
    {
        juce::AudioBuffer<float> warm (2, blockSize);
        juce::MidiBuffer none;
        proc->processBlock (warm, none);
        proc->getSpecimenLibrary().waitUntilSettled (10000);
        proc->prepareToPlay (sampleRate, blockSize);
    }

    const int total = (int) (seconds * sampleRate);
    juce::AudioBuffer<float> output (2, total);
    output.clear();

    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < total; pos += blockSize)
    {
        const int n = std::min (blockSize, total - pos);
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
        proc->processBlock (view, midi);
    }
    const double elapsedMs = juce::Time::getMillisecondCounterHiRes() - t0;

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

    std::cout << "preset=\"" << proc->getProgramName (proc->getCurrentProgram()) << "\""
              << " specimen=\"" << proc->getSpecimenLibrary().getStatusText() << "\""
              << " seconds=" << seconds
              << " peak_db=" << juce::String (juce::Decibels::gainToDecibels (peak, -120.0f), 2)
              << " rms_db=" << juce::String (juce::Decibels::gainToDecibels ((float) rms, -120.0f), 2)
              << " max_jump=" << juce::String (maxJump, 4)
              << " non_finite=" << nonFinite
              << " render_ms=" << juce::String (elapsedMs, 1)
              << " realtime_x=" << juce::String (realtimeFactor, 1)
              << std::endl;

    if (outFile != juce::File())
    {
        outFile.getParentDirectory().createDirectory();
        outFile.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (outFile.createOutputStream());
        auto options = juce::AudioFormatWriterOptions().withSampleRate (sampleRate)
                                                       .withNumChannels (2)
                                                       .withBitsPerSample (32)
                                                       .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        if (auto writer = wav.createWriterFor (stream, options))
            writer->writeFromAudioSampleBuffer (output, 0, total);
        else
        {
            std::cerr << "could not write " << outFile.getFullPathName() << std::endl;
            return 3;
        }
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
