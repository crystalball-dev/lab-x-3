#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include "DSP/Granular.h"
#include "SpecimenCatalog.h"

namespace labx3
{
    // Loads SPECIMEN source files from the user's own disk on a background thread and hands
    // them to the audio thread without locks. Retired buffers are freed only after the audio
    // thread has started two more blocks, so a buffer is never freed while it is being read.
    class SpecimenLibrary : private juce::Thread
    {
    public:
        enum class Status { idle, loading, ready, missingLibrary, missingFile, failed };

        SpecimenLibrary();
        ~SpecimenLibrary() override;

        // Any thread, lock-free. choice is a value of the specimen_source parameter.
        void requestChoice (int choice) noexcept;

        // Audio thread: call once at the start of every block and use the result for that block.
        const dsp::SpecimenData* acquireForBlock() noexcept;

        // Message thread.
        void setLibraryRoot (const juce::File& folder, bool persist);
        juce::File getLibraryRoot() const;
        void setUserFile (const juce::File& file);
        juce::File getUserFile() const;

        Status getStatus() const noexcept { return status.load(); }
        juce::String getStatusText() const;

        // Blocks until the latest request has been handled. Used by the render harness.
        bool waitUntilSettled (int timeoutMs) const;

        static juce::File autodetectRoot();

    private:
        void run() override;
        void load (int choice);
        void publish (std::unique_ptr<dsp::SpecimenData> next);
        void reclaim();
        void setStatus (Status s, const juce::String& text);

        juce::AudioFormatManager formats;
        std::unique_ptr<juce::PropertiesFile> settings;

        mutable juce::CriticalSection lock;   // guards root, userFile, statusText
        juce::File root, userFile;
        juce::String statusText { "NO SPECIMEN" };

        std::atomic<int> requested { -1 }, generation { 0 };
        std::atomic<int> handledChoice { -2 }, handledGeneration { -1 };
        std::atomic<Status> status { Status::idle };

        std::atomic<dsp::SpecimenData*> active { nullptr };
        std::atomic<uint64_t> epoch { 0 };
        std::unique_ptr<dsp::SpecimenData> activeOwner;                                   // loader thread
        std::vector<std::pair<std::unique_ptr<dsp::SpecimenData>, uint64_t>> retired;      // loader thread

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpecimenLibrary)
    };
}
