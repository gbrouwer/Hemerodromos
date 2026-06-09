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

juce::Colour panelText (bool lightMode)
{
    return lightMode ? juce::Colour (0xff20242a) : juce::Colour (0xfff1efe7);
}

void drawRackScrew (juce::Graphics& g, juce::Point<float> centre, bool lightMode)
{
    const auto radius = 8.0f;
    auto bounds = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (lightMode ? juce::Colour (0xffffffff)
                                                        : juce::Colour (0xff525a63),
                                             bounds.getX(),
                                             bounds.getY(),
                                             lightMode ? juce::Colour (0xff8e897e)
                                                        : juce::Colour (0xff171b20),
                                             bounds.getRight(),
                                             bounds.getBottom(),
                                             false));
    g.fillEllipse (bounds);
    g.setColour (lightMode ? juce::Colour (0xff6f6a60) : juce::Colour (0xff030405));
    g.drawEllipse (bounds, 1.1f);

    const auto slotColour = lightMode ? juce::Colour (0xff514d45) : juce::Colour (0xff050607);
    const auto highlight = lightMode ? juce::Colour (0x8fffffff) : juce::Colour (0x44ffffff);
    const auto slotLength = radius * 1.35f;
    const auto slotWidth = 2.2f;

    g.setColour (slotColour);
    g.drawLine (centre.x - slotLength * 0.5f, centre.y, centre.x + slotLength * 0.5f, centre.y, slotWidth);
    g.drawLine (centre.x, centre.y - slotLength * 0.5f, centre.x, centre.y + slotLength * 0.5f, slotWidth);
    g.setColour (highlight);
    g.drawLine (centre.x - slotLength * 0.5f, centre.y - 1.5f,
                centre.x + slotLength * 0.5f, centre.y - 1.5f, 0.7f);
    g.drawLine (centre.x - 1.5f, centre.y - slotLength * 0.5f,
                centre.x - 1.5f, centre.y + slotLength * 0.5f, 0.7f);
}

void drawRackPanel (juce::Graphics& g, juce::Rectangle<float> bounds, bool lightMode)
{
    g.fillAll (lightMode ? juce::Colour (0xffffffff) : juce::Colour (0xff07090b));

    auto panel = bounds;
    juce::ColourGradient face (lightMode ? juce::Colour (0xffffffff) : juce::Colour (0xff222831),
                               bounds.getCentreX(),
                               panel.getY(),
                               lightMode ? juce::Colour (0xfffbfbfb) : juce::Colour (0xff101419),
                               bounds.getCentreX(),
                               panel.getBottom(),
                               false);
    face.addColour (0.42, lightMode ? juce::Colour (0xffffffff) : juce::Colour (0xff171d24));
    g.setGradientFill (face);
    g.fillRect (panel);

    auto inner = bounds;
    g.setColour (lightMode ? juce::Colour (0x4cffffff) : juce::Colour (0x22ffffff));
    g.drawLine (inner.getX(), inner.getY(), inner.getRight(), inner.getY(), 1.0f);
    g.setColour (lightMode ? juce::Colour (0x39000000) : juce::Colour (0x88000000));
    g.drawLine (inner.getX(), inner.getBottom(), inner.getRight(), inner.getBottom(), 1.0f);

    const auto screwInset = 22.0f;
    drawRackScrew (g, { bounds.getX() + screwInset, bounds.getY() + screwInset }, lightMode);
    drawRackScrew (g, { bounds.getRight() - screwInset, bounds.getY() + screwInset }, lightMode);
    drawRackScrew (g, { bounds.getX() + screwInset, bounds.getBottom() - screwInset }, lightMode);
    drawRackScrew (g, { bounds.getRight() - screwInset, bounds.getBottom() - screwInset }, lightMode);
}

} // namespace

