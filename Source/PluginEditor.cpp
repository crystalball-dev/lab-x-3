#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace labx3;
using namespace labx3::ui;

namespace
{
    constexpr int headerHeight = 58;
    constexpr int footerHeight = 24;
    constexpr int margin = 10;
    constexpr int sideWidth = 252;
    constexpr int gap = 8;
    constexpr int panelPad = 8;
    constexpr int titleHeight = 18;
}

//==============================================================================
LabX3AudioProcessorEditor::SpecimenControls::SpecimenControls (LabX3AudioProcessorEditor& owner,
                                                               juce::AudioProcessorValueTreeState& state)
    : source (state, "specimen_source", "SOURCE")
{
    addAndMakeVisible (source);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (libraryButton);
    addAndMakeVisible (status);

    loadButton.setTooltip ("Load your own audio file as the specimen");
    libraryButton.setTooltip ("Choose the 'stalker sounds' folder that the catalogue entries are read from");
    loadButton.onClick = [&owner] { owner.chooseUserFile(); };
    libraryButton.onClick = [&owner] { owner.chooseLibraryFolder(); };

    status.setFont (monoFont (10.0f));
    status.setJustificationType (juce::Justification::topLeft);
    status.setMinimumHorizontalScale (0.7f);
}

void LabX3AudioProcessorEditor::SpecimenControls::resized()
{
    auto b = getLocalBounds();
    source.setBounds (b.removeFromTop (37));
    b.removeFromTop (4);
    auto buttons = b.removeFromTop (20);
    loadButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 2));
    buttons.removeFromLeft (4);
    libraryButton.setBounds (buttons);
    b.removeFromTop (3);
    status.setBounds (b);
}

