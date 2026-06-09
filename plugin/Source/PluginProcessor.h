#pragma once

#include <JuceHeader.h>

#include "DroneBank.h"
#include "ParameterIds.h"
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
    const DroneBank* currentBank (int layerIndex) const noexcept { return synth_.currentBank (layerIndex); }
    int getLayerBankIndex (int layerIndex) const;
    bool setLayerBankIndex (int layerIndex, int bankIndex);
    void resetLayerParameterToNeutral (int layerIndex, const char* baseParameterId);
    void resetAllKnobParametersToNeutral (int layerIndex);
    bool savePresetToFile (const juce::File& file) const;
    bool loadPresetFromFile (const juce::File& file);
    juce::File defaultPresetDirectory() const;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts_;
    DroneBankLibrary bankLibrary_;
    SynthEngine synth_;
    std::array<int, ParameterIds::layerCount> currentBankIndices_ {};

    SynthParameters readParameters() const;
    void updateBankIfNeeded (int layerIndex, int bankIndex);
    void updateBanksIfNeeded (const SynthParameters& parameters);
    float getParameterPlainValue (const juce::String& parameterId) const;
    bool setParameterPlainValue (const juce::String& parameterId, float value);
};
