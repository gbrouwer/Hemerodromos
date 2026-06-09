#pragma once

#include <JuceHeader.h>

namespace ui
{
class DroneLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DroneLookAndFeel();

    void setLightMode (bool enabled);
    bool isLightMode() const noexcept { return lightMode_; }

    void drawRotarySlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;
    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;
    void drawToggleButton (juce::Graphics& g,
                           juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
    void drawComboBox (juce::Graphics& g,
                       int width,
                       int height,
                       bool isButtonDown,
                       int buttonX,
                       int buttonY,
                       int buttonW,
                       int buttonH,
                       juce::ComboBox& box) override;
    juce::Font getComboBoxFont (juce::ComboBox& box) override;
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;
    juce::Font getLabelFont (juce::Label& label) override;

    juce::Font displayFont (float height, int styleFlags = juce::Font::plain) const;

private:
    juce::Typeface::Ptr displayTypeface_;
    bool lightMode_ = true;

    static juce::Typeface::Ptr loadDisplayTypeface();
    juce::Colour panelTextColour() const noexcept;
    juce::Colour mutedControlColour() const noexcept;
    juce::Colour controlFaceTopColour (bool active, bool down) const noexcept;
    juce::Colour controlFaceBottomColour (bool active, bool down) const noexcept;
    void drawLedRing (juce::Graphics& g,
                      juce::Point<float> centre,
                      float radius,
                      float sliderPosProportional,
                      float rotaryStartAngle,
                      float rotaryEndAngle,
                      juce::Colour accent) const;
    void drawLed (juce::Graphics& g, juce::Rectangle<float> bounds, bool enabled, juce::Colour colour) const;
    void drawHardwareButtonBody (juce::Graphics& g,
                                 juce::Rectangle<float> bounds,
                                 bool active,
                                 bool highlighted,
                                 bool down) const;
};
} // namespace ui
