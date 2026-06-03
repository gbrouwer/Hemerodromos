#include "PluginProcessor.h"

#include "ParameterIds.h"
#include "PluginEditor.h"

namespace
{
const ParameterIds::KnobParameterSpec* findKnobSpec (const juce::String& parameterId) noexcept
{
    for (const auto& spec : ParameterIds::knobParameterSpecs)
        if (parameterId == spec.id)
            return &spec;

    return nullptr;
}
} // namespace

HemerodromosDroneAudioProcessor::HemerodromosDroneAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_ (*this, nullptr, "Parameters", createParameterLayout())
{
}

void HemerodromosDroneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth_.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    currentBankIndex_ = -1;
    updateBankIfNeeded (readParameters().bankIndex);
}

void HemerodromosDroneAudioProcessor::releaseResources()
{
    synth_.reset();
}

bool HemerodromosDroneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto main = layouts.getMainOutputChannelSet();
    return main == juce::AudioChannelSet::mono() || main == juce::AudioChannelSet::stereo();
}

void HemerodromosDroneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const auto parameters = readParameters();
    updateBankIfNeeded (parameters.bankIndex);
    synth_.process (buffer, midiMessages, parameters);
}

juce::AudioProcessorEditor* HemerodromosDroneAudioProcessor::createEditor()
{
    return new HemerodromosDroneAudioProcessorEditor (*this);
}

void HemerodromosDroneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts_.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HemerodromosDroneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts_.state.getType()))
            apvts_.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout
HemerodromosDroneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIds::bank, 1 },
        "Bank",
        juce::StringArray { "Drone 1", "Drone 2", "Drone 3", "Drone 4", "Drone 5" },
        3));

    for (const auto& spec : ParameterIds::knobParameterSpecs)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { spec.id, 1 },
            spec.name,
            juce::NormalisableRange<float> { spec.minimum, spec.maximum, spec.interval },
            spec.neutral,
            juce::AudioParameterFloatAttributes().withLabel (spec.unit)));
    }

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIds::latch, 1 },
        "Latch",
        HEMERODROMOS_TRIGGERED_INSTRUMENT == 0));

    return { params.begin(), params.end() };
}

SynthParameters HemerodromosDroneAudioProcessor::readParameters() const
{
    auto value = [this] (const char* id)
    {
        return apvts_.getRawParameterValue (id)->load();
    };

    SynthParameters params;
    params.bankIndex = static_cast<int> (std::round (value (ParameterIds::bank)));
    params.gainDb = value (ParameterIds::gain);
    params.tuneSemitones = value (ParameterIds::tune);
    params.brightness = value (ParameterIds::brightness);
    params.motionDepth = value (ParameterIds::motionDepth);
    params.motionRate = value (ParameterIds::motionRate);
    params.roughness = value (ParameterIds::roughness);
    params.macroOsc = value (ParameterIds::macroOsc);
    params.resonator = value (ParameterIds::resonator);
    params.reverbMix = value (ParameterIds::reverbMix);
    params.reverbSize = value (ParameterIds::reverbSize);
    params.reverbDecay = value (ParameterIds::reverbDecay);
    params.stereoWidth = value (ParameterIds::stereoWidth);
    params.attackSeconds = value (ParameterIds::attack);
    params.releaseSeconds = value (ParameterIds::release);
    params.latch = value (ParameterIds::latch) >= 0.5f;
    return params;
}

void HemerodromosDroneAudioProcessor::updateBankIfNeeded (int bankIndex)
{
    const auto bounded = juce::jlimit (0, bankLibrary_.size() - 1, bankIndex);
    if (bounded == currentBankIndex_)
        return;

    synth_.loadBank (bankLibrary_.getBank (bounded));
    currentBankIndex_ = bounded;
}

void HemerodromosDroneAudioProcessor::resetParameterToNeutral (const juce::String& parameterId)
{
    const auto* spec = findKnobSpec (parameterId);
    if (spec == nullptr)
        return;

    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts_.getParameter (parameterId)))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (spec->neutral));
        parameter->endChangeGesture();
    }
}

void HemerodromosDroneAudioProcessor::resetAllKnobParametersToNeutral()
{
    for (const auto& spec : ParameterIds::knobParameterSpecs)
        resetParameterToNeutral (spec.id);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HemerodromosDroneAudioProcessor();
}
