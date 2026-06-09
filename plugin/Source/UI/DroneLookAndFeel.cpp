#include "UI/DroneLookAndFeel.h"

#include "BinaryData.h"

namespace ui
{
DroneLookAndFeel::DroneLookAndFeel()
    : displayTypeface_ (loadDisplayTypeface())
{
    if (displayTypeface_ != nullptr)
        setDefaultSansSerifTypeface (displayTypeface_);

    setLightMode (true);
}

void DroneLookAndFeel::setLightMode (bool enabled)
{
    lightMode_ = enabled;

    setColour (juce::Slider::thumbColourId, juce::Colour (0xffd7b46a));
    setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffd7b46a));
    setColour (juce::Slider::rotarySliderOutlineColourId, mutedControlColour());
    setColour (juce::Label::textColourId, panelTextColour());
    setColour (juce::ComboBox::textColourId, panelTextColour());
    setColour (juce::ComboBox::backgroundColourId, lightMode_ ? juce::Colour (0xffffffff)
                                                               : juce::Colour (0xff101418));
    setColour (juce::ComboBox::outlineColourId, lightMode_ ? juce::Colour (0xff98958b)
                                                            : juce::Colour (0xff3b424a));
    setColour (juce::PopupMenu::backgroundColourId, lightMode_ ? juce::Colour (0xffffffff)
                                                                : juce::Colour (0xff101418));
    setColour (juce::PopupMenu::textColourId, panelTextColour());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, lightMode_ ? juce::Colour (0xffd9d1bf)
                                                                           : juce::Colour (0xff24313a));
    setColour (juce::PopupMenu::highlightedTextColourId, panelTextColour());
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
    const auto outer = juce::Rectangle<float> (static_cast<float> (x),
                                              static_cast<float> (y),
                                              static_cast<float> (width),
                                              static_cast<float> (height));
    const auto squareSize = juce::jmin (outer.getWidth(), outer.getHeight());
    const auto bounds = juce::Rectangle<float> (squareSize, squareSize)
                            .withCentre (outer.getCentre())
                            .reduced (7.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

    drawLedRing (g, centre, radius * 0.93f, sliderPosProportional, rotaryStartAngle, rotaryEndAngle, accent);

    const auto skirtRadius = radius * 0.72f;
    auto skirt = juce::Rectangle<float> (skirtRadius * 2.0f, skirtRadius * 2.0f).withCentre (centre);
    g.setColour (lightMode_ ? juce::Colour (0x28000000) : juce::Colour (0x8a000000));
    g.fillEllipse (skirt.translated (0.0f, radius * 0.055f).expanded (1.5f));
    g.setGradientFill (juce::ColourGradient (lightMode_ ? juce::Colour (0xff24292f)
                                                         : juce::Colour (0xff2c333b),
                                             skirt.getCentreX(),
                                             skirt.getY(),
                                             lightMode_ ? juce::Colour (0xff050607)
                                                        : juce::Colour (0xff07090b),
                                             skirt.getCentreX(),
                                             skirt.getBottom(),
                                             false));
    g.fillEllipse (skirt);
    g.setColour (lightMode_ ? juce::Colour (0xff000000).withAlpha (0.65f)
                            : juce::Colour (0xff67717c).withAlpha (0.72f));
    g.drawEllipse (skirt, 1.2f);
    g.setColour (accent.withAlpha (0.72f));
    g.drawEllipse (skirt.reduced (skirtRadius * 0.12f), 1.0f);

    const auto capRadius = radius * 0.46f;
    auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff464d54),
                                             cap.getCentreX() - capRadius * 0.3f,
                                             cap.getY(),
                                             juce::Colour (0xff14181d),
                                             cap.getCentreX() + capRadius * 0.2f,
                                             cap.getBottom(),
                                             false));
    g.fillEllipse (cap);

    juce::Path pointer;
    pointer.addRoundedRectangle (-radius * 0.035f,
                                 -radius * 0.54f,
                                 radius * 0.07f,
                                 radius * 0.27f,
                                 radius * 0.035f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (juce::Colour (0xfffff4d0));
    g.fillPath (pointer);

    g.setColour (lightMode_ ? juce::Colour (0x55ffffff) : juce::Colour (0x33ffffff));
    g.fillEllipse (cap.reduced (capRadius * 0.38f).translated (-capRadius * 0.12f, -capRadius * 0.18f));
}

void DroneLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                             juce::Button& button,
                                             const juce::Colour&,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto hasLed = static_cast<bool> (button.getProperties().getWithDefault ("hardwareLed", false));
    const auto reserveLedSpace = hasLed
                              || static_cast<bool> (button.getProperties().getWithDefault ("reserveLedSpace",
                                                                                           false));
    if (reserveLedSpace)
    {
        auto ledSlot = bounds.removeFromTop (juce::jmin (8.0f, bounds.getHeight() * 0.32f));
        if (hasLed)
        {
            drawLed (g,
                     ledSlot.withSizeKeepingCentre (7.0f, 7.0f),
                     button.getToggleState(),
                     button.findColour (juce::TextButton::buttonOnColourId));
        }
        bounds.removeFromTop (5.0f);
    }

    drawHardwareButtonBody (
        g,
        bounds,
        button.getToggleState(),
        shouldDrawButtonAsHighlighted,
        shouldDrawButtonAsDown);
}

void DroneLookAndFeel::drawButtonText (juce::Graphics& g,
                                       juce::TextButton& button,
                                       bool,
                                       bool)
{
    auto area = button.getLocalBounds().reduced (3, 1);
    if (static_cast<bool> (button.getProperties().getWithDefault ("hardwareLed", false))
        || static_cast<bool> (button.getProperties().getWithDefault ("reserveLedSpace", false)))
    {
        area.removeFromTop (13);
    }

    g.setFont (displayFont (juce::jlimit (10.0f, 15.0f, static_cast<float> (area.getHeight()) * 0.56f),
                            juce::Font::bold));
    g.setColour (panelTextColour());
    g.drawFittedText (button.getButtonText().toUpperCase(),
                      area,
                      juce::Justification::centred,
                      1);
}

void DroneLookAndFeel::drawToggleButton (juce::Graphics& g,
                                         juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted,
                                         bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto hasLed = ! static_cast<bool> (button.getProperties().getWithDefault ("hideHardwareLed", false));
    auto ledSlot = bounds.removeFromTop (juce::jmin (10.0f, bounds.getHeight() * 0.34f));
    if (hasLed)
    {
        drawLed (g,
                 ledSlot.withSizeKeepingCentre (8.0f, 8.0f),
                 button.getToggleState(),
                 button.findColour (juce::ToggleButton::tickColourId));
    }
    bounds.removeFromTop (6.0f);
    drawHardwareButtonBody (g, bounds, button.getToggleState(), shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    g.setFont (displayFont (juce::jlimit (10.0f, 15.0f, bounds.getHeight() * 0.52f), juce::Font::bold));
    g.setColour (panelTextColour());
    g.drawFittedText (button.getButtonText().toUpperCase(),
                      bounds.toNearestInt().reduced (5, 1),
                      juce::Justification::centred,
                      1);
}

void DroneLookAndFeel::drawComboBox (juce::Graphics& g,
                                     int width,
                                     int height,
                                     bool isButtonDown,
                                     int,
                                     int,
                                     int,
                                     int,
                                     juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height))
                      .reduced (1.0f);
    drawHardwareButtonBody (g, bounds, true, box.isMouseOver(), isButtonDown);

    auto arrowArea = juce::Rectangle<float> (static_cast<float> (width - 20),
                                            0.0f,
                                            14.0f,
                                            static_cast<float> (height));
    juce::Path arrow;
    arrow.startNewSubPath (arrowArea.getCentreX() - 4.0f, arrowArea.getCentreY() - 2.0f);
    arrow.lineTo (arrowArea.getCentreX() + 4.0f, arrowArea.getCentreY() - 2.0f);
    arrow.lineTo (arrowArea.getCentreX(), arrowArea.getCentreY() + 3.5f);
    arrow.closeSubPath();
    g.setColour (lightMode_ ? juce::Colour (0xff30343a) : juce::Colour (0xffd7b46a));
    g.fillPath (arrow);
}

