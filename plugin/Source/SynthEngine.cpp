#include "SynthEngine.h"

namespace
{
float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db / 20.0f);
}

double midiRatio (int midiNote, float tuneSemitones) noexcept
{
    return std::pow (2.0, (static_cast<double> (midiNote - 48) + tuneSemitones) / 12.0);
}
} // namespace

void SynthEngine::prepare (double sampleRate, int maxBlockSize, int outputChannels)
{
    sampleRate_ = sampleRate;
    outputChannels_ = juce::jmax (1, outputChannels);
    gain_.reset (sampleRate, 0.03);
    tune_.reset (sampleRate, 0.05);
    postEffects_.prepare (sampleRate, maxBlockSize);
    reset();
}

void SynthEngine::reset()
{
    std::fill (phases_.begin(), phases_.end(), 0.0);
    modelTimeSeconds_ = 0.0;
    envelope_ = 0.0f;
    macroPhase_ = 0.0f;
    postEffects_.reset();
}

void SynthEngine::loadBank (const DroneBank& bank)
{
    bank_ = &bank;
    phases_.assign (bank.partials.size(), 0.0);
    for (size_t i = 0; i < bank.partials.size(); ++i)
        phases_[i] = bank.partials[i].phaseRadians;
    modelTimeSeconds_ = 0.0;
}

void SynthEngine::process (juce::AudioBuffer<float>& buffer,
                           const juce::MidiBuffer& midi,
                           const SynthParameters& parameters)
{
    buffer.clear();
    if (bank_ == nullptr || ! bank_->isValid())
        return;

    handleMidi (midi, parameters);
    gain_.setTargetValue (dbToGain (parameters.gainDb));
    tune_.setTargetValue (parameters.tuneSemitones);

    const auto samples = buffer.getNumSamples();
    const auto channels = buffer.getNumChannels();
    const auto partialCount = static_cast<int> (bank_->partials.size());

    for (int sample = 0; sample < samples; ++sample)
    {
        const auto env = evaluateEnvelope (parameters);
        const auto gain = gain_.getNextValue() * env * velocity_;
        const auto ratio = midiRatio (activeMidiNote_, tune_.getNextValue());
        const auto modelTime = std::fmod (modelTimeSeconds_, bank_->durationSeconds);
        float left = 0.0f;
        float right = 0.0f;

        for (int i = 0; i < partialCount; ++i)
        {
            const auto& partial = bank_->partials[static_cast<size_t> (i)];
            const auto frequency = partial.frequencyHz * ratio;
            if (frequency >= sampleRate_ * 0.48)
                continue;

            const auto ampDb = evaluateAmpDb (partial, modelTime, parameters);
            const auto amp = static_cast<float> (std::pow (10.0, ampDb / 20.0));
            const auto value = amp * std::sin (phases_[static_cast<size_t> (i)]);
            const auto pan = 0.5f + 0.5f * std::sin (static_cast<float> (i) * 2.39996323f);
            const auto spread = 0.5f + (pan - 0.5f) * parameters.stereoWidth;
            left += value * std::cos (spread * juce::MathConstants<float>::halfPi);
            right += value * std::sin (spread * juce::MathConstants<float>::halfPi);

            phases_[static_cast<size_t> (i)] += juce::MathConstants<double>::twoPi * frequency / sampleRate_;
            if (phases_[static_cast<size_t> (i)] > juce::MathConstants<double>::twoPi)
                phases_[static_cast<size_t> (i)] -= juce::MathConstants<double>::twoPi;
        }

        const auto macroAmount = std::abs (parameters.macroOsc);
        if (macroAmount > 0.001f)
        {
            macroPhase_ += static_cast<float> (juce::MathConstants<double>::twoPi
                         * (parameters.macroOsc >= 0.0f ? 55.0 : 27.5) * ratio / sampleRate_);
            if (macroPhase_ > juce::MathConstants<float>::twoPi)
                macroPhase_ -= juce::MathConstants<float>::twoPi;

            const auto osc = parameters.macroOsc >= 0.0f
                ? std::sin (macroPhase_)
                    + 0.35f * std::sin (2.01f * macroPhase_)
                    + 0.18f * std::sin (3.98f * macroPhase_)
                : 0.9f * std::sin (macroPhase_)
                    + 0.25f * std::sin (1.51f * macroPhase_)
                    - 0.12f * std::sin (2.63f * macroPhase_);
            left += osc * macroAmount * (parameters.macroOsc >= 0.0f ? 0.24f : 0.20f);
            right += osc * macroAmount * (parameters.macroOsc >= 0.0f ? 0.22f : 0.17f);
        }

        buffer.setSample (0, sample, left * gain);
        if (channels > 1)
            buffer.setSample (1, sample, right * gain);
    }

    modelTimeSeconds_ += static_cast<double> (samples) / sampleRate_ * parameters.motionRate;
    if (modelTimeSeconds_ > bank_->durationSeconds)
        modelTimeSeconds_ = std::fmod (modelTimeSeconds_, bank_->durationSeconds);

    postEffects_.process (buffer, { parameters.roughness,
                                    parameters.resonator,
                                    parameters.reverbMix,
                                    parameters.reverbSize,
                                    parameters.reverbDecay,
                                    parameters.stereoWidth });
}