HemerodromosDroneAudioProcessorEditor::HemerodromosDroneAudioProcessorEditor (
    HemerodromosDroneAudioProcessor& processor)
    : AudioProcessorEditor (&processor), processor_ (processor)
{
    setLookAndFeel (&lookAndFeel_);
    setSize (1160, 700);

    titleLabel_.setText (JucePlugin_Name, juce::dontSendNotification);
    titleLabel_.setFont (lookAndFeel_.displayFont (25.0f, juce::Font::bold));
    titleLabel_.setColour (juce::Label::textColourId, juce::Colour (0xfff0f2f5));

    statusLabel_.setFont (lookAndFeel_.displayFont (13.0f));
    statusLabel_.setColour (juce::Label::textColourId, juce::Colour (0xffaeb6c2));

    brandLabel_.setText ("UMOJA", juce::dontSendNotification);
    brandLabel_.setJustificationType (juce::Justification::centred);
    brandLabel_.setFont (lookAndFeel_.displayFont (78.0f, juce::Font::bold));
    brandLabel_.setColour (juce::Label::textColourId, juce::Colour (0xffffffff));
    addAndMakeVisible (brandLabel_);

    modelLabel_.setText ("DR-001", juce::dontSendNotification);
    modelLabel_.setJustificationType (juce::Justification::centred);
    modelLabel_.setFont (lookAndFeel_.displayFont (51.0f, juce::Font::bold));
    modelLabel_.setColour (juce::Label::textColourId, juce::Colour (0xffffffff));
    addAndMakeVisible (modelLabel_);

    for (int layerIndex = 0; layerIndex < ParameterIds::layerCount; ++layerIndex)
    {
        auto& button = layerButtons_[static_cast<size_t> (layerIndex)];
        button.setButtonText (juce::String (layerIndex + 1));
        button.setTooltip ("Select layer " + juce::String (layerIndex + 1));
        button.getProperties().set ("hardwareLed", true);
        button.setClickingTogglesState (false);
        button.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff49f074));
        button.onClick = [this, layerIndex]
        {
            setSelectedLayer (layerIndex);
        };
        addAndMakeVisible (button);
    }

    layerEnabledButton_.setButtonText ("Enabled");
    layerEnabledButton_.setTooltip ("Enable the selected layer");
    layerEnabledButton_.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff49f074));
    layerEnabledButton_.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff49f074).withAlpha (0.25f));
    addAndMakeVisible (layerEnabledButton_);

    sourceStems_ = processor_.bankLibrary().getSourceStems();
    bankBox_.addItemList (processor_.bankLibrary().getSourceNames(), 1);
    bankBox_.onChange = [this]
    {
        if (refreshingBankControls_)
            return;

        rebuildVersionBoxForSelectedSource();
        applySelectedBankControls();
    };
    addAndMakeVisible (bankBox_);

    versionBox_.setTooltip ("Select fitted version for the selected drone");
    versionBox_.onChange = [this]
    {
        if (! refreshingBankControls_)
            applySelectedBankControls();
    };
    addAndMakeVisible (versionBox_);

    neutralButton_.setTooltip ("Reset selected layer knobs to neutral");
    neutralButton_.getProperties().set ("reserveLedSpace", true);
    neutralButton_.onClick = [this]
    {
        processor_.resetAllKnobParametersToNeutral (selectedLayer_);
    };
    addAndMakeVisible (neutralButton_);

    savePresetButton_.setTooltip ("Save all layer settings to a preset file");
    savePresetButton_.getProperties().set ("reserveLedSpace", true);
    savePresetButton_.onClick = [this]
    {
        savePreset();
    };
    addAndMakeVisible (savePresetButton_);

    loadPresetButton_.setTooltip ("Load layer settings from a preset file");
    loadPresetButton_.getProperties().set ("reserveLedSpace", true);
    loadPresetButton_.onClick = [this]
    {
        loadPreset();
    };
    addAndMakeVisible (loadPresetButton_);

    themeButton_.setTooltip ("Switch light/dark rack panel");
    themeButton_.getProperties().set ("reserveLedSpace", true);
    themeButton_.onClick = [this]
    {
        lightMode_ = ! lightMode_;
        applyTheme();
    };
    addAndMakeVisible (themeButton_);

    latchButton_.setButtonText ("Latch");
    latchButton_.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff49f074));
    latchButton_.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff49f074).withAlpha (0.25f));
    addAndMakeVisible (latchButton_);

    auto& state = processor_.state();
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

    applyTheme();
    setSelectedLayer (0);
}

HemerodromosDroneAudioProcessorEditor::~HemerodromosDroneAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void HemerodromosDroneAudioProcessorEditor::paint (juce::Graphics& g)
{
    drawRackPanel (g, getLocalBounds().toFloat(), lightMode_);
}

void HemerodromosDroneAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (28);
    auto controlRow = area.removeFromTop (62);
    auto layerArea = controlRow.removeFromLeft (132).reduced (2, 8);
    const auto layerButtonWidth = layerArea.getWidth() / ParameterIds::layerCount;
    for (auto& button : layerButtons_)
        button.setBounds (layerArea.removeFromLeft (layerButtonWidth).reduced (3, 0));

    layerEnabledButton_.setBounds (controlRow.removeFromLeft (92).reduced (6, 7));
    auto bankArea = controlRow.removeFromLeft (204).reduced (8, 0);
    bankArea.removeFromTop (18);
    bankArea.removeFromBottom (8);
    bankBox_.setBounds (bankArea);
    auto versionArea = controlRow.removeFromLeft (112).reduced (8, 0);
    versionArea.removeFromTop (18);
    versionArea.removeFromBottom (8);
    versionBox_.setBounds (versionArea);
    neutralButton_.setBounds (controlRow.removeFromLeft (96).reduced (6, 11));
    savePresetButton_.setBounds (controlRow.removeFromLeft (66).reduced (5, 11));
    loadPresetButton_.setBounds (controlRow.removeFromLeft (66).reduced (5, 11));
    themeButton_.setBounds (controlRow.removeFromLeft (76).reduced (5, 11));
    latchButton_.setBounds (controlRow.removeFromRight (78).reduced (6, 7));

    auto body = area.withTrimmedTop (18);
    constexpr auto knobRowHeight = 186;

    auto top = body.removeFromTop (knobRowHeight);
    ui::Knob* macroKnobs[] = { &gain_, &tune_, &brightness_, &motionDepth_, &motionRate_, &roughness_ };
    layoutKnobs (top, juce::Span<ui::Knob*> (macroKnobs), 6);

    auto middle = body.removeFromTop (knobRowHeight);
    ui::Knob* processKnobs[] = { &macroOsc_, &resonator_, &reverbMix_, &reverbSize_, &reverbDecay_, &stereoWidth_ };
    layoutKnobs (middle, juce::Span<ui::Knob*> (processKnobs), 6);

    auto bottom = body.removeFromTop (knobRowHeight);
    const auto columnWidth = bottom.getWidth() / 6;
    ui::Knob* envelopeKnobs[] = { &attack_, &release_ };
    layoutKnobs (bottom.removeFromLeft (columnWidth * 2), juce::Span<ui::Knob*> (envelopeKnobs), 2);
    auto brandArea = bottom.removeFromRight (columnWidth * 3).withTrimmedTop (18);
    brandLabel_.setBounds (brandArea.removeFromTop (96));
    modelLabel_.setBounds (brandArea.removeFromTop (64));
}

void HemerodromosDroneAudioProcessorEditor::addKnob (ui::Knob& knob, const char* parameterId)
{
    addAndMakeVisible (knob);
    knobs_.push_back (&knob);
    if (const auto* spec = findKnobSpec (parameterId))
        knob.setNeutralValue (spec->neutral);

    knob.setResetCallback ([this, parameterId]
    {
        processor_.resetLayerParameterToNeutral (selectedLayer_, parameterId);
    });

    knobBindings_.push_back ({ &knob, parameterId });
}

void HemerodromosDroneAudioProcessorEditor::layoutKnobs (juce::Rectangle<int> area,
                                                         juce::Span<ui::Knob*> knobs,
                                                         int columns)
{
    const auto width = area.getWidth() / columns;
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (area.removeFromLeft (width).reduced (8));
}

void HemerodromosDroneAudioProcessorEditor::setSelectedLayer (int layerIndex)
{
    const auto bounded = juce::jlimit (0, ParameterIds::layerCount - 1, layerIndex);
    if (bounded == selectedLayer_ && layerEnabledAttachment_ != nullptr)
        return;

    selectedLayer_ = bounded;
    updateLayerButtonStates();
    updateLayerTint();
    rebuildLayerAttachments();
}

void HemerodromosDroneAudioProcessorEditor::rebuildLayerAttachments()
{
    sliderAttachments_.clear();
    layerEnabledAttachment_.reset();

    auto& state = processor_.state();
    layerEnabledAttachment_ = std::make_unique<ButtonAttachment> (
        state,
        ParameterIds::layerParameterId (selectedLayer_, ParameterIds::enabled),
        layerEnabledButton_);

    for (const auto& binding : knobBindings_)
    {
        sliderAttachments_.push_back (std::make_unique<SliderAttachment> (
            state,
            ParameterIds::layerParameterId (selectedLayer_, binding.baseParameterId),
            binding.knob->slider()));
    }

    rebuildBankControlsFromParameter();
    updateStatusLabel();
}

