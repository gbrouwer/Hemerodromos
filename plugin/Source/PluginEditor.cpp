#include "PluginEditor.h"

#include "ParameterIds.h"

namespace
{
const ParameterIds::KnobParameterSpec* findKnobSpec (const char* parameterId) noexcept
{
    for (const auto& spec : ParameterIds::knobParameterSpecs)
        if (juce::StringRef (parameterId) == juce::StringRef (spec.id))
            return &spec;

    return nullptr;
}
} // namespace

HemerodromosDroneAudioProcessorEditor::HemerodromosDroneAudioProcessorEditor (
    HemerodromosDroneAudioProcessor& processor)
    : AudioProcessorEditor (&processor), processor_ (processor)
{
    setLookAndFeel (&lookAndFeel_);
    setSize (980, 620);

    titleLabel_.setText ("Hemerodromos Drone", juce::dontSendNotification);
    titleLabel_.setFont (juce::FontOptions (25.0f, juce::Font::bold));
    titleLabel_.setColour (juce::Label::textColourId, juce::Colour (0xfff0f2f5));
    addAndMakeVisible (titleLabel_);

    statusLabel_.setFont (juce::FontOptions (13.0f));
    statusLabel_.setColour (juce::Label::textColourId, juce::Colour (0xffaeb6c2));
    addAndMakeVisible (statusLabel_);

    bankBox_.addItemList (processor_.bankLibrary().getNames(), 1);
    bankBox_.setSelectedItemIndex (3, juce::dontSendNotification);
    addAndMakeVisible (bankBox_);

    neutralButton_.setTooltip ("Reset knobs to neutral");
    neutralButton_.onClick = [this]
    {
        processor_.resetAllKnobParametersToNeutral();
    };
    addAndMakeVisible (neutralButton_);

    latchButton_.setButtonText ("Latch");
    addAndMakeVisible (latchButton_);

    auto& state = processor_.state();
    bankAttachment_ = std::make_unique<ComboAttachment> (state, ParameterIds::bank, bankBox_);
    latchAttachment_ = std::make_unique<ButtonAttachment> (state, ParameterIds::latch, latchButton_);

    addKnob (gain_, ParameterIds::gain);
    addKnob (tune_, ParameterIds::tune);
    addKnob (brightness_, ParameterIds::brightness);
    addKnob (motionDepth_, ParameterIds::motionDepth);
    addKnob (motionRate_, ParameterIds::motionRate);
    addKnob (roughness_, ParameterIds::roughness);
    addKnob (macroOsc_, ParameterIds::macroOsc);
    addKnob (resonator_, ParameterIds::resonator);
    addKnob (reverbMix_, ParameterIds::reverbMix);
    addKnob (reverbSize_, ParameterIds::reverbSize);
    addKnob (reverbDecay_, ParameterIds::reverbDecay);
    addKnob (stereoWidth_, ParameterIds::stereoWidth);
    addKnob (attack_, ParameterIds::attack);
    addKnob (release_, ParameterIds::release);

    if (const auto* bank = processor_.currentBank())
        statusLabel_.setText (juce::String ("Partials: ") + juce::String (static_cast<int> (bank->partials.size()))
                                  + "  Model: " + bank->modelType,
                              juce::dontSendNotification);
    else
        statusLabel_.setText ("Embedded fitted banks", juce::dontSendNotification);
}

HemerodromosDroneAudioProcessorEditor::~HemerodromosDroneAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void HemerodromosDroneAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff101318), bounds.getTopLeft(),
                                             juce::Colour (0xff1c2027), bounds.getBottomRight(), false));
    g.fillAll();

    g.setColour (juce::Colour (0xff272d35));
    g.fillRoundedRectangle (bounds.reduced (18.0f).withY (78.0f), 8.0f);
    g.setColour (juce::Colour (0xff3a414c));
    g.drawRoundedRectangle (bounds.reduced (18.0f).withY (78.0f), 8.0f, 1.0f);

    g.setColour (juce::Colour (0xffd7b46a));
    g.drawHorizontalLine (70, 28.0f, static_cast<float> (getWidth() - 28));
}

void HemerodromosDroneAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (26);
    auto header = area.removeFromTop (58);
    titleLabel_.setBounds (header.removeFromLeft (360));
    bankBox_.setBounds (header.removeFromLeft (260).reduced (8, 12));
    neutralButton_.setBounds (header.removeFromLeft (104).reduced (6, 14));
    latchButton_.setBounds (header.removeFromRight (90).reduced (6, 14));
    statusLabel_.setBounds (header.reduced (10, 12));

    auto body = area.reduced (14);
    auto top = body.removeFromTop (150);
    ui::Knob* macroKnobs[] = { &gain_, &tune_, &brightness_, &motionDepth_, &motionRate_, &roughness_ };
    layoutKnobs (top, juce::Span<ui::Knob*> (macroKnobs), 6);

    auto middle = body.removeFromTop (150);
    ui::Knob* processKnobs[] = { &macroOsc_, &resonator_, &reverbMix_, &reverbSize_, &reverbDecay_, &stereoWidth_ };
    layoutKnobs (middle, juce::Span<ui::Knob*> (processKnobs), 6);

    auto bottom = body.removeFromTop (150);
    ui::Knob* envelopeKnobs[] = { &attack_, &release_ };
    layoutKnobs (bottom.removeFromLeft (330), juce::Span<ui::Knob*> (envelopeKnobs), 2);
}

void HemerodromosDroneAudioProcessorEditor::addKnob (ui::Knob& knob, const char* parameterId)
{
    addAndMakeVisible (knob);
    knobs_.push_back (&knob);
    if (const auto* spec = findKnobSpec (parameterId))
        knob.setNeutralValue (spec->neutral);

    knob.setResetCallback ([this, parameterId = juce::String (parameterId)]
    {
        processor_.resetParameterToNeutral (parameterId);
    });

    sliderAttachments_.push_back (
        std::make_unique<SliderAttachment> (processor_.state(), parameterId, knob.slider()));
}

void HemerodromosDroneAudioProcessorEditor::layoutKnobs (juce::Rectangle<int> area,
                                                         juce::Span<ui::Knob*> knobs,
                                                         int columns)
{
    const auto width = area.getWidth() / columns;
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (area.removeFromLeft (width).reduced (8));
}
