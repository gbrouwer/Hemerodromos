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

    void resized() override;

private:
    void timerCallback() override;

    juce::Slider slider_;
    juce::Label nameLabel_;
    juce::Label valueLabel_;
    juce::TextButton resetButton_ { "N" };
    juce::String units_;
    std::function<void()> resetCallback_;
};
} // namespace ui
