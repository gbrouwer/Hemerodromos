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

int readIntProperty (const juce::DynamicObject& object, const juce::Identifier& name, int fallback)
{
    const auto value = object.getProperty (name);
    return value.isInt() || value.isDouble() ? static_cast<int> (value) : fallback;
}

float readFloatProperty (const juce::DynamicObject& object, const juce::Identifier& name, float fallback)
{
    const auto value = object.getProperty (name);
    return value.isInt() || value.isDouble() ? static_cast<float> (value) : fallback;
}

int bankIndexForPresetValue (const DroneBankLibrary& library,
                             int fallbackIndex,
                             const juce::String& bankName)
{
    const auto names = library.getNames();
    const auto nameIndex = names.indexOf (bankName);
    if (nameIndex >= 0)
        return nameIndex;

    return juce::jlimit (0, library.size() - 1, fallbackIndex);
}
} // namespace

HemerodromosDroneAudioProcessor::HemerodromosDroneAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_ (*this, nullptr, "Parameters", createParameterLayout())
{
    currentBankIndices_.fill (-1);
}

void HemerodromosDroneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth_.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    updateBanksIfNeeded (readParameters());
}

void HemerodromosDroneAudioProcessor::releaseResources()
{
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
    updateBanksIfNeeded (parameters);
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
    const auto bankNames = DroneBankLibrary::getEmbeddedChoiceNames();
    const auto maxDefaultBank = juce::jmax (0, bankNames.size() - 1);

    for (int layerIndex = 0; layerIndex < ParameterIds::layerCount; ++layerIndex)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParameterIds::layerParameterId (layerIndex, ParameterIds::enabled), 1 },
            ParameterIds::layerParameterName (layerIndex, "Enabled"),
            layerIndex == 0));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParameterIds::layerParameterId (layerIndex, ParameterIds::bank), 1 },
            ParameterIds::layerParameterName (layerIndex, "Bank"),
            bankNames,
            juce::jlimit (0, maxDefaultBank, layerIndex)));

        for (const auto& spec : ParameterIds::knobParameterSpecs)
        {
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { ParameterIds::layerParameterId (layerIndex, spec.id), 1 },
                ParameterIds::layerParameterName (layerIndex, spec.name),
                juce::NormalisableRange<float> { spec.minimum, spec.maximum, spec.interval },
                spec.neutral,
                juce::AudioParameterFloatAttributes().withLabel (spec.unit)));
        }
    }

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIds::latch, 1 },
        "Latch",
        true));

    return { params.begin(), params.end() };
}

SynthParameters HemerodromosDroneAudioProcessor::readParameters() const
{
    auto value = [this] (const juce::String& id)
    {
        return apvts_.getRawParameterValue (id)->load();
    };

    SynthParameters params;
    for (int layerIndex = 0; layerIndex < ParameterIds::layerCount; ++layerIndex)
    {
        auto& layer = params.layers[static_cast<size_t> (layerIndex)];
        auto layerValue = [&value, layerIndex] (const char* baseId)
        {
            return value (ParameterIds::layerParameterId (layerIndex, baseId));
        };

        layer.enabled = layerValue (ParameterIds::enabled) >= 0.5f;
        layer.bankIndex = static_cast<int> (std::round (layerValue (ParameterIds::bank)));
        layer.gainDb = layerValue (ParameterIds::gain);
        layer.tuneSemitones = layerValue (ParameterIds::tune);
        layer.brightness = layerValue (ParameterIds::brightness);
        layer.motionDepth = layerValue (ParameterIds::motionDepth);
        layer.motionRate = layerValue (ParameterIds::motionRate);
        layer.roughness = layerValue (ParameterIds::roughness);
        layer.macroOsc = layerValue (ParameterIds::macroOsc);
        layer.resonator = layerValue (ParameterIds::resonator);
        layer.reverbMix = layerValue (ParameterIds::reverbMix);
        layer.reverbSize = layerValue (ParameterIds::reverbSize);
        layer.reverbDecay = layerValue (ParameterIds::reverbDecay);
        layer.stereoWidth = layerValue (ParameterIds::stereoWidth);
        layer.attackSeconds = layerValue (ParameterIds::attack);
        layer.releaseSeconds = layerValue (ParameterIds::release);
    }
    params.latch = value (ParameterIds::latch) >= 0.5f;
    return params;
}