void SynthEngine::handleMidi (const juce::MidiBuffer& midi, const SynthParameters& parameters)
{
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            activeMidiNote_ = message.getNoteNumber();
            velocity_ = message.getFloatVelocity();
            gateOpen_ = true;
        }
        else if (message.isNoteOff() && message.getNoteNumber() == activeMidiNote_)
        {
            gateOpen_ = parameters.latch;
        }
    }
}

float SynthEngine::evaluateEnvelope (const SynthParameters& parameters) noexcept
{
    const auto attackSamples = juce::jmax (1.0f, parameters.attackSeconds * static_cast<float> (sampleRate_));
    const auto releaseSamples =
        juce::jmax (1.0f, parameters.releaseSeconds * static_cast<float> (sampleRate_));

    if (gateOpen_)
        envelope_ += (1.0f - envelope_) / attackSamples;
    else
        envelope_ -= envelope_ / releaseSamples;

    envelope_ = juce::jlimit (0.0f, 1.0f, envelope_);
    return envelope_;
}

double SynthEngine::evaluateAmpDb (const DronePartial& partial,
                                   double modelTime,
                                   const SynthParameters& parameters) const noexcept
{
    auto ampDb = partial.amplitudeDb;
    const auto pivotHz = 700.0;
    const auto brightness = static_cast<double> (parameters.brightness);
    ampDb += brightness * 9.0 * std::log2 (juce::jmax (partial.frequencyHz, 20.0) / pivotHz);

    const auto coeffCount = static_cast<int> (partial.amplitudeCoefficients.size());
    const auto order = coeffCount / 2;
    auto motionDb = 0.0;
    for (int harmonic = 1; harmonic <= order; ++harmonic)
    {
        const auto angle = juce::MathConstants<double>::twoPi
                         * static_cast<double> (harmonic) * modelTime / bank_->durationSeconds;
        motionDb += partial.amplitudeCoefficients[static_cast<size_t> ((harmonic - 1) * 2)] * std::sin (angle)
                  + partial.amplitudeCoefficients[static_cast<size_t> ((harmonic - 1) * 2 + 1)] * std::cos (angle);
    }
    const auto motionDepth = juce::jlimit (0.0, 2.0, static_cast<double> (parameters.motionDepth));
    const auto effectiveMotionDepth = motionDepth <= 1.0 ? motionDepth : 1.0 + (motionDepth - 1.0) * 3.0;
    ampDb += motionDb * effectiveMotionDepth;

    const auto roughness = juce::jlimit (-1.0, 1.0, static_cast<double> (parameters.roughness));
    const auto roughnessAmount = std::abs (roughness);
    if (roughnessAmount > 0.001)
    {
        const auto contour = roughness >= 0.0
            ? std::sin (partial.frequencyHz * 0.017)
            : std::cos (partial.frequencyHz * 0.007 + 1.3);
        ampDb += roughnessAmount * 4.0 * contour;
    }

    return ampDb;
}
