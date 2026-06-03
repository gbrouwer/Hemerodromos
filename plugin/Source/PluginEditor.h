#pragma once

#include <JuceHeader.h>

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
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    HemerodromosDroneAudioProcessor& processor_;
    ui::DroneLookAndFeel lookAndFeel_;

    juce::ComboBox bankBox_;
    juce::Label titleLabel_;
    juce::Label statusLabel_;
    juce::TextButton neutralButton_ { "Neutral" };
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

    std::vector<ui::Knob*> knobs_;
    std::unique_ptr<ComboAttachment> bankAttachment_;
    std::unique_ptr<ButtonAttachment> latchAttachment_;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments_;

    void addKnob (ui::Knob& knob, const char* parameterId);
    void layoutKnobs (juce::Rectangle<int> area, juce::Span<ui::Knob*> knobs, int columns);
};
