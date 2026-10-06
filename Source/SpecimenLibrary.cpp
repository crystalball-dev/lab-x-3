#include <algorithm>
#include <cmath>
#include "SpecimenLibrary.h"
#include "Config.h"

namespace labx3
{
    namespace
    {
        constexpr double maxSeconds = 60.0;     // longest stretch of a file that is decoded
        constexpr float targetRms = 0.25f;      // -12 dBFS after normalisation
        constexpr float peakCeiling = 2.0f;     // sources may exceed full scale; the voice drive stage tames them

        juce::PropertiesFile::Options settingsOptions()
        {
            juce::PropertiesFile::Options o;
            o.applicationName = pluginName;
            o.filenameSuffix = ".settings";
            o.folderName = vendorName;
            o.osxLibrarySubFolder = "Application Support";
            o.commonToAllUsers = false;
            return o;
        }

        bool looksLikePack (const juce::File& dir)
        {
            return dir.isDirectory() && dir.getChildFile (specimenCatalog[0].relativePath).existsAsFile();
        }

        // Many recordings swell from silence to a single loud moment. Grains can start anywhere,
        // so the slow loudness contour is flattened (a quarter of the original swing is kept)
        // while everything faster than ~50 ms, the texture itself, is left untouched.
        void flattenDynamics (float* d, int length, double sampleRate)
        {
            const int frame = std::max (64, (int) (sampleRate * 0.05));
            const int numFrames = (length + frame - 1) / frame;
            if (numFrames < 3)
                return;

            std::vector<float> env ((size_t) numFrames);
            for (int f = 0; f < numFrames; ++f)
            {
                const int start = f * frame;
                const int end = std::min (length, start + frame);
                double sum = 0.0;
                for (int i = start; i < end; ++i)
                    sum += (double) d[i] * d[i];
                env[(size_t) f] = (float) std::sqrt (sum / (double) std::max (1, end - start));
            }

            // Light smoothing so the gain contour has no steps.
            std::vector<float> smooth (env.size());
            for (int f = 0; f < numFrames; ++f)
            {
                const float prev = env[(size_t) std::max (0, f - 1)];
                const float next = env[(size_t) std::min (numFrames - 1, f + 1)];
                smooth[(size_t) f] = 0.25f * prev + 0.5f * env[(size_t) f] + 0.25f * next;
            }

            auto sorted = smooth;
            std::sort (sorted.begin(), sorted.end());
            const float reference = std::max (1.0e-5f, sorted[(size_t) (0.8f * (float) (numFrames - 1))]);

            std::vector<float> gains (smooth.size());
            for (size_t f = 0; f < smooth.size(); ++f)
                gains[f] = std::clamp (std::pow (reference / std::max (smooth[f], 1.0e-6f), 0.75f), 0.25f, 16.0f);

            for (int i = 0; i < length; ++i)
            {
                const float pos = ((float) i + 0.5f) / (float) frame - 0.5f;
                const int f0 = std::clamp ((int) std::floor (pos), 0, numFrames - 1);
                const int f1 = std::min (f0 + 1, numFrames - 1);
                const float t = std::clamp (pos - (float) f0, 0.0f, 1.0f);
                d[i] *= gains[(size_t) f0] + (gains[(size_t) f1] - gains[(size_t) f0]) * t;
            }
        }
    }

    SpecimenLibrary::SpecimenLibrary()
        : juce::Thread ("LAB X-3 specimen loader")
    {
        formats.registerBasicFormats();

        settings = std::make_unique<juce::PropertiesFile> (settingsOptions());
        juce::File saved (settings->getValue ("libraryRoot"));
        root = looksLikePack (saved) ? saved : autodetectRoot();

        startThread (juce::Thread::Priority::low);
    }

    SpecimenLibrary::~SpecimenLibrary()
    {
        stopThread (5000);
        active.store (nullptr);
        activeOwner.reset();
        retired.clear();
    }

    juce::File SpecimenLibrary::autodetectRoot()
    {
        juce::Array<juce::File> candidates;

        for (auto drive : { "C", "D", "E", "F", "G" })
            for (auto programFiles : { ":\\Program Files\\Image-Line", ":\\Program Files (x86)\\Image-Line" })
            {
                juce::File base (juce::String (drive) + programFiles);
                if (! base.isDirectory())
                    continue;
                for (const auto& install : base.findChildFiles (juce::File::findDirectories, false, "FL Studio*"))
                    candidates.add (install.getChildFile (specimenPackSubPath));
            }

        for (const auto& c : candidates)
            if (looksLikePack (c))
                return c;

        return {};
    }

    void SpecimenLibrary::requestChoice (int choice) noexcept
    {
        requested.store (choice);
    }

    const dsp::SpecimenData* SpecimenLibrary::acquireForBlock() noexcept
    {
        // Sequentially consistent on purpose: pairs with publish() so a retired buffer can
        // only be freed after the block that might still read it has finished.
        epoch.fetch_add (1);
        return active.load();
    }

    void SpecimenLibrary::setLibraryRoot (const juce::File& folder, bool persist)
    {
        {
            const juce::ScopedLock sl (lock);
            root = folder;
        }
        if (persist && settings != nullptr)
        {
            settings->setValue ("libraryRoot", folder.getFullPathName());
            settings->saveIfNeeded();
        }
        generation.fetch_add (1);
        notify();
    }

