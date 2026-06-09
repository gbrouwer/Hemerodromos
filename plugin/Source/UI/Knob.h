#pragma once

#include <JuceHeader.h>

#include <functional>

namespace ui
{
class Knob final : public juce::Component,
                   private juce::Timer
{
public:
    Knob (juce::String label, juce::String units = {});

    juce::Slider& slider() noexcept { return slider_; }
    void setNeutralValue (double neutral);
    void setResetCallback (std::function<void()> callback);
    void setAccentColour (juce::Colour colour);
    void setLightMode (bool enabled);

    void resized() override;

private:
    void timerCallback() override;
    void applyColours();

    juce::Slider slider_;
    juce::Label nameLabel_;
    juce::Label valueLabel_;
    juce::TextButton resetButton_ {};
    juce::String units_;
    juce::Colour accentColour_ { 0xffd7b46a };
    double neutralValue_ = 0.0;
    bool lightMode_ = true;
    std::function<void()> resetCallback_;
};
} // namespace ui
