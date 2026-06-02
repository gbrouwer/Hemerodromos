#pragma once

#include <JuceHeader.h>

namespace ui
{
class Knob final : public juce::Component,
                   private juce::Timer
{
public:
    Knob (juce::String label, juce::String units = {});

    juce::Slider& slider() noexcept { return slider_; }

    void resized() override;

private:
    void timerCallback() override;

    juce::Slider slider_;
    juce::Label nameLabel_;
    juce::Label valueLabel_;
    juce::String units_;
};
} // namespace ui