//==============================================================================
LabX3AudioProcessorEditor::LabX3AudioProcessorEditor (LabX3AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);
    auto& state = audioProcessor.apvts;

    specimenControls = std::make_unique<SpecimenControls> (*this, state);
    addAndMakeVisible (*specimenControls);

    // Row 1: sources
    rows.push_back ({
        { utf8 ("OSC A  //  ГЕНЕРАТОР"), { { choice ("osc_a_wave", "WAVE"), 1.5f }, { knob ("osc_a_octave", "OCTAVE"), 1.0f },
                                    { knob ("osc_a_detune", "DETUNE"), 1.0f }, { knob ("osc_a_shape", "SHAPE"), 1.0f },
                                    { knob ("osc_a_level", "LEVEL"), 1.0f } } },
        { "OSC B  //  MODULATOR", { { choice ("osc_b_wave", "WAVE"), 1.5f }, { knob ("osc_b_ratio", "RATIO"), 1.0f },
                                    { knob ("osc_b_fm", "FM > A"), 1.0f }, { knob ("osc_b_level", "LEVEL"), 1.0f } } },
        { "SUB  +  NOISE",        { { knob ("sub_level", "SUB"), 1.0f }, { knob ("noise_level", "NOISE"), 1.0f },
                                    { knob ("noise_color", "COLOUR"), 1.0f } } },
        { "GEIGER",               { { knob ("geiger_density", "DENSITY"), 1.0f }, { knob ("geiger_tone", "TONE"), 1.0f } } },
    });

    // Row 2: the lab's own sounds
    rows.push_back ({
        { utf8 ("SPECIMEN  //  ОБРАЗЕЦ"), { { specimenControls.get(), 2.7f }, { knob ("specimen_level", "LEVEL"), 1.0f },
                                     { knob ("specimen_position", "POSITION"), 1.0f }, { knob ("specimen_spray", "SPRAY"), 1.0f },
                                     { knob ("specimen_size", "GRAIN"), 1.0f }, { knob ("specimen_density", "DENSITY"), 1.0f },
                                     { knob ("specimen_track", "KEYTRACK"), 1.0f } } },
        { "WHISPER",               { { knob ("whisper_level", "LEVEL"), 1.0f }, { knob ("whisper_formant", "FORMANT"), 1.0f },
                                     { knob ("whisper_keytrack", "KEYTRACK"), 1.0f } } },
        { "PRESENCE",              { { knob ("presence_level", "LEVEL"), 1.0f }, { knob ("presence_freq", "FREQ"), 1.0f } } },
    });

    // Row 3: filter and envelopes
    rows.push_back ({
        { "FILTER",           { { choice ("filter_type", "TYPE"), 1.5f }, { knob ("filter_cutoff", "CUTOFF"), 1.0f },
                                { knob ("filter_res", "RESO"), 1.0f }, { knob ("filter_drive", "DRIVE"), 1.0f },
                                { knob ("filter_env", "ENV"), 1.0f }, { knob ("filter_keytrack", "KEYTRACK"), 1.0f } } },
        { "AMP ENVELOPE",     { { knob ("env1_attack", "A"), 1.0f }, { knob ("env1_decay", "D"), 1.0f },
                                { knob ("env1_sustain", "S"), 1.0f }, { knob ("env1_release", "R"), 1.0f } } },
        { "FILTER ENVELOPE",  { { knob ("env2_attack", "A"), 1.0f }, { knob ("env2_decay", "D"), 1.0f },
                                { knob ("env2_sustain", "S"), 1.0f }, { knob ("env2_release", "R"), 1.0f } } },
    });

    // Row 4: modulation, damage, space, master
    rows.push_back ({
        { "PROGRAMMER",  { { knob ("prog_rate", "RATE"), 1.0f }, { knob ("prog_depth", "DEPTH"), 1.0f },
                           { choice ("prog_target", "TARGET"), 1.5f } } },
        { "SCRUB",       { { knob ("scrub_bits", "BITS"), 1.0f }, { knob ("scrub_rate", "HOLD"), 1.0f } } },
        { "NOOSPHERE",   { { knob ("noo_size", "SIZE"), 1.0f }, { knob ("noo_decay", "DECAY"), 1.0f },
                           { knob ("noo_mix", "MIX"), 1.0f } } },
        { "MASTER",      { { knob ("voices", "VOICES"), 1.0f }, { knob ("glide", "GLIDE"), 1.0f },
                           { toggle ("voice_pan", "PAN", "RANDOM", "CENTRE"), 1.2f },
                           { knob ("stereo_width", "WIDTH"), 1.0f }, { knob ("master_volume", "VOLUME"), 1.0f } } },
    });

    darkKnob = knob ("dark", "DARK", true);

    sidePanels.push_back ({ "DOSIMETER", { { &meter, 1.0f } } });
    sidePanels.push_back ({ "MONITOR", { { &scopeView, 1.0f } } });
    sidePanels.push_back ({ utf8 ("DARK  //  ТЬМА"), { { darkKnob, 1.0f } }, {}, true });
    sidePanels.push_back ({ "SYSTEM", { { &systemLabel, 1.0f } } });

    addAndMakeVisible (meter);
    addAndMakeVisible (scopeView);
    systemLabel.setFont (monoFont (11.0f));
    systemLabel.setJustificationType (juce::Justification::topLeft);
    systemLabel.setColour (juce::Label::textColourId, Palette::phosphor);
    addAndMakeVisible (systemLabel);

    // Preset browser
    const auto& presets = factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (juce::String (i + 1).paddedLeft ('0', 2) + "  " + presets[(size_t) i].name, i + 1);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0 && index != audioProcessor.getCurrentProgram())
            audioProcessor.setCurrentProgram (index);
        lastProgram = audioProcessor.getCurrentProgram();
    };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    addAndMakeVisible (presetBox);
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    addAndMakeVisible (overlay);

    scopeBuffer.assign ((size_t) LabX3AudioProcessor::scopeSize, 0.0f);

    // Controls were built before they had a parent, so they captured the default theme's colours.
    // Re-broadcasting makes every slider rebuild its text box through the CRT look-and-feel.
    sendLookAndFeelChange();

    setSize (editorWidth, editorHeight);
    refreshFromProcessor();
    startTimerHz (30);
}