juce::Font DroneLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return displayFont (14.0f, juce::Font::bold);
}

void DroneLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, panelTextColour());
}

juce::Font DroneLookAndFeel::getLabelFont (juce::Label& label)
{
    const auto source = label.getFont();
    return displayFont (source.getHeight(), source.getStyleFlags());
}

juce::Colour DroneLookAndFeel::panelTextColour() const noexcept
{
    return lightMode_ ? juce::Colour (0xff20242a) : juce::Colour (0xfff1efe7);
}

juce::Colour DroneLookAndFeel::mutedControlColour() const noexcept
{
    return lightMode_ ? juce::Colour (0xffb8b4aa) : juce::Colour (0xff343b44);
}

juce::Colour DroneLookAndFeel::controlFaceTopColour (bool active, bool down) const noexcept
{
    if (lightMode_)
        return down ? juce::Colour (0xfff4f5f5)
                    : active ? juce::Colour (0xffffffff) : juce::Colour (0xfffbfbfb);

    return active ? juce::Colour (0xff20262d) : juce::Colour (0xff12171d);
}

juce::Colour DroneLookAndFeel::controlFaceBottomColour (bool active, bool down) const noexcept
{
    if (lightMode_)
        return down ? juce::Colour (0xffebeeee)
                    : active ? juce::Colour (0xfff9fbfb) : juce::Colour (0xfff4f6f6);

    return down ? juce::Colour (0xff06080a)
                : active ? juce::Colour (0xff0d1115) : juce::Colour (0xff080a0d);
}

void DroneLookAndFeel::drawLedRing (juce::Graphics& g,
                                    juce::Point<float> centre,
                                    float radius,
                                    float sliderPosProportional,
                                    float rotaryStartAngle,
                                    float rotaryEndAngle,
                                    juce::Colour accent) const
{
    constexpr auto segmentCount = 31;
    const auto activePosition = juce::jlimit (0.0f, 1.0f, sliderPosProportional);
    const auto offColour = lightMode_ ? juce::Colour (0xffc9c4b8) : juce::Colour (0xff2a3037);
    for (int segment = 0; segment < segmentCount; ++segment)
    {
        const auto proportion = static_cast<float> (segment) / static_cast<float> (segmentCount - 1);
        const auto angle = rotaryStartAngle + proportion * (rotaryEndAngle - rotaryStartAngle);
        const auto active = proportion <= activePosition + 0.001f;
        const auto segmentCentre = centre + juce::Point<float> (std::sin (angle), -std::cos (angle)) * radius;
        const auto dotSize = active ? 4.0f : 3.2f;
        auto dot = juce::Rectangle<float> (dotSize, dotSize).withCentre (segmentCentre);

        if (active)
        {
            g.setColour (accent.withAlpha (lightMode_ ? 0.20f : 0.34f));
            g.fillEllipse (dot.expanded (2.2f));
            g.setColour (accent.brighter (lightMode_ ? 0.18f : 0.35f));
        }
        else
        {
            g.setColour (offColour);
        }

        g.fillEllipse (dot);
        g.setColour ((lightMode_ ? juce::Colour (0xfff8f5ec) : juce::Colour (0xff000000)).withAlpha (0.45f));
        g.fillEllipse (dot.removeFromTop (dot.getHeight() * 0.42f).reduced (1.1f, 0.8f));
    }
}

