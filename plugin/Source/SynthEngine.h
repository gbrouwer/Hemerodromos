#pragma once

#include <JuceHeader.h>

#include "DroneBank.h"
#include "PostEffects.h"

#ifndef HEMERODROMOS_TRIGGERED_INSTRUMENT
#define HEMERODROMOS_TRIGGERED_INSTRUMENT 0
#endif

struct SynthParameters
{
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
    bool latch = HEMERODROMOS_TRIGGERED_INSTRUMENT == 0;
};

class SynthEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize, int outputChannels);
    void reset();
    void loadBank (const DroneBank& bank);
    void process (juce::AudioBuffer<float>& buffer,
                  const juce::MidiBuffer& midi,
                  const SynthParameters& parameters);

    const DroneBank* currentBank() const noexcept { return bank_; }
    bool isGateOpen() const noexcept { return gateOpen_; }

private:
    static constexpr bool defaultGateOpen() noexcept { return HEMERODROMOS_TRIGGERED_INSTRUMENT == 0; }

    const DroneBank* bank_ = nullptr;
    double sampleRate_ = 44100.0;
    int outputChannels_ = 2;
    std::vector<double> phases_;
    double modelTimeSeconds_ = 0.0;
    int activeMidiNote_ = 48;
    float velocity_ = 1.0f;
    bool gateOpen_ = defaultGateOpen();
    float envelope_ = 0.0f;
    float macroPhase_ = 0.0f;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gain_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> tune_;
    PostEffects postEffects_;

    void handleMidi (const juce::MidiBuffer& midi, const SynthParameters& parameters);
    float evaluateEnvelope (const SynthParameters& parameters) noexcept;
    double evaluateAmpDb (const DronePartial& partial,
                          double modelTime,
                          const SynthParameters& parameters) const noexcept;
};