LabX3AudioProcessorEditor::~LabX3AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

LabeledKnob* LabX3AudioProcessorEditor::knob (const char* id, const char* caption, bool accent)
{
    knobs.push_back (std::make_unique<LabeledKnob> (audioProcessor.apvts, id, caption, accent));
    addAndMakeVisible (*knobs.back());
    return knobs.back().get();
}

LabeledToggle* LabX3AudioProcessorEditor::toggle (const char* id, const char* caption, const char* onText, const char* offText)
{
    toggles.push_back (std::make_unique<LabeledToggle> (audioProcessor.apvts, id, caption, onText, offText));
    addAndMakeVisible (*toggles.back());
    return toggles.back().get();
}

LabeledChoice* LabX3AudioProcessorEditor::choice (const char* id, const char* caption)
{
    choices.push_back (std::make_unique<LabeledChoice> (audioProcessor.apvts, id, caption));
    addAndMakeVisible (*choices.back());
    return choices.back().get();
}

//==============================================================================
void LabX3AudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    g.fillAll (Palette::background);

    // Faint grid behind everything
    g.setColour (Palette::faint.withAlpha (0.35f));
    for (int x = 0; x < bounds.getWidth(); x += 24)
        g.drawVerticalLine (x, 0.0f, (float) bounds.getHeight());
    for (int y = 0; y < bounds.getHeight(); y += 24)
        g.drawHorizontalLine (y, 0.0f, (float) bounds.getWidth());

    // Header
    auto header = bounds.withHeight (headerHeight).reduced (margin + 6, 6);
    g.setFont (monoFont (30.0f, true));
    g.setColour (Palette::phosphor.withAlpha (0.18f));
    g.drawText ("LAB X-3", header.translated (0, 1).expanded (1, 0), juce::Justification::topLeft);
    g.setColour (Palette::phosphor);
    g.drawText ("LAB X-3", header, juce::Justification::topLeft);
    g.setFont (monoFont (11.0f));
    g.setColour (Palette::dim);
    g.drawText (utf8 ("ОАЗИС-3  //  SUBTLE MATTER SYNTHESIZER"), header.withTrimmedTop (32), juce::Justification::topLeft);
    g.setColour (Palette::dim);
    g.drawText (juce::String (vendorName) + "  //  v" + versionString, header.withTrimmedTop (32), juce::Justification::topRight);
    g.setColour (Palette::border);
    g.drawHorizontalLine (headerHeight - 2, (float) margin, (float) (bounds.getWidth() - margin));

    // Panels
    auto drawPanel = [&g] (const Panel& panel)
    {
        const auto r = panel.bounds.toFloat();
        g.setColour (Palette::panel.withAlpha (0.92f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (panel.accent ? Palette::amber.withAlpha (0.45f) : Palette::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

        auto title = panel.bounds.withHeight (titleHeight).reduced (panelPad, 0);
        g.setFont (monoFont (11.0f, true));
        g.setColour (panel.accent ? Palette::amber : Palette::phosphor.withAlpha (0.85f));
        g.drawText (panel.title, title.withTrimmedTop (3), juce::Justification::centredLeft);
        g.setColour (panel.accent ? Palette::amber.withAlpha (0.3f) : Palette::faint);
        g.drawHorizontalLine (panel.bounds.getY() + titleHeight, r.getX() + 4.0f, r.getRight() - 4.0f);
    };

    for (const auto& row : rows)
        for (const auto& panel : row)
            drawPanel (panel);
    for (const auto& panel : sidePanels)
        drawPanel (panel);

    // Footer
    auto footer = bounds.withTop (bounds.getHeight() - footerHeight).reduced (margin + 6, 4);
    g.setFont (monoFont (10.0f));
    g.setColour (Palette::dim);
    g.drawText (juce::String ("OPERATION FAIRWAY  //  AGPL-3.0  //  ") + projectUrl, footer, juce::Justification::centredLeft);
    g.drawText ("SOUND SOURCES READ FROM YOUR OWN DISK  //  NO GAME AUDIO INCLUDED", footer, juce::Justification::centredRight);
}

void LabX3AudioProcessorEditor::layoutRow (std::vector<Panel>& row, juce::Rectangle<int> area, float unitWidth)
{
    float naturalTotal = 0.0f;
    for (const auto& panel : row)
    {
        float units = 0.0f;
        for (const auto& item : panel.items)
            units += item.units;
        naturalTotal += units * unitWidth + (float) (2 * panelPad);
    }

    const float spare = std::max (0.0f, (float) area.getWidth() - naturalTotal - (float) (gap * ((int) row.size() - 1)));
    const float extraPerPanel = spare / (float) row.size();

    float x = (float) area.getX();
    for (auto& panel : row)
    {
        float units = 0.0f;
        for (const auto& item : panel.items)
            units += item.units;

        const float width = units * unitWidth + (float) (2 * panelPad) + extraPerPanel;
        panel.bounds = juce::Rectangle<int> ((int) std::round (x), area.getY(), (int) std::round (width), area.getHeight());

        auto content = panel.bounds.withTrimmedTop (titleHeight + 5).reduced (panelPad, 0).withTrimmedBottom (6);
        const float contentNatural = units * unitWidth;
        float cx = (float) content.getX() + ((float) content.getWidth() - contentNatural) * 0.5f;
        for (const auto& item : panel.items)
        {
            const float w = item.units * unitWidth;
            auto cell = juce::Rectangle<int> ((int) std::round (cx), content.getY(), (int) std::round (w), content.getHeight());
            if (dynamic_cast<LabeledChoice*> (item.component) != nullptr)
                cell = cell.reduced (3, 0).withHeight (37).withY (content.getY() + 8);
            item.component->setBounds (cell);
            cx += w;
        }

        x += width + (float) gap;
    }
}

void LabX3AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    overlay.setBounds (bounds);

    // Header controls
    auto header = bounds.withHeight (headerHeight).reduced (margin, 14);
    auto presetArea = header.withSizeKeepingCentre (330, 26).translated (90, -4);
    prevButton.setBounds (presetArea.removeFromLeft (26));
    nextButton.setBounds (presetArea.removeFromRight (26));
    presetBox.setBounds (presetArea.reduced (4, 0));

    auto body = bounds.withTrimmedTop (headerHeight + 4).withTrimmedBottom (footerHeight).reduced (margin, 0);
    auto side = body.removeFromRight (sideWidth);
    body.removeFromRight (gap);

    // One unit width for the whole grid: the narrowest row decides, so every knob is the same size.
    const int rowCount = (int) rows.size();
    const int rowHeight = (body.getHeight() - gap * (rowCount - 1)) / rowCount;
    float unitWidth = 64.0f;
    for (const auto& row : rows)
    {
        float units = 0.0f;
        for (const auto& panel : row)
            for (const auto& item : panel.items)
                units += item.units;
        const float overhead = (float) (row.size() * 2 * panelPad + (row.size() - 1) * gap);
        unitWidth = std::min (unitWidth, ((float) body.getWidth() - overhead) / units);
    }

    for (int r = 0; r < rowCount; ++r)
        layoutRow (rows[(size_t) r], body.withTrimmedTop (r * (rowHeight + gap)).withHeight (rowHeight), unitWidth);

    // Side column
    const int heights[] = { 168, 118, 150, 0 };
    auto column = side;
    for (size_t i = 0; i < sidePanels.size(); ++i)
    {
        auto& panel = sidePanels[i];
        panel.bounds = heights[i] > 0 ? column.removeFromTop (heights[i]) : column;
        column.removeFromTop (gap);
        auto content = panel.bounds.withTrimmedTop (titleHeight + 4).reduced (panelPad, 4);
        for (auto& item : panel.items)
            item.component->setBounds (content);
    }

    if (darkKnob != nullptr)
        darkKnob->setBounds (sidePanels[2].bounds.withTrimmedTop (titleHeight + 2).reduced (60, 4));

    overlay.toFront (false);
}

//==============================================================================
void LabX3AudioProcessorEditor::refreshFromProcessor()
{
    const auto& m = audioProcessor.meters;
    // Pseudo dose rate: logarithmic in click rate so ordinary patches sit mid-scale and storms reach the red.
    const float level = juce::jlimit (0.0f, 1.0f, (m.rmsDb.load() + 60.0f) / 60.0f);
    meter.setTarget (7.0f * std::log2 (1.0f + m.clicksPerSecond.load()) + level * 14.0f + m.dark.load() * 5.0f);
    meter.tick();

    audioProcessor.copyScope (scopeBuffer.data(), (int) scopeBuffer.size());
    scopeView.setSamples (scopeBuffer.data(), (int) scopeBuffer.size());

    auto& lib = audioProcessor.getSpecimenLibrary();
    const auto status = lib.getStatus();
    specimenControls->status.setText (lib.getStatusText(), juce::dontSendNotification);
    specimenControls->status.setColour (juce::Label::textColourId,
        status == SpecimenLibrary::Status::ready ? Palette::phosphor
        : (status == SpecimenLibrary::Status::loading || status == SpecimenLibrary::Status::idle ? Palette::dim : Palette::red));

    const int program = audioProcessor.getCurrentProgram();
    if (program != lastProgram)
    {
        presetBox.setSelectedId (program + 1, juce::dontSendNotification);
        lastProgram = program;
    }

    const auto root = lib.getLibraryRoot();
    juce::String text;
    text << "VOICES    " << m.activeVoices.load() << " / " << (int) std::lround (audioProcessor.apvts.getRawParameterValue ("voices")->load()) << "\n"
         << "MIDI IN   " << juce::String (m.midiPerSecond.load(), 1) << " /s\n"
         << "PEAK      " << juce::String (m.peakDb.load(), 1) << " dBFS\n"
         << "FAULTS    " << m.faults.load() << "\n"
         << "LIBRARY   " << (root.isDirectory() ? "LINKED" : "NOT FOUND") << "\n"
         << "PRESET    " << audioProcessor.getProgramName (program);
    systemLabel.setText (text, juce::dontSendNotification);
}

void LabX3AudioProcessorEditor::stepPreset (int delta)
{
    const int count = audioProcessor.getNumPrograms();
    const int next = (audioProcessor.getCurrentProgram() + delta + count) % count;
    audioProcessor.setCurrentProgram (next);
    presetBox.setSelectedId (next + 1, juce::dontSendNotification);
    lastProgram = next;
}

void LabX3AudioProcessorEditor::chooseLibraryFolder()
{
    auto& lib = audioProcessor.getSpecimenLibrary();
    chooser = std::make_unique<juce::FileChooser> ("Select the 'stalker sounds' folder", lib.getLibraryRoot(), "*");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto dir = fc.getResult();
                              if (dir.isDirectory())
                                  audioProcessor.getSpecimenLibrary().setLibraryRoot (dir, true);
                          });
}

void LabX3AudioProcessorEditor::chooseUserFile()
{
    auto& lib = audioProcessor.getSpecimenLibrary();
    const auto start = lib.getUserFile().existsAsFile() ? lib.getUserFile() : lib.getLibraryRoot();
    chooser = std::make_unique<juce::FileChooser> ("Load a specimen", start, "*.wav;*.ogg;*.flac;*.aif;*.aiff");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (! file.existsAsFile())
                                  return;
                              audioProcessor.getSpecimenLibrary().setUserFile (file);
                              if (auto* param = audioProcessor.apvts.getParameter ("specimen_source"))
                                  param->setValueNotifyingHost (param->convertTo0to1 ((float) specimenUserChoice));
                          });
}