void HemerodromosDroneAudioProcessorEditor::rebuildBankControlsFromParameter()
{
    juce::ScopedValueSetter<bool> scopedRefresh (refreshingBankControls_, true);
    const auto bankIndex = processor_.getLayerBankIndex (selectedLayer_);
    const auto& bank = processor_.bankLibrary().getBank (bankIndex);
    const auto sourceStem = bank.sourceStem.isNotEmpty() ? bank.sourceStem : bank.name;
    auto sourceIndex = sourceStems_.indexOf (sourceStem);
    if (sourceIndex < 0)
        sourceIndex = 0;

    bankBox_.setSelectedItemIndex (sourceIndex, juce::dontSendNotification);
    rebuildVersionBoxForSelectedSource();

    const auto profileLabel = processor_.bankLibrary().getProfileLabelsForSource (sourceStems_[sourceIndex]);
    auto selectedProfileLabel = bank.profileLabel;
    if (selectedProfileLabel.isEmpty())
        selectedProfileLabel = bank.fitVersion.isNotEmpty() ? bank.fitVersion.toUpperCase() : "Default";

    const auto profileIndex = profileLabel.indexOf (selectedProfileLabel);
    versionBox_.setSelectedItemIndex (profileIndex >= 0 ? profileIndex : 0, juce::dontSendNotification);
}

void HemerodromosDroneAudioProcessorEditor::rebuildVersionBoxForSelectedSource()
{
    const auto sourceIndex = juce::jlimit (0, sourceStems_.size() - 1, bankBox_.getSelectedItemIndex());
    const auto sourceStem = sourceStems_[sourceIndex];
    const auto labels = processor_.bankLibrary().getProfileLabelsForSource (sourceStem);
    const auto previousText = versionBox_.getText();

    versionBox_.clear (juce::dontSendNotification);
    versionBox_.addItemList (labels, 1);

    auto index = labels.indexOf (previousText);
    if (index < 0)
        index = 0;
    versionBox_.setSelectedItemIndex (index, juce::dontSendNotification);
}

void HemerodromosDroneAudioProcessorEditor::applySelectedBankControls()
{
    if (sourceStems_.isEmpty())
        return;

    const auto sourceIndex = juce::jlimit (0, sourceStems_.size() - 1, bankBox_.getSelectedItemIndex());
    const auto sourceStem = sourceStems_[sourceIndex];
    const auto profileLabel = versionBox_.getText().isNotEmpty() ? versionBox_.getText() : "Default";
    const auto bankIndex = processor_.bankLibrary().findBankIndex (sourceStem, profileLabel);
    processor_.setLayerBankIndex (selectedLayer_, bankIndex);
    updateStatusLabel();
}

void HemerodromosDroneAudioProcessorEditor::updateLayerButtonStates()
{
    for (int layerIndex = 0; layerIndex < ParameterIds::layerCount; ++layerIndex)
    {
        auto& button = layerButtons_[static_cast<size_t> (layerIndex)];
        const auto selected = layerIndex == selectedLayer_;
        button.setColour (juce::TextButton::buttonColourId, lightMode_ ? juce::Colour (0xfff7f8f8)
                                                                        : juce::Colour (0xff101010));
        button.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff49f074));
        button.setColour (juce::TextButton::textColourOffId, panelText (lightMode_));
        button.setColour (juce::TextButton::textColourOnId, panelText (lightMode_));
        button.setToggleState (selected, juce::dontSendNotification);
        button.repaint();
    }
}

void HemerodromosDroneAudioProcessorEditor::updateLayerTint()
{
    const auto colour = layerColour (selectedLayer_);
    for (auto* knob : knobs_)
    {
        knob->setAccentColour (colour);
        knob->setLightMode (lightMode_);
    }

    layerEnabledButton_.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff49f074));
    layerEnabledButton_.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff49f074).withAlpha (0.25f));
    neutralButton_.setColour (juce::TextButton::buttonColourId, lightMode_ ? juce::Colour (0xfff7f8f8)
                                                                            : juce::Colour (0xff101010));
    neutralButton_.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff49f074));
    neutralButton_.setColour (juce::TextButton::textColourOffId, panelText (lightMode_));
    neutralButton_.setColour (juce::TextButton::textColourOnId, panelText (lightMode_));
    bankBox_.setColour (juce::ComboBox::outlineColourId, lightMode_ ? juce::Colour (0xff928b7d)
                                                                     : juce::Colour (0xff3a3429));
    bankBox_.setColour (juce::ComboBox::textColourId, panelText (lightMode_));
    versionBox_.setColour (juce::ComboBox::outlineColourId, lightMode_ ? juce::Colour (0xff928b7d)
                                                                        : juce::Colour (0xff3a3429));
    versionBox_.setColour (juce::ComboBox::textColourId, panelText (lightMode_));

    for (auto* button : { &savePresetButton_, &loadPresetButton_, &themeButton_ })
    {
        button->setColour (juce::TextButton::buttonColourId, lightMode_ ? juce::Colour (0xfff7f8f8)
                                                                         : juce::Colour (0xff101010));
        button->setColour (juce::TextButton::textColourOffId, panelText (lightMode_));
        button->setColour (juce::TextButton::textColourOnId, panelText (lightMode_));
        button->repaint();
    }
    neutralButton_.repaint();
    repaint();
}