juce::Typeface::Ptr DroneLookAndFeel::loadDisplayTypeface()
{
    for (auto index = 0; index < BinaryData::namedResourceListSize; ++index)
    {
        const auto resourceName = juce::String (BinaryData::namedResourceList[index]);
        const auto* original = BinaryData::getNamedResourceOriginalFilename (resourceName.toRawUTF8());
        if (original == nullptr)
            continue;

        if (! juce::String (original).endsWithIgnoreCase ("Audiowide-Regular.ttf"))
            continue;

        auto size = 0;
        if (const auto* data = BinaryData::getNamedResource (resourceName.toRawUTF8(), size))
            return juce::Typeface::createSystemTypefaceFor (data, static_cast<size_t> (size));
    }

    return {};
}

juce::Font DroneLookAndFeel::displayFont (float height, int styleFlags) const
{
    auto font = displayTypeface_ != nullptr
        ? juce::Font (juce::FontOptions (displayTypeface_).withHeight (height))
        : juce::Font (juce::FontOptions (height));

    if ((styleFlags & juce::Font::bold) != 0)
        font = font.boldened();

    return font;
}

void DroneLookAndFeel::drawLed (juce::Graphics& g,
                                juce::Rectangle<float> bounds,
                                bool enabled,
                                juce::Colour colour) const
{
    bounds = bounds.reduced (0.5f);
    g.setColour (lightMode_ ? juce::Colour (0xff8f8b81) : juce::Colour (0xff050505));
    g.fillEllipse (bounds.expanded (2.0f));
    g.setColour (lightMode_ ? juce::Colour (0xff5a564d) : juce::Colour (0xff2a2418));
    g.drawEllipse (bounds.expanded (1.0f), 1.0f);

    const auto ledColour = enabled ? colour.brighter (0.4f)
                                   : (lightMode_ ? juce::Colour (0xff716d63) : juce::Colour (0xff211b12));
    juce::ColourGradient glow (ledColour.withAlpha (enabled ? 0.70f : 0.16f),
                               bounds.getCentreX(),
                               bounds.getCentreY(),
                               juce::Colour (0x00000000),
                               bounds.getCentreX() + bounds.getWidth() * 1.8f,
                               bounds.getCentreY() + bounds.getHeight() * 1.8f,
                               true);
    g.setGradientFill (glow);
    g.fillEllipse (bounds.expanded (4.0f));
    g.setColour (ledColour);
    g.fillEllipse (bounds);
    g.setColour (juce::Colour (0xaafffff0));
    g.fillEllipse (bounds.removeFromTop (bounds.getHeight() * 0.42f).reduced (2.0f, 1.0f));
}

void DroneLookAndFeel::drawHardwareButtonBody (juce::Graphics& g,
                                               juce::Rectangle<float> bounds,
                                               bool active,
                                               bool highlighted,
                                               bool down) const
{
    bounds = bounds.reduced (0.5f);
    const auto radius = juce::jmin (5.0f, bounds.getHeight() * 0.22f);
    const auto top = controlFaceTopColour (active, down);
    const auto bottom = controlFaceBottomColour (active, down);

    juce::ColourGradient face (top,
                               bounds.getCentreX(),
                               bounds.getY(),
                               bottom,
                               bounds.getCentreX(),
                               bounds.getBottom(),
                               false);
    face.addColour (0.48, lightMode_ ? juce::Colour (0xffe8e2d6) : juce::Colour (0xff050505));
    g.setGradientFill (face);
    g.fillRoundedRectangle (bounds, radius);

    g.setColour (lightMode_ ? juce::Colour (0xff716c62) : juce::Colour (0xff000000));
    g.drawRoundedRectangle (bounds, radius, 1.4f);
    g.setColour ((active ? juce::Colour (0xff746242) : (lightMode_ ? juce::Colour (0xff9a9488)
                                                                    : juce::Colour (0xff3a3328))).withAlpha (
        highlighted ? 0.95f : 0.74f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), radius, 1.0f);

    g.setColour (lightMode_ ? juce::Colour (0x9fffffff) : juce::Colour (0x66ffffff));
    g.drawLine (bounds.getX() + 4.0f,
                bounds.getY() + 2.0f,
                bounds.getRight() - 4.0f,
                bounds.getY() + 2.0f,
                0.8f);
}
} // namespace ui
