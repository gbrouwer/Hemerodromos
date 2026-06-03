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

    const auto resonatorValue = juce::jlimit (-1.0f, 1.0f, parameters.resonator);
    const auto resonatorAmount = std::abs (resonatorValue);
    if (resonatorAmount > 0.001f)
    {
        const auto cutoff = resonatorValue >= 0.0f
            ? 220.0f + 2400.0f * resonatorAmount * resonatorAmount
            : 55.0f + 500.0f * resonatorAmount * resonatorAmount;
        const auto resonance = 0.7f + 7.0f * resonatorAmount;
        resonatorL_.setCutoffFrequency (cutoff);
        resonatorR_.setCutoffFrequency (cutoff * (resonatorValue >= 0.0f ? 1.007f : 0.993f));
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

    const auto roughnessValue = juce::jlimit (-1.0f, 1.0f, parameters.roughness);
    const auto roughnessAmount = std::abs (roughnessValue);
    if (roughnessAmount > 0.001f)
    {
        auto* left = buffer.getWritePointer (0);
        const auto rate = roughnessValue >= 0.0f
            ? 0.17f + 1.7f * roughnessAmount
            : 0.05f + 0.55f * roughnessAmount;
        const auto step = 2.0f * juce::MathConstants<float>::pi * rate / static_cast<float> (sampleRate_);
        if (channels > 1)
        {
            auto* right = buffer.getWritePointer (1);
            for (int i = 0; i < samples; ++i)
            {
                roughPhase_ += step;
                if (roughPhase_ > juce::MathConstants<float>::twoPi)
                    roughPhase_ -= juce::MathConstants<float>::twoPi;

                const auto wobble = std::sin (roughPhase_) * roughnessAmount;
                if (roughnessValue >= 0.0f)
                {
                    left[i] *= 1.0f + wobble * 0.05f;
                    right[i] *= 1.0f - wobble * 0.05f;
                }
                else
                {
                    const auto flutter = 1.0f + wobble * 0.08f;
                    left[i] = std::tanh (left[i] * flutter * (1.0f + 0.18f * roughnessAmount))
                            / (1.0f + 0.05f * roughnessAmount);
                    right[i] = std::tanh (right[i] * flutter * (1.0f + 0.18f * roughnessAmount))
                             / (1.0f + 0.05f * roughnessAmount);
                }
            }
        }
        else
        {
            for (int i = 0; i < samples; ++i)
            {
                roughPhase_ += step;
                if (roughPhase_ > juce::MathConstants<float>::twoPi)
                    roughPhase_ -= juce::MathConstants<float>::twoPi;

                const auto wobble = std::sin (roughPhase_) * roughnessAmount;
                if (roughnessValue >= 0.0f)
                    left[i] *= 1.0f + wobble * 0.04f;
                else
                    left[i] = std::tanh (left[i] * (1.0f + wobble * 0.08f)
                                       * (1.0f + 0.18f * roughnessAmount))
                            / (1.0f + 0.05f * roughnessAmount);
            }
        }
    }

    const auto space = juce::jlimit (-1.0f, 1.0f, parameters.reverbMix);
    const auto sizeOffset = juce::jlimit (-1.0f, 1.0f, parameters.reverbSize);
    const auto decayOffset = juce::jlimit (-1.0f, 1.0f, parameters.reverbDecay);
    const auto mix = juce::jlimit (0.0f, 1.0f, std::abs (space)
                                                + 0.18f * juce::jmax (std::abs (sizeOffset),
                                                                      std::abs (decayOffset)));
    if (mix > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = juce::jlimit (0.0f, 1.0f, 0.55f + 0.35f * sizeOffset
                                                  + (space >= 0.0f ? 0.15f : -0.10f) * std::abs (space));
        rp.damping = space >= 0.0f
            ? juce::jlimit (0.0f, 1.0f, 0.22f + 0.35f * (1.0f - (0.55f + 0.40f * decayOffset)))
            : juce::jlimit (0.0f, 1.0f, 0.55f + 0.35f * std::abs (space));
        rp.wetLevel = mix * (space >= 0.0f ? 0.55f : 0.42f);
        rp.dryLevel = 1.0f - mix * 0.18f;
        rp.width = juce::jlimit (0.0f, 1.0f, 0.55f + 0.45f * (std::abs (space)
                                       + 0.5f * parameters.stereoWidth / 0.70f));
        rp.freezeMode = 0.0f;
        reverb_.setParameters (rp);
        if (channels > 1)
            reverb_.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), samples);
        else
            reverb_.processMono (buffer.getWritePointer (0), samples);
    }

    const auto width = juce::jlimit (0.0f, 0.70f, parameters.stereoWidth);
    if (channels >= 2)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        const auto sideScale = 0.1f + 1.8f * (width / 0.70f);
        for (int i = 0; i < samples; ++i)
        {
            const auto mid = 0.5f * (left[i] + right[i]);
            const auto side = 0.5f * (left[i] - right[i]) * sideScale;
            left[i] = mid + side;
            right[i] = mid - side;
        }
    }
}
