#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/CrtLookAndFeel.h"
#include "UI/Widgets.h"

class LabX3AudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LabX3AudioProcessorEditor (LabX3AudioProcessor&);
    ~LabX3AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Pulls meters, scope and status from the processor. Public so the render harness can call it.
    void refreshFromProcessor();

    static constexpr int editorWidth = 1220;
    static constexpr int editorHeight = 660;

private:
    void timerCallback() override { refreshFromProcessor(); }

    struct PanelItem
    {
        juce::Component* component;
        float units;
    };

    struct Panel
    {
        juce::String title;
        std::vector<PanelItem> items;
        juce::Rectangle<int> bounds;
        bool accent = false;
    };

    // SPECIMEN source selection, file buttons and loader status.
    class SpecimenControls final : public juce::Component
    {
    public:
        SpecimenControls (LabX3AudioProcessorEditor& owner, juce::AudioProcessorValueTreeState& state);
        void resized() override;

        labx3::ui::LabeledChoice source;
        juce::TextButton loadButton { "LOAD FILE" }, libraryButton { "LIBRARY" };
        juce::Label status;
    };

    labx3::ui::LabeledKnob* knob (const char* id, const char* caption, bool accent = false);
    labx3::ui::LabeledChoice* choice (const char* id, const char* caption);
    labx3::ui::LabeledToggle* toggle (const char* id, const char* caption, const char* onText, const char* offText);
    void layoutRow (std::vector<Panel>& row, juce::Rectangle<int> area, float unitWidth);
    void stepPreset (int delta);
    void chooseLibraryFolder();
    void chooseUserFile();

    LabX3AudioProcessor& audioProcessor;
    labx3::ui::CrtLookAndFeel lookAndFeel;

    std::vector<std::unique_ptr<labx3::ui::LabeledKnob>> knobs;
    std::vector<std::unique_ptr<labx3::ui::LabeledChoice>> choices;
    std::vector<std::unique_ptr<labx3::ui::LabeledToggle>> toggles;
    std::vector<std::vector<Panel>> rows;
    std::vector<Panel> sidePanels;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    std::unique_ptr<SpecimenControls> specimenControls;
    labx3::ui::RadiationMeter meter;
    labx3::ui::ScopeView scopeView;
    labx3::ui::LabeledKnob* darkKnob = nullptr;
    juce::Label systemLabel;
    labx3::ui::ScanlineOverlay overlay;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<float> scopeBuffer;
    int lastProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabX3AudioProcessorEditor)
};
