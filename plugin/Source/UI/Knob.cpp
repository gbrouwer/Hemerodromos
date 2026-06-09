#include "UI/Knob.h"

#include <cmath>

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

    nameLabel_.setText (label.toUpperCase(), juce::dontSendNotification);
    nameLabel_.setJustificationType (juce::Justification::centred);
    nameLabel_.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    nameLabel_.setColour (juce::Label::textColourId, juce::Colour (0xff20242a));

    valueLabel_.setJustificationType (juce::Justification::centred);
    valueLabel_.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    valueLabel_.setColour (juce::Label::textColourId, juce::Colour (0xff20242a));

    resetButton_.setTooltip ("Reset to neutral");
    resetButton_.getProperties().set ("hardwareLed", true);
    resetButton_.setClickingTogglesState (false);
    resetButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff262c34));
    resetButton_.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff49f074));
    resetButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffffff));
    resetButton_.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffffffff));
    resetButton_.onClick = [this]
    {
        if (resetCallback_ != nullptr)
            resetCallback_();
    };
    setAccentColour (accentColour_);

    addAndMakeVisible (slider_);
    addAndMakeVisible (nameLabel_);
    addAndMakeVisible (valueLabel_);
    addAndMakeVisible (resetButton_);
    startTimerHz (20);
}

void Knob::setNeutralValue (double neutral)
{
    neutralValue_ = neutral;
    slider_.setDoubleClickReturnValue (true, neutral);
}

void Knob::setResetCallback (std::function<void()> callback)
{
    resetCallback_ = std::move (callback);
}

void Knob::setAccentColour (juce::Colour colour)
{
    accentColour_ = colour;
    applyColours();
}

void Knob::setLightMode (bool enabled)
{
    lightMode_ = enabled;
    applyColours();
}

void Knob::applyColours()
{
    slider_.setColour (juce::Slider::thumbColourId, accentColour_.brighter (0.25f));
    slider_.setColour (juce::Slider::rotarySliderFillColourId, accentColour_);
    nameLabel_.setColour (juce::Label::textColourId, lightMode_ ? juce::Colour (0xff20242a)
                                                                 : juce::Colour (0xfff1efe7));
    valueLabel_.setColour (juce::Label::textColourId, lightMode_ ? juce::Colour (0xff20242a)
                                                                  : juce::Colour (0xfff1efe7));
    resetButton_.setColour (juce::TextButton::buttonColourId, lightMode_ ? juce::Colour (0xfff7f8f8)
                                                                          : juce::Colour (0xff101010));
    resetButton_.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff49f074));
    resetButton_.setColour (juce::TextButton::textColourOffId, lightMode_ ? juce::Colour (0xff20242a)
                                                                           : juce::Colour (0xffffffff));
    resetButton_.setColour (juce::TextButton::textColourOnId, lightMode_ ? juce::Colour (0xff20242a)
                                                                          : juce::Colour (0xffffffff));
    repaint();
}

void Knob::resized()
{
    auto area = getLocalBounds();

    nameLabel_.setBounds (area.removeFromTop (30));

    auto valueRow = area.removeFromBottom (34);
    constexpr auto valueWidth = 92;
    constexpr auto resetWidth = 30;
    constexpr auto gap = 8;
    const auto groupWidth = juce::jmin (valueWidth + resetWidth + gap, valueRow.getWidth());
    auto valueGroup = valueRow.withSizeKeepingCentre (groupWidth, valueRow.getHeight());

    const auto actualResetWidth = juce::jmin (resetWidth, valueGroup.getWidth());
    auto resetArea = valueGroup.removeFromRight (actualResetWidth);
    valueGroup.removeFromRight (juce::jmin (gap, valueGroup.getWidth()));
    valueLabel_.setBounds (valueGroup);
    resetButton_.setBounds (resetArea.withSizeKeepingCentre (30, 34));

    const auto knobSide = juce::jmax (0, juce::jmin (area.getWidth(), area.getHeight()) - 2);
    slider_.setBounds (juce::Rectangle<int> (knobSide, knobSide).withCentre (area.getCentre()));
}

void Knob::timerCallback()
{
    auto text = juce::String (slider_.getValue(), units_.isEmpty() ? 2 : 1);
    if (slider_.getMinimum() < 0.0 && slider_.getValue() > 0.0)
        text = "+" + text;

    if (units_.isNotEmpty())
        text << " " << units_;

    valueLabel_.setText (text, juce::dontSendNotification);
    resetButton_.setToggleState (std::abs (slider_.getValue() - neutralValue_) <= 0.0005,
                                 juce::dontSendNotification);
}
} // namespace ui
