#include "SynthEngine.h"

#include <algorithm>

namespace
{
constexpr double parameterSmoothingSeconds = 0.005;

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
    maxBlockSize_ = juce::jmax (1, maxBlockSize);
    outputChannels_ = juce::jmax (1, outputChannels);
    layerBuffer_.setSize (outputChannels_, maxBlockSize_, false, false, true);

    for (auto& layer : layers_)
    {
        layer.gain.reset (sampleRate, parameterSmoothingSeconds);
        layer.tune.reset (sampleRate, parameterSmoothingSeconds);
        layer.postEffects.prepare (sampleRate, maxBlockSize_);
        if (layer.bank != nullptr && layer.phases.size() != layer.bank->partials.size())
            prepareLayerState (layer, *layer.bank);
    }
}

void SynthEngine::reset()
{
    gateOpen_ = true;
    activeMidiNote_ = 48;
    velocity_ = 1.0f;

    for (auto& layer : layers_)
    {
        if (layer.bank != nullptr)
        {
            layer.phases.assign (layer.bank->partials.size(), 0.0);
            for (size_t i = 0; i < layer.bank->partials.size(); ++i)
                layer.phases[i] = layer.bank->partials[i].phaseRadians;
        }
        else
        {
            layer.phases.clear();
        }

        layer.modelTimeSeconds = 0.0;
        layer.envelope = 0.0f;
        layer.macroPhase = 0.0f;
        layer.gain.reset (sampleRate_, parameterSmoothingSeconds);
        layer.tune.reset (sampleRate_, parameterSmoothingSeconds);
        layer.postEffects.reset();
    }
}

void SynthEngine::loadBank (int layerIndex, const DroneBank& bank)
{
    prepareLayerState (layers_[static_cast<size_t> (juce::jlimit (0, ParameterIds::layerCount - 1, layerIndex))],
                       bank);
}

const DroneBank* SynthEngine::currentBank (int layerIndex) const noexcept
{
    return layers_[static_cast<size_t> (juce::jlimit (0, ParameterIds::layerCount - 1, layerIndex))].bank;
}

void SynthEngine::prepareLayerState (LayerState& layer, const DroneBank& bank)
{
    layer.bank = &bank;
    layer.phases.assign (bank.partials.size(), 0.0);
    for (size_t i = 0; i < bank.partials.size(); ++i)
        layer.phases[i] = bank.partials[i].phaseRadians;

    layer.modelTimeSeconds = 0.0;
    layer.envelope = 0.0f;
    layer.macroPhase = 0.0f;
    layer.postEffects.reset();
}

void SynthEngine::process (juce::AudioBuffer<float>& buffer,
                           const juce::MidiBuffer& midi,
                           const SynthParameters& parameters)
{
    buffer.clear();
    handleMidi (midi, parameters);

    const auto samples = buffer.getNumSamples();
    const auto channels = buffer.getNumChannels();
    if (samples <= 0 || channels <= 0)
        return;

    if (layerBuffer_.getNumChannels() < channels || layerBuffer_.getNumSamples() < samples)
        layerBuffer_.setSize (channels, samples, false, false, true);

    for (int layerIndex = 0; layerIndex < ParameterIds::layerCount; ++layerIndex)
    {
        const auto& layerParameters = parameters.layers[static_cast<size_t> (layerIndex)];
        auto& layer = layers_[static_cast<size_t> (layerIndex)];
        if (! layerParameters.enabled || layer.bank == nullptr || ! layer.bank->isValid())
            continue;

        layerBuffer_.setSize (channels, samples, false, false, true);
        layerBuffer_.clear();
        renderLayer (layer, layerBuffer_, layerParameters);
        for (int channel = 0; channel < channels; ++channel)
            buffer.addFrom (channel, 0, layerBuffer_, channel, 0, samples);
    }
}

