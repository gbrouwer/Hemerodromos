#pragma once

#include <JuceHeader.h>

#include <array>

#include "PluginProcessor.h"
#include "UI/DroneLookAndFeel.h"
#include "UI/Knob.h"

class HemerodromosDroneAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit HemerodromosDroneAudioProcessorEditor (HemerodromosDroneAudioProcessor&);
    ~HemerodromosDroneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    HemerodromosDroneAudioProcessor& processor_;
    ui::DroneLookAndFeel lookAndFeel_;

    std::array<juce::TextButton, ParameterIds::layerCount> layerButtons_;
    juce::ToggleButton layerEnabledButton_;
    juce::ComboBox bankBox_;
    juce::ComboBox versionBox_;
    juce::Label titleLabel_;
    juce::Label statusLabel_;
    juce::Label brandLabel_;
    juce::Label modelLabel_;
    juce::TextButton neutralButton_ { "Neutral" };
    juce::TextButton savePresetButton_ { "Save" };
    juce::TextButton loadPresetButton_ { "Load" };
    juce::TextButton themeButton_ { "Dark" };
    juce::ToggleButton latchButton_;

    ui::Knob gain_ { "Gain", "dB" };
    ui::Knob tune_ { "Tune", "st" };
    ui::Knob brightness_ { "Brightness" };
    ui::Knob motionDepth_ { "Motion" };
    ui::Knob motionRate_ { "Rate", "x" };
    ui::Knob roughness_ { "Roughness" };
    ui::Knob macroOsc_ { "Macro Osc" };
    ui::Knob resonator_ { "Resonator" };
    ui::Knob reverbMix_ { "Plate Space" };
    ui::Knob reverbSize_ { "Size" };
    ui::Knob reverbDecay_ { "Decay" };
    ui::Knob stereoWidth_ { "Width" };
    ui::Knob attack_ { "Attack", "s" };
    ui::Knob release_ { "Release", "s" };

    struct KnobBinding
    {
        ui::Knob* knob = nullptr;
        const char* baseParameterId = nullptr;
    };

    int selectedLayer_ = 0;
    bool refreshingBankControls_ = false;
    bool lightMode_ = true;
    std::vector<ui::Knob*> knobs_;
    std::vector<KnobBinding> knobBindings_;
    std::unique_ptr<ButtonAttachment> layerEnabledAttachment_;
    std::unique_ptr<ButtonAttachment> latchAttachment_;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments_;
    std::unique_ptr<juce::FileChooser> presetChooser_;
    juce::StringArray sourceStems_;

    void addKnob (ui::Knob& knob, const char* parameterId);
    void layoutKnobs (juce::Rectangle<int> area, juce::Span<ui::Knob*> knobs, int columns);
    void setSelectedLayer (int layerIndex);
    void rebuildLayerAttachments();
    void rebuildBankControlsFromParameter();
    void rebuildVersionBoxForSelectedSource();
    void applySelectedBankControls();
    void updateLayerButtonStates();
    void updateLayerTint();
    void applyTheme();
    void updateStatusLabel();
    void savePreset();
    void loadPreset();
    static juce::Colour layerColour (int layerIndex) noexcept;
};
