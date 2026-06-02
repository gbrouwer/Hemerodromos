#include "PostEffects.h"

void PostEffects::prepare (double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (maxBlockSize), 2 };
    resonatorL_.prepare (spec);
    resonatorR_.prepare (spec);
    resonatorL_.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    resonatorR_.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    reset();
}

void PostEffects::reset()
{
    reverb_.reset();
    resonatorL_.reset();
    resonatorR_.reset();
    roughPhase_ = 0.0f;
}

void PostEffects::process (juce::AudioBuffer<float>& buffer, const PostEffectParameters& parameters)
{
    const auto channels = buffer.getNumChannels();
    const auto samples = buffer.getNumSamples();
    if (channels == 0 || samples == 0)
        return;

    const auto resonatorAmount = juce::jlimit (0.0f, 1.0f, parameters.resonator);
    if (resonatorAmount > 0.001f)
    {
        const auto cutoff = 90.0f + 1900.0f * resonatorAmount * resonatorAmount;
        const auto resonance = 0.7f + 7.0f * resonatorAmount;
        resonatorL_.setCutoffFrequency (cutoff);
        resonatorR_.setCutoffFrequency (cutoff * 1.007f);
        resonatorL_.setResonance (resonance);
        resonatorR_.setResonance (resonance);

        auto* left = buffer.getWritePointer (0);
        if (channels > 1)
        {
            auto* right = buffer.getWritePointer (1);
            for (int i = 0; i < samples; ++i)
            {
                const auto wetL = resonatorL_.processSample (0, left[i]);
                const auto wetR = resonatorR_.processSample (0, right[i]);
                left[i] = juce::jmap (resonatorAmount, left[i], wetL);
                right[i] = juce::jmap (resonatorAmount, right[i], wetR);
            }
        }
        else
        {
            for (int i = 0; i < samples; ++i)
            {
                const auto wet = resonatorL_.processSample (0, left[i]);
                left[i] = juce::jmap (resonatorAmount, left[i], wet);
            }
        }
    }

    const auto roughness = juce::jlimit (0.0f, 1.0f, parameters.roughness);
    if (roughness > 0.001f)
    {
        auto* left = buffer.getWritePointer (0);
        if (channels > 1)
        {
            auto* right = buffer.getWritePointer (1);
            const auto step = 2.0f * juce::MathConstants<float>::pi * (0.17f + 1.7f * roughness)
                            / static_cast<float> (sampleRate_);
            for (int i = 0; i < samples; ++i)
            {
                roughPhase_ += step;
                if (roughPhase_ > juce::MathConstants<float>::twoPi)
                    roughPhase_ -= juce::MathConstants<float>::twoPi;

                const auto wobble = std::sin (roughPhase_) * roughness * 0.05f;
                left[i] *= 1.0f + wobble;
                right[i] *= 1.0f - wobble;
            }
        }
    }

    const auto mix = juce::jlimit (0.0f, 1.0f, parameters.reverbMix);
    if (mix > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = juce::jlimit (0.0f, 1.0f, parameters.reverbSize);
        rp.damping = 0.18f + 0.55f * (1.0f - parameters.reverbDecay);
        rp.wetLevel = mix * 0.55f;
        rp.dryLevel = 1.0f - mix * 0.28f;
        rp.width = 0.75f + 0.25f * parameters.stereoWidth;
        rp.freezeMode = 0.0f;
        reverb_.setParameters (rp);
        if (channels > 1)
            reverb_.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), samples);
        else
            reverb_.processMono (buffer.getWritePointer (0), samples);
    }

    const auto width = juce::jlimit (0.0f, 1.0f, parameters.stereoWidth);
    if (channels >= 2)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        for (int i = 0; i < samples; ++i)
        {
            const auto mid = 0.5f * (left[i] + right[i]);
            const auto side = 0.5f * (left[i] - right[i]) * (0.1f + 1.9f * width);
            left[i] = mid + side;
            right[i] = mid - side;
        }
    }
}
