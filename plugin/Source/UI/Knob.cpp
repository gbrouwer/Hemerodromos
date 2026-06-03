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

    resetButton_.setTooltip ("Reset to neutral");
    resetButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff262c34));
    resetButton_.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd7b46a));
    resetButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd7b46a));
    resetButton_.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff101318));
    resetButton_.onClick = [this]
    {
        if (resetCallback_ != nullptr)
            resetCallback_();
    };

    addAndMakeVisible (slider_);
    addAndMakeVisible (nameLabel_);
    addAndMakeVisible (valueLabel_);
    addAndMakeVisible (resetButton_);
    startTimerHz (20);
}

void Knob::setNeutralValue (double neutral)
{
    slider_.setDoubleClickReturnValue (true, neutral);
}

void Knob::setResetCallback (std::function<void()> callback)
{
    resetCallback_ = std::move (callback);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (22);
    resetButton_.setBounds (header.removeFromRight (22).reduced (2));
    nameLabel_.setBounds (header);
    valueLabel_.setBounds (area.removeFromBottom (18));
    slider_.setBounds (area.reduced (2));
}

void Knob::timerCallback()
{
    auto text = juce::String (slider_.getValue(), units_.isEmpty() ? 2 : 1);
    if (slider_.getMinimum() < 0.0 && slider_.getValue() > 0.0)
        text = "+" + text;

    if (units_.isNotEmpty())
        text << " " << units_;

    valueLabel_.setText (text, juce::dontSendNotification);
}
} // namespace ui