void SynthEngine::renderLayer (LayerState& layer,
                               juce::AudioBuffer<float>& buffer,
                               const LayerSynthParameters& parameters)
{
    auto& bank = *layer.bank;
    layer.gain.setTargetValue (dbToGain (parameters.gainDb));
    layer.tune.setTargetValue (parameters.tuneSemitones);

    const auto samples = buffer.getNumSamples();
    const auto channels = buffer.getNumChannels();
    const auto partialCount = static_cast<int> (bank.partials.size());
    const auto fittedWidth = juce::jlimit (0.0f, 0.70f, static_cast<float> (bank.stereoWidth));
    const auto effectiveStereoWidth = juce::jlimit (
        0.0f,
        0.70f,
        fittedWidth + (parameters.stereoWidth - 0.35f));
    const auto usesFrequencyMotion = std::any_of (
        bank.partials.begin(),
        bank.partials.end(),
        [] (const DronePartial& partial)
        {
            return ! partial.frequencyLogRatioCoefficients.empty();
        });

    for (int sample = 0; sample < samples; ++sample)
    {
        const auto env = evaluateEnvelope (layer, parameters);
        const auto gain = layer.gain.getNextValue() * env * velocity_;
        const auto ratio = midiRatio (activeMidiNote_, layer.tune.getNextValue());
        const auto modelTime = std::fmod (layer.modelTimeSeconds, bank.durationSeconds);
        float left = 0.0f;
        float right = 0.0f;

        for (int i = 0; i < partialCount; ++i)
        {
            const auto& partial = bank.partials[static_cast<size_t> (i)];
            const auto frequency = partial.frequencyHz * ratio
                                 * std::pow (2.0, evaluateFrequencyLogRatio (partial, bank, modelTime, parameters));
            if (frequency >= sampleRate_ * 0.48)
                continue;

            const auto ampDb = evaluateAmpDb (partial, bank, modelTime, parameters);
            const auto amp = static_cast<float> (std::pow (10.0, ampDb / 20.0));
            const auto value = amp * std::sin (layer.phases[static_cast<size_t> (i)]);
            const auto pan = 0.5f + 0.5f * std::sin (static_cast<float> (i) * 2.39996323f);
            const auto spread = 0.5f + (pan - 0.5f) * effectiveStereoWidth;
            left += value * std::cos (spread * juce::MathConstants<float>::halfPi);
            right += value * std::sin (spread * juce::MathConstants<float>::halfPi);

            layer.phases[static_cast<size_t> (i)] += juce::MathConstants<double>::twoPi
                                                   * frequency / sampleRate_;
            if (layer.phases[static_cast<size_t> (i)] > juce::MathConstants<double>::twoPi)
                layer.phases[static_cast<size_t> (i)] -= juce::MathConstants<double>::twoPi;
        }

        const auto macroAmount = std::abs (parameters.macroOsc);
        if (macroAmount > 0.001f)
        {
            layer.macroPhase += static_cast<float> (juce::MathConstants<double>::twoPi
                              * (parameters.macroOsc >= 0.0f ? 55.0 : 27.5) * ratio / sampleRate_);
            if (layer.macroPhase > juce::MathConstants<float>::twoPi)
                layer.macroPhase -= juce::MathConstants<float>::twoPi;

            const auto osc = parameters.macroOsc >= 0.0f
                ? std::sin (layer.macroPhase)
                    + 0.35f * std::sin (2.01f * layer.macroPhase)
                    + 0.18f * std::sin (3.98f * layer.macroPhase)
                : 0.9f * std::sin (layer.macroPhase)
                    + 0.25f * std::sin (1.51f * layer.macroPhase)
                    - 0.12f * std::sin (2.63f * layer.macroPhase);
            left += osc * macroAmount * (parameters.macroOsc >= 0.0f ? 0.24f : 0.20f);
            right += osc * macroAmount * (parameters.macroOsc >= 0.0f ? 0.22f : 0.17f);
        }

        if (channels == 1)
        {
            buffer.setSample (0, sample, 0.5f * (left + right) * gain);
        }
        else
        {
            buffer.setSample (0, sample, left * gain);
            buffer.setSample (1, sample, right * gain);
            for (int channel = 2; channel < channels; ++channel)
                buffer.setSample (channel, sample, 0.5f * (left + right) * gain);
        }

        if (usesFrequencyMotion)
        {
            layer.modelTimeSeconds += (1.0 / sampleRate_) * static_cast<double> (parameters.motionRate);
            if (layer.modelTimeSeconds > bank.durationSeconds)
                layer.modelTimeSeconds = std::fmod (layer.modelTimeSeconds, bank.durationSeconds);
        }
    }

    if (! usesFrequencyMotion)
    {
        layer.modelTimeSeconds += static_cast<double> (samples) / sampleRate_
                                * static_cast<double> (parameters.motionRate);
        if (layer.modelTimeSeconds > bank.durationSeconds)
            layer.modelTimeSeconds = std::fmod (layer.modelTimeSeconds, bank.durationSeconds);
    }

    layer.postEffects.process (buffer, { parameters.roughness,
                                         parameters.resonator,
                                         parameters.reverbMix,
                                         parameters.reverbSize,
                                         parameters.reverbDecay,
                                         effectiveStereoWidth });
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

float SynthEngine::evaluateEnvelope (LayerState& layer,
                                     const LayerSynthParameters& parameters) const noexcept
{
    const auto attackSamples = juce::jmax (1.0f, parameters.attackSeconds * static_cast<float> (sampleRate_));
    const auto releaseSamples =
        juce::jmax (1.0f, parameters.releaseSeconds * static_cast<float> (sampleRate_));

    if (gateOpen_)
        layer.envelope += (1.0f - layer.envelope) / attackSamples;
    else
        layer.envelope -= layer.envelope / releaseSamples;

    layer.envelope = juce::jlimit (0.0f, 1.0f, layer.envelope);
    return layer.envelope;
}

double SynthEngine::evaluateAmpDb (const DronePartial& partial,
                                   const DroneBank& bank,
                                   double modelTime,
                                   const LayerSynthParameters& parameters) const noexcept
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
                         * static_cast<double> (harmonic) * modelTime / bank.durationSeconds;
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

double SynthEngine::evaluateFrequencyLogRatio (const DronePartial& partial,
                                               const DroneBank& bank,
                                               double modelTime,
                                               const LayerSynthParameters& parameters) const noexcept
{
    const auto coeffCount = static_cast<int> (partial.frequencyLogRatioCoefficients.size());
    const auto order = coeffCount / 2;
    auto logRatio = 0.0;
    for (int harmonic = 1; harmonic <= order; ++harmonic)
    {
        const auto angle = juce::MathConstants<double>::twoPi
                         * static_cast<double> (harmonic) * modelTime / bank.durationSeconds;
        logRatio += partial.frequencyLogRatioCoefficients[static_cast<size_t> ((harmonic - 1) * 2)]
                    * std::sin (angle)
                  + partial.frequencyLogRatioCoefficients[static_cast<size_t> ((harmonic - 1) * 2 + 1)]
                    * std::cos (angle);
    }

    const auto motionDepth = juce::jlimit (0.0, 2.0, static_cast<double> (parameters.motionDepth));
    return logRatio * motionDepth;
}
