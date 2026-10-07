#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CrtLookAndFeel.h"

namespace labx3::ui
{
    using APVTS = juce::AudioProcessorValueTreeState;

    // Rotary control with a caption above and its value below.
    class LabeledKnob final : public juce::Component
    {
    public:
        LabeledKnob (APVTS& state, const juce::String& paramID, const juce::String& caption, bool accent = false)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 14);
            slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            if (accent)
                slider.getProperties().set ("accent", true);

            if (auto* param = state.getParameter (paramID))
            {
                const auto range = param->getNormalisableRange();
                if (range.start < 0.0f && range.end > 0.0f)
                    slider.getProperties().set ("bipolar", true);
                slider.setTooltip (param->getName (64));
            }

            label.setText (caption, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setColour (juce::Label::textColourId, accent ? Palette::amber : Palette::dim);
            label.setInterceptsMouseClicks (false, false);

            addAndMakeVisible (label);
            addAndMakeVisible (slider);

            attachment = std::make_unique<APVTS::SliderAttachment> (state, paramID, slider);

            if (auto* param = state.getParameter (paramID))
                slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        }

        void resized() override
        {
            auto b = getLocalBounds();
            label.setBounds (b.removeFromTop (13));
            slider.setBounds (b);
        }

        juce::Slider slider;
        juce::Label label;

