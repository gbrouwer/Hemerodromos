#pragma once

#include <JuceHeader.h>

namespace ui
{
class DroneLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DroneLookAndFeel();

    void drawRotarySlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;
};
} // namespace ui