    juce::File SpecimenLibrary::getLibraryRoot() const
    {
        const juce::ScopedLock sl (lock);
        return root;
    }

    void SpecimenLibrary::setUserFile (const juce::File& file)
    {
        {
            const juce::ScopedLock sl (lock);
            if (file == userFile)
                return;
            userFile = file;
        }
        generation.fetch_add (1);
        notify();
    }

    juce::File SpecimenLibrary::getUserFile() const
    {
        const juce::ScopedLock sl (lock);
        return userFile;
    }

    juce::String SpecimenLibrary::getStatusText() const
    {
        const juce::ScopedLock sl (lock);
        return statusText;
    }

    bool SpecimenLibrary::waitUntilSettled (int timeoutMs) const
    {
        const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (juce::Time::getMillisecondCounter() < deadline)
        {
            if (handledChoice.load() == requested.load()
                 && handledGeneration.load() == generation.load()
                 && status.load() != Status::loading)
                return true;
            juce::Thread::sleep (10);
        }
        return false;
    }

    void SpecimenLibrary::setStatus (Status s, const juce::String& text)
    {
        {
            const juce::ScopedLock sl (lock);
            statusText = text;
        }
        status.store (s);
    }

    void SpecimenLibrary::run()
    {
        while (! threadShouldExit())
        {
            reclaim();

            const int choice = requested.load();
            const int gen = generation.load();

            if (choice >= 0 && (choice != handledChoice.load() || gen != handledGeneration.load()))
            {
                load (choice);
                handledGeneration.store (gen);
                handledChoice.store (choice);
                continue;
            }

            wait (40);
        }
    }

    void SpecimenLibrary::load (int choice)
    {
        juce::File file;
        juce::String name;
        {
            const juce::ScopedLock sl (lock);
            if (choice == specimenUserChoice)
            {
                file = userFile;
                name = userFile.getFileNameWithoutExtension();
            }
            else if (juce::isPositiveAndNotGreaterThan (choice, specimenCatalogSize))
            {
                const auto& entry = specimenCatalog[choice - 1];
                name = entry.name;
                if (root.isDirectory())
                    file = root.getChildFile (entry.relativePath);
            }
        }

        if (choice != specimenUserChoice && file == juce::File())
        {
            publish (nullptr);
            setStatus (Status::missingLibrary, "LIBRARY NOT FOUND / SET FOLDER");
            return;
        }

        if (choice == specimenUserChoice && file == juce::File())
        {
            publish (nullptr);
            setStatus (Status::missingFile, "NO USER FILE / LOAD ONE");
            return;
        }

        if (! file.existsAsFile())
        {
            publish (nullptr);
            setStatus (Status::missingFile, "FILE NOT FOUND: " + file.getFileName());
            return;
        }

        setStatus (Status::loading, "LOADING " + name.toUpperCase());

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        {
            publish (nullptr);
            setStatus (Status::failed, "UNREADABLE: " + file.getFileName());
            return;
        }

        const auto maxSamples = (juce::int64) (reader->sampleRate * maxSeconds);
        const int length = (int) std::min (reader->lengthInSamples, maxSamples);
        const int channels = juce::jlimit (1, 2, (int) reader->numChannels);

        juce::AudioBuffer<float> decoded (channels, length);
        decoded.clear();
        reader->read (decoded.getArrayOfWritePointers(), channels, 0, length);

        if (threadShouldExit())
            return;

        auto data = std::make_unique<dsp::SpecimenData>();
        data->storage.assign ((size_t) length + 2 * dsp::SpecimenData::guard, 0.0f);
        data->length = length;
        data->sampleRate = reader->sampleRate;

        float* d = data->storage.data() + dsp::SpecimenData::guard;
        for (int i = 0; i < length; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                s += decoded.getSample (ch, i);
            d[i] = s / (float) channels;
        }

        flattenDynamics (d, length, reader->sampleRate);

        double sumSquares = 0.0;
        float peak = 0.0f;
        for (int i = 0; i < length; ++i)
        {
            sumSquares += (double) d[i] * (double) d[i];
            peak = std::max (peak, std::abs (d[i]));
        }

        const float rms = (float) std::sqrt (sumSquares / (double) length);
        if (rms > 1.0e-6f && peak > 0.0f)
        {
            const float gain = std::min (targetRms / rms, peakCeiling / peak);
            for (int i = 0; i < length; ++i)
                d[i] *= gain;
        }

        publish (std::move (data));
        setStatus (Status::ready, name.toUpperCase() + " / " + juce::String ((double) length / reader->sampleRate, 1) + " S");
    }

    void SpecimenLibrary::publish (std::unique_ptr<dsp::SpecimenData> next)
    {
        active.exchange (next.get());
        const auto e = epoch.load();
        if (activeOwner != nullptr)
            retired.emplace_back (std::move (activeOwner), e);
        activeOwner = std::move (next);
    }

    void SpecimenLibrary::reclaim()
    {
        const auto now = epoch.load();
        retired.erase (std::remove_if (retired.begin(), retired.end(),
                                       [now] (const auto& r) { return now > r.second + 1; }),
                       retired.end());
    }
}
