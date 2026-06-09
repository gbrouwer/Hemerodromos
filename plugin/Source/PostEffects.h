#pragma once

#include <JuceHeader.h>

struct PostEffectParameters
{
    float roughness = 0.0f;
    float resonator = 0.0f;
    float reverbMix = 0.0f;
    float reverbSize = 0.0f;
    float reverbDecay = 0.0f;
    float stereoWidth = 0.35f;
};

class PostEffects
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const PostEffectParameters& parameters);

private:
    double sampleRate_ = 44100.0;
    int maxBlockSize_ = 0;
    bool prepared_ = false;
    juce::Reverb reverb_;
    juce::dsp::StateVariableTPTFilter<float> resonatorL_;
    juce::dsp::StateVariableTPTFilter<float> resonatorR_;
    float roughPhase_ = 0.0f;
};
