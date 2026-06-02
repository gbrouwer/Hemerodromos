#pragma once

#include <JuceHeader.h>

struct PostEffectParameters
{
    float roughness = 0.0f;
    float resonator = 0.0f;
    float reverbMix = 0.0f;
    float reverbSize = 0.5f;
    float reverbDecay = 0.5f;
    float stereoWidth = 0.5f;
};

class PostEffects
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const PostEffectParameters& parameters);

private:
    double sampleRate_ = 44100.0;
    juce::Reverb reverb_;
    juce::dsp::StateVariableTPTFilter<float> resonatorL_;
    juce::dsp::StateVariableTPTFilter<float> resonatorR_;
    float roughPhase_ = 0.0f;
};
