#pragma once

#include <JuceHeader.h>

#include <array>

#include "DroneBank.h"
#include "PostEffects.h"
#include "ParameterIds.h"

struct LayerSynthParameters
{
    bool enabled = true;
    int bankIndex = 0;
    float gainDb = -12.0f;
    float tuneSemitones = 0.0f;
    float brightness = 0.0f;
    float motionDepth = 1.0f;
    float motionRate = 1.0f;
    float roughness = 0.0f;
    float macroOsc = 0.0f;
    float resonator = 0.0f;
    float reverbMix = 0.0f;
    float reverbSize = 0.0f;
    float reverbDecay = 0.0f;
    float stereoWidth = 0.35f;
    float attackSeconds = 1.0f;
    float releaseSeconds = 3.0f;
};

struct SynthParameters
{
    std::array<LayerSynthParameters, ParameterIds::layerCount> layers;
    bool latch = true;
};

class SynthEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize, int outputChannels);
    void reset();
    void loadBank (int layerIndex, const DroneBank& bank);
    void process (juce::AudioBuffer<float>& buffer,
                  const juce::MidiBuffer& midi,
                  const SynthParameters& parameters);

    const DroneBank* currentBank (int layerIndex) const noexcept;
    bool isGateOpen() const noexcept { return gateOpen_; }

private:
    struct LayerState
    {
        const DroneBank* bank = nullptr;
        std::vector<double> phases;
        double modelTimeSeconds = 0.0;
        float envelope = 0.0f;
        float macroPhase = 0.0f;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gain;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> tune;
        PostEffects postEffects;
    };

    double sampleRate_ = 44100.0;
    int maxBlockSize_ = 512;
    int outputChannels_ = 2;
    std::array<LayerState, ParameterIds::layerCount> layers_;
    juce::AudioBuffer<float> layerBuffer_;
    int activeMidiNote_ = 48;
    float velocity_ = 1.0f;
    bool gateOpen_ = true;

    void handleMidi (const juce::MidiBuffer& midi, const SynthParameters& parameters);
    void prepareLayerState (LayerState& layer, const DroneBank& bank);
    void renderLayer (LayerState& layer,
                      juce::AudioBuffer<float>& buffer,
                      const LayerSynthParameters& parameters);
    float evaluateEnvelope (LayerState& layer, const LayerSynthParameters& parameters) const noexcept;
    double evaluateAmpDb (const DronePartial& partial,
                          const DroneBank& bank,
                          double modelTime,
                          const LayerSynthParameters& parameters) const noexcept;
    double evaluateFrequencyLogRatio (const DronePartial& partial,
                                      const DroneBank& bank,
                                      double modelTime,
                                      const LayerSynthParameters& parameters) const noexcept;
};