void HemerodromosDroneAudioProcessorEditor::applyTheme()
{
    lookAndFeel_.setLightMode (lightMode_);
    const auto text = panelText (lightMode_);
    const auto muted = lightMode_ ? juce::Colour (0xff575e66) : juce::Colour (0xffaeb6c2);
    themeButton_.setButtonText (lightMode_ ? "Dark" : "Light");

    titleLabel_.setColour (juce::Label::textColourId, text);
    statusLabel_.setColour (juce::Label::textColourId, muted);
    brandLabel_.setColour (juce::Label::textColourId, text);
    modelLabel_.setColour (juce::Label::textColourId, text);

    layerEnabledButton_.setColour (juce::ToggleButton::textColourId, text);
    latchButton_.setColour (juce::ToggleButton::textColourId, text);
    bankBox_.setColour (juce::ComboBox::textColourId, text);
    versionBox_.setColour (juce::ComboBox::textColourId, text);

    for (auto* knob : knobs_)
        knob->setLightMode (lightMode_);

    updateLayerButtonStates();
    updateLayerTint();
    repaint();
}

void HemerodromosDroneAudioProcessorEditor::updateStatusLabel()
{
    auto bankIndex = processor_.getLayerBankIndex (selectedLayer_);
    if (! sourceStems_.isEmpty() && bankBox_.getSelectedItemIndex() >= 0)
    {
        const auto sourceIndex = juce::jlimit (0, sourceStems_.size() - 1, bankBox_.getSelectedItemIndex());
        const auto profileLabel = versionBox_.getText().isNotEmpty() ? versionBox_.getText() : "Default";
        bankIndex = processor_.bankLibrary().findBankIndex (sourceStems_[sourceIndex], profileLabel);
    }

    const auto* bank = bankIndex >= 0 && bankIndex < processor_.bankLibrary().size()
        ? &processor_.bankLibrary().getBank (bankIndex)
        : processor_.currentBank (selectedLayer_);

    if (bank != nullptr)
    {
        statusLabel_.setText ("Layer " + juce::String (selectedLayer_ + 1)
                                  + "  Partials: " + juce::String (static_cast<int> (bank->partials.size()))
                                  + "  Model: " + bank->modelType
                                  + "  Latched drone",
                              juce::dontSendNotification);
    }
    else
    {
        statusLabel_.setText ("Layer " + juce::String (selectedLayer_ + 1)
                                  + "  Embedded fitted banks",
                              juce::dontSendNotification);
    }
}

void HemerodromosDroneAudioProcessorEditor::savePreset()
{
    auto presetDir = processor_.defaultPresetDirectory();
    presetDir.createDirectory();
    const auto defaultFile = presetDir.getChildFile ("Hemerodromos Drone.hmdpreset");

    presetChooser_ = std::make_unique<juce::FileChooser> (
        "Save Hemerodromos preset",
        defaultFile,
        "*.hmdpreset;*.json");

    presetChooser_->launchAsync (
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file == juce::File())
                return;

            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension (".hmdpreset");

            const auto saved = processor_.savePresetToFile (file);
            statusLabel_.setText (
                saved ? "Preset saved: " + file.getFileName() : "Preset save failed",
                juce::dontSendNotification);
        });
}

void HemerodromosDroneAudioProcessorEditor::loadPreset()
{
    auto presetDir = processor_.defaultPresetDirectory();
    presetDir.createDirectory();

    presetChooser_ = std::make_unique<juce::FileChooser> (
        "Load Hemerodromos preset",
        presetDir,
        "*.hmdpreset;*.json");

    presetChooser_->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file == juce::File())
                return;

            const auto loaded = processor_.loadPresetFromFile (file);
            if (loaded)
                rebuildBankControlsFromParameter();
            statusLabel_.setText (
                loaded ? "Preset loaded: " + file.getFileName() : "Preset load failed",
                juce::dontSendNotification);
        });
}

juce::Colour HemerodromosDroneAudioProcessorEditor::layerColour (int layerIndex) noexcept
{
    static constexpr std::array<juce::uint32, ParameterIds::layerCount> colours {
        0xffd7b46a,
        0xff70c7d4,
        0xffd989a6,
        0xff9cc978,
    };
    return juce::Colour (colours[static_cast<size_t> (
        juce::jlimit (0, ParameterIds::layerCount - 1, layerIndex))]);
}