void HemerodromosDroneAudioProcessor::updateBankIfNeeded (int layerIndex, int bankIndex)
{
    const auto bounded = juce::jlimit (0, bankLibrary_.size() - 1, bankIndex);
    auto& currentBankIndex = currentBankIndices_[static_cast<size_t> (layerIndex)];
    if (bounded == currentBankIndex && synth_.currentBank (layerIndex) != nullptr)
        return;

    synth_.loadBank (layerIndex, bankLibrary_.getBank (bounded));
    currentBankIndex = bounded;
}

void HemerodromosDroneAudioProcessor::updateBanksIfNeeded (const SynthParameters& parameters)
{
    for (int layerIndex = 0; layerIndex < ParameterIds::layerCount; ++layerIndex)
        updateBankIfNeeded (layerIndex, parameters.layers[static_cast<size_t> (layerIndex)].bankIndex);
}

void HemerodromosDroneAudioProcessor::resetLayerParameterToNeutral (int layerIndex,
                                                                    const char* baseParameterId)
{
    const auto* spec = findKnobSpec (baseParameterId);
    if (spec == nullptr)
        return;

    const auto parameterId = ParameterIds::layerParameterId (layerIndex, baseParameterId);
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts_.getParameter (parameterId)))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (spec->neutral));
        parameter->endChangeGesture();
    }
}

int HemerodromosDroneAudioProcessor::getLayerBankIndex (int layerIndex) const
{
    return juce::jlimit (
        0,
        bankLibrary_.size() - 1,
        static_cast<int> (std::round (getParameterPlainValue (
            ParameterIds::layerParameterId (layerIndex, ParameterIds::bank)))));
}

bool HemerodromosDroneAudioProcessor::setLayerBankIndex (int layerIndex, int bankIndex)
{
    return setParameterPlainValue (
        ParameterIds::layerParameterId (layerIndex, ParameterIds::bank),
        static_cast<float> (juce::jlimit (0, bankLibrary_.size() - 1, bankIndex)));
}

void HemerodromosDroneAudioProcessor::resetAllKnobParametersToNeutral (int layerIndex)
{
    for (const auto& spec : ParameterIds::knobParameterSpecs)
        resetLayerParameterToNeutral (layerIndex, spec.id);
}

bool HemerodromosDroneAudioProcessor::savePresetToFile (const juce::File& file) const
{
    auto outputFile = file;
    if (outputFile.getFileExtension().isEmpty())
        outputFile = outputFile.withFileExtension (".hmdpreset");

    if (! outputFile.getParentDirectory().createDirectory())
        return false;

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("schema", "hemerodromos.drone.preset.v1");
    root->setProperty ("plugin", JucePlugin_Name);
    root->setProperty ("saved_at", juce::Time::getCurrentTime().toISO8601 (true));

    auto global = std::make_unique<juce::DynamicObject>();
    global->setProperty ("latch", getParameterPlainValue (ParameterIds::latch) >= 0.5f);
    root->setProperty ("global", juce::var (global.release()));

    juce::Array<juce::var> layers;
    const auto bankNames = bankLibrary_.getNames();
    for (int layerIndex = 0; layerIndex < ParameterIds::layerCount; ++layerIndex)
    {
        auto layer = std::make_unique<juce::DynamicObject>();
        const auto enabled = getParameterPlainValue (
            ParameterIds::layerParameterId (layerIndex, ParameterIds::enabled)) >= 0.5f;
        const auto bankIndex = juce::jlimit (
            0,
            bankLibrary_.size() - 1,
            static_cast<int> (std::round (getParameterPlainValue (
                ParameterIds::layerParameterId (layerIndex, ParameterIds::bank)))));

        layer->setProperty ("index", layerIndex + 1);
        layer->setProperty ("enabled", enabled);
        layer->setProperty ("bank_index", bankIndex);
        layer->setProperty ("bank_name", bankNames[bankIndex]);

        auto parameters = std::make_unique<juce::DynamicObject>();
        for (const auto& spec : ParameterIds::knobParameterSpecs)
        {
            parameters->setProperty (
                spec.id,
                getParameterPlainValue (ParameterIds::layerParameterId (layerIndex, spec.id)));
        }
        layer->setProperty ("parameters", juce::var (parameters.release()));
        layers.add (juce::var (layer.release()));
    }
    root->setProperty ("layers", juce::var (layers));

    return outputFile.replaceWithText (juce::JSON::toString (juce::var (root.release()), true));
}

