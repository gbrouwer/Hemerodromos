#include "PluginProcessor.h"

#include "ParameterIds.h"
#include "PluginEditor.h"

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

    auto makeFloat = [&params] (const juce::String& id,
                                const juce::String& name,
                                juce::NormalisableRange<float> range,
                                float defaultValue,
                                const juce::String& unit = {})
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, defaultValue,
            juce::AudioParameterFloatAttributes().withLabel (unit)));
    };

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIds::bank, 1 },
        "Bank",
        juce::StringArray { "Drone 1", "Drone 2", "Drone 3", "Drone 4", "Drone 5" },
        3));

    makeFloat (ParameterIds::gain, "Gain", { -60.0f, 6.0f, 0.01f }, -16.0f, "dB");
    makeFloat (ParameterIds::tune, "Tune", { -24.0f, 24.0f, 0.01f }, 0.0f, "st");
    makeFloat (ParameterIds::brightness, "Brightness", { 0.0f, 1.0f, 0.001f }, 0.5f);
    makeFloat (ParameterIds::motionDepth, "Motion Depth", { 0.0f, 1.5f, 0.001f }, 0.65f);
    makeFloat (ParameterIds::motionRate, "Motion Rate", { 0.05f, 4.0f, 0.001f }, 1.0f, "x");
    makeFloat (ParameterIds::roughness, "Roughness", { 0.0f, 1.0f, 0.001f }, 0.1f);
    makeFloat (ParameterIds::macroOsc, "Macro Osc", { 0.0f, 1.0f, 0.001f }, 0.0f);
    makeFloat (ParameterIds::resonator, "Resonator", { 0.0f, 1.0f, 0.001f }, 0.0f);
    makeFloat (ParameterIds::reverbMix, "Plate Space", { 0.0f, 1.0f, 0.001f }, 0.28f);
    makeFloat (ParameterIds::reverbSize, "Space Size", { 0.0f, 1.0f, 0.001f }, 0.72f);
    makeFloat (ParameterIds::reverbDecay, "Space Decay", { 0.0f, 1.0f, 0.001f }, 0.65f);
    makeFloat (ParameterIds::stereoWidth, "Width", { 0.0f, 1.0f, 0.001f }, 0.75f);
    makeFloat (ParameterIds::attack, "Attack", { 0.001f, 10.0f, 0.001f }, 1.0f, "s");
    makeFloat (ParameterIds::release, "Release", { 0.01f, 30.0f, 0.001f }, 3.0f, "s");
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIds::latch, 1 }, "Latch", true));

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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HemerodromosDroneAudioProcessor();
}