    private:
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    // Choice parameter as a combo box with a caption above.
    class LabeledChoice final : public juce::Component
    {
    public:
        // populate, when given, fills the box itself (headings, sub-menus). Items must stay in
        // parameter order: the attachment maps item index to choice index, counting depth-first
        // and skipping headings and sub-menu titles.
        LabeledChoice (APVTS& state, const juce::String& paramID, const juce::String& caption,
                       std::function<void (juce::ComboBox&)> populate = nullptr)
        {
            if (populate != nullptr)
                populate (box);
            else if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID)))
                box.addItemList (choice->choices, 1);

            label.setText (caption, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centredLeft);
            label.setInterceptsMouseClicks (false, false);

            addAndMakeVisible (label);
            addAndMakeVisible (box);
            attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramID, box);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            label.setBounds (b.removeFromTop (13));
            b.removeFromTop (2);
            box.setBounds (b.removeFromTop (22));
        }

        juce::ComboBox box;
        juce::Label label;

    private:
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    // On/off parameter as a latching button with a caption above. The button reads onText or offText.
    class LabeledToggle final : public juce::Component
    {
    public:
        LabeledToggle (APVTS& state, const juce::String& paramID, const juce::String& caption,
                       const juce::String& onText, const juce::String& offText)
            : textOn (onText), textOff (offText)
        {
            button.setClickingTogglesState (true);
            button.onStateChange = [this] { button.setButtonText (button.getToggleState() ? textOn : textOff); };

            label.setText (caption, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setInterceptsMouseClicks (false, false);

            addAndMakeVisible (label);
            addAndMakeVisible (button);
            attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramID, button);
            button.setButtonText (button.getToggleState() ? textOn : textOff);

            if (auto* param = state.getParameter (paramID))
                button.setTooltip (param->getName (64));
        }

        void resized() override
        {
            auto b = getLocalBounds();
            label.setBounds (b.removeFromTop (13));
            b.removeFromTop (10);
            button.setBounds (b.removeFromTop (24).reduced (2, 0));
        }

        juce::TextButton button;
        juce::Label label;

    private:
        juce::String textOn, textOff;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    // Dosimeter gauge. Shows a pseudo dose rate driven by geiger activity and output level.
    class RadiationMeter final : public juce::Component
    {
    public:
        void setTarget (float v) noexcept { target = std::clamp (v, 0.0f, maxValue); }

        void tick()
        {
            const float coef = target > value ? 0.35f : 0.07f;
            value += (target - value) * coef;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (6.0f);
            const auto readout = bounds.removeFromBottom (22.0f);
            const float radius = std::min (bounds.getWidth() * 0.5f, bounds.getHeight() * 0.95f) - 4.0f;
            const juce::Point<float> centre (bounds.getCentreX(), bounds.getBottom() - 4.0f);
            const float start = -juce::MathConstants<float>::halfPi * 0.95f;
            const float end = juce::MathConstants<float>::halfPi * 0.95f;

            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, end, true);
            g.setColour (Palette::faint);
            g.strokePath (arc, juce::PathStrokeType (3.0f));

            const float danger = start + (end - start) * 0.7f;
            juce::Path hot;
            hot.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, danger, end, true);
            g.setColour (Palette::red.withAlpha (0.55f));
            g.strokePath (hot, juce::PathStrokeType (3.0f));

            g.setFont (monoFont (10.0f));
            for (int i = 0; i <= 10; ++i)
            {
                const float a = start + (end - start) * (float) i / 10.0f;
                const float inner = radius - (i % 2 == 0 ? 10.0f : 6.0f);
                const auto p1 = centre.getPointOnCircumference (inner, a);
                const auto p2 = centre.getPointOnCircumference (radius, a);
                g.setColour (i >= 7 ? Palette::red.withAlpha (0.8f) : Palette::dim);
                g.drawLine ({ p1, p2 }, 1.2f);
                if (i % 2 == 0)
                {
                    const auto tp = centre.getPointOnCircumference (radius - 20.0f, a);
                    g.drawText (juce::String (i * 5), juce::Rectangle<float> (24.0f, 12.0f).withCentre (tp), juce::Justification::centred);
                }
            }

            const float needleAngle = start + (end - start) * (value / maxValue);
            const auto tip = centre.getPointOnCircumference (radius - 4.0f, needleAngle);
            g.setColour (Palette::amber.withAlpha (0.25f));
            g.drawLine ({ centre, tip }, 5.0f);
            g.setColour (Palette::amber);
            g.drawLine ({ centre, tip }, 1.8f);
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (centre));

            g.setColour (value > maxValue * 0.7f ? Palette::red : Palette::phosphor);
            g.setFont (monoFont (14.0f, true));
            g.drawText (juce::String (value, 1) + " " + utf8 ("мкЗв/ч"), readout, juce::Justification::centred);
        }

    private:
        static constexpr float maxValue = 50.0f;
        float value = 0.0f, target = 0.0f;
    };

    // Phosphor oscilloscope with a rising-edge trigger and a short persistence trail.
    class ScopeView final : public juce::Component
    {
    public:
        static constexpr int points = 512;

        void setSamples (const float* source, int numSource)
        {
            // Find a rising zero crossing in the first half so the trace stands still.
            int start = 0;
            const int searchEnd = std::max (0, numSource - points * 2);
            for (int i = 1; i < searchEnd; ++i)
                if (source[i - 1] <= 0.0f && source[i] > 0.0f)
                {
                    start = i;
                    break;
                }

            history[2] = history[1];
            history[1] = history[0];
            for (int i = 0; i < points; ++i)
            {
                const int idx = start + i * 2;
                history[0][(size_t) i] = idx < numSource ? source[idx] : 0.0f;
            }
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            g.setColour (Palette::background);
            g.fillRect (b);

            g.setColour (Palette::faint);
            for (int i = 1; i < 8; ++i)
                g.drawVerticalLine ((int) (b.getX() + b.getWidth() * (float) i / 8.0f), b.getY(), b.getBottom());
            for (int i = 1; i < 4; ++i)
                g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * (float) i / 4.0f), b.getX(), b.getRight());

            const float alphas[3] = { 1.0f, 0.35f, 0.15f };
            for (int h = 2; h >= 0; --h)
            {
                juce::Path trace;
                for (int i = 0; i < points; ++i)
                {
                    const float x = b.getX() + b.getWidth() * (float) i / (float) (points - 1);
                    const float y = b.getCentreY() - std::clamp (history[(size_t) h][(size_t) i], -1.0f, 1.0f) * b.getHeight() * 0.45f;
                    if (i == 0)
                        trace.startNewSubPath (x, y);
                    else
                        trace.lineTo (x, y);
                }
                if (h == 0)
                {
                    g.setColour (Palette::phosphor.withAlpha (0.18f));
                    g.strokePath (trace, juce::PathStrokeType (4.0f));
                }
                g.setColour (Palette::phosphor.withAlpha (alphas[h]));
                g.strokePath (trace, juce::PathStrokeType (1.3f));
            }

            g.setColour (Palette::border);
            g.drawRect (b, 1.0f);
        }

    private:
        std::array<std::array<float, points>, 3> history {};
    };

    // Scanlines and a vignette drawn over everything. Never takes mouse input.
    class ScanlineOverlay final : public juce::Component
    {
    public:
        ScanlineOverlay()
        {
            setInterceptsMouseClicks (false, false);
            pattern = juce::Image (juce::Image::ARGB, 4, 3, true);
            for (int x = 0; x < 4; ++x)
                pattern.setPixelAt (x, 2, juce::Colour (0x2a000000));
        }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            g.setFillType (juce::FillType (pattern, juce::AffineTransform()));
            g.fillRect (b);

            juce::ColourGradient vignette (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                                           juce::Colour (0x66000000), b.getX(), b.getY(), true);
            g.setGradientFill (vignette);
            g.fillRect (b);
        }

    private:
        juce::Image pattern;
    };
}
