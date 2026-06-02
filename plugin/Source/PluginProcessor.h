#pragma once

#include <JuceHeader.h>

#include "DroneBank.h"
#include "SynthEngine.h"

class HemerodromosDroneAudioProcessor final : public juce::AudioProcessor
{
public:
    HemerodromosDroneAudioProcessor();
    ~HemerodromosDroneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return apvts_; }
    DroneBankLibrary& bankLibrary() noexcept { return bankLibrary_; }
    const DroneBank* currentBank() const noexcept { return synth_.currentBank(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts_;
    DroneBankLibrary bankLibrary_;
    SynthEngine synth_;
    int currentBankIndex_ = -1;

    SynthParameters readParameters() const;
    void updateBankIfNeeded (int bankIndex);
};
