#include "UI/Knob.h"

namespace ui
{
Knob::Knob (juce::String label, juce::String units)
    : units_ (std::move (units))
{
    slider_.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider_.setRotaryParameters (juce::MathConstants<float>::pi * 1.18f,
                                 juce::MathConstants<float>::pi * 2.82f,
                                 true);
    slider_.setMouseDragSensitivity (220);

    nameLabel_.setText (label, juce::dontSendNotification);
    nameLabel_.setJustificationType (juce::Justification::centred);
    nameLabel_.setFont (juce::FontOptions (13.0f, juce::Font::bold));

    valueLabel_.setJustificationType (juce::Justification::centred);
    valueLabel_.setFont (juce::FontOptions (12.0f));
    valueLabel_.setColour (juce::Label::textColourId, juce::Colour (0xffaeb6c2));

    addAndMakeVisible (slider_);
    addAndMakeVisible (nameLabel_);
    addAndMakeVisible (valueLabel_);
    startTimerHz (20);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    nameLabel_.setBounds (area.removeFromTop (20));
    valueLabel_.setBounds (area.removeFromBottom (18));
    slider_.setBounds (area.reduced (2));
}

void Knob::timerCallback()
{
    auto text = juce::String (slider_.getValue(), units_.isEmpty() ? 2 : 1);
    if (units_.isNotEmpty())
        text << " " << units_;
    valueLabel_.setText (text, juce::dontSendNotification);
}
} // namespace ui