bool HemerodromosDroneAudioProcessor::loadPresetFromFile (const juce::File& file)
{
    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    if (! parsed.isObject())
        return false;

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return false;

    if (auto* global = root->getProperty ("global").getDynamicObject())
    {
        const auto latchValue = global->getProperty ("latch");
        if (latchValue.isBool())
            setParameterPlainValue (ParameterIds::latch, static_cast<bool> (latchValue) ? 1.0f : 0.0f);
    }

    if (auto* layers = root->getProperty ("layers").getArray())
    {
        for (int presetLayerIndex = 0; presetLayerIndex < layers->size(); ++presetLayerIndex)
        {
            auto* layer = layers->getReference (presetLayerIndex).getDynamicObject();
            if (layer == nullptr)
                continue;

            const auto layerIndex = juce::jlimit (
                0,
                ParameterIds::layerCount - 1,
                readIntProperty (*layer, "index", presetLayerIndex + 1) - 1);

            const auto enabled = layer->getProperty ("enabled");
            if (enabled.isBool())
            {
                setParameterPlainValue (
                    ParameterIds::layerParameterId (layerIndex, ParameterIds::enabled),
                    static_cast<bool> (enabled) ? 1.0f : 0.0f);
            }

            const auto bankIndex = bankIndexForPresetValue (
                bankLibrary_,
                readIntProperty (*layer, "bank_index", 0),
                layer->getProperty ("bank_name").toString());
            setParameterPlainValue (
                ParameterIds::layerParameterId (layerIndex, ParameterIds::bank),
                static_cast<float> (bankIndex));

            if (auto* parameters = layer->getProperty ("parameters").getDynamicObject())
            {
                for (const auto& spec : ParameterIds::knobParameterSpecs)
                {
                    const auto parameterValue = parameters->getProperty (spec.id);
                    if (parameterValue.isDouble() || parameterValue.isInt())
                    {
                        setParameterPlainValue (
                            ParameterIds::layerParameterId (layerIndex, spec.id),
                            readFloatProperty (*parameters, spec.id, spec.neutral));
                    }
                }
            }
        }
    }

    updateBanksIfNeeded (readParameters());
    return true;
}

juce::File HemerodromosDroneAudioProcessor::defaultPresetDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Hemerodromos")
        .getChildFile ("Presets");
}

float HemerodromosDroneAudioProcessor::getParameterPlainValue (const juce::String& parameterId) const
{
    if (const auto* value = apvts_.getRawParameterValue (parameterId))
        return value->load();

    return 0.0f;
}

bool HemerodromosDroneAudioProcessor::setParameterPlainValue (const juce::String& parameterId, float value)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts_.getParameter (parameterId)))
    {
        const auto normalized = parameter->convertTo0to1 (value);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (normalized);
        parameter->endChangeGesture();
        return true;
    }

    return false;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HemerodromosDroneAudioProcessor();
}
