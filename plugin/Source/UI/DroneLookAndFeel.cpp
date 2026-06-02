#include "UI/DroneLookAndFeel.h"

namespace ui
{
DroneLookAndFeel::DroneLookAndFeel()
{
    setColour (juce::Slider::thumbColourId, juce::Colour (0xffd7b46a));
    setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffd7b46a));
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff30343a));
    setColour (juce::Label::textColourId, juce::Colour (0xffd9dde3));
}

void DroneLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                         int x,
                                         int y,
                                         int width,
                                         int height,
                                         float sliderPosProportional,
                                         float rotaryStartAngle,
                                         float rotaryEndAngle,
                                         juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                               static_cast<float> (y),
                                               static_cast<float> (width),
                                               static_cast<float> (height))
                            .reduced (5.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff2e343d));
    g.strokePath (backgroundArc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                            rotaryStartAngle, angle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (valueArc, juce::PathStrokeType (4.6f, juce::PathStrokeType::curved));

    auto knob = bounds.reduced (radius * 0.18f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff22272e), knob.getCentreX(), knob.getY(),
                                             juce::Colour (0xff111418), knob.getCentreX(), knob.getBottom(),
                                             false));
    g.fillEllipse (knob);
    g.setColour (juce::Colour (0xff4a515d));
    g.drawEllipse (knob, 1.2f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.8f, -radius * 0.68f, 3.6f, radius * 0.34f, 1.8f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (juce::Colour (0xfff3d28d));
    g.fillPath (pointer);
}
} // namespace ui
