#include "DroneBank.h"

#include "BinaryData.h"

namespace
{
double dbToAmplitude (double db) noexcept
{
    return std::pow (10.0, db / 20.0);
}

double readDouble (const juce::var& value, double fallback)
{
    return value.isDouble() || value.isInt() ? static_cast<double> (value) : fallback;
}

int readInt (const juce::var& value, int fallback)
{
    return value.isInt() ? static_cast<int> (value) : fallback;
}

std::vector<double> readDoubleArray (const juce::var& value)
{
    std::vector<double> result;
    if (auto* array = value.getArray())
    {
        result.reserve (static_cast<size_t> (array->size()));
        for (const auto& item : *array)
            result.push_back (readDouble (item, 0.0));
    }
    return result;
}
} // namespace

DroneBankLibrary::DroneBankLibrary()
{
    banks_.push_back (parseBank (BinaryData::drone_base1_fit_v001_dronebank_json,
                                 BinaryData::drone_base1_fit_v001_dronebank_jsonSize));
    banks_.push_back (parseBank (BinaryData::drone_base2_fit_v001_dronebank_json,
                                 BinaryData::drone_base2_fit_v001_dronebank_jsonSize));
    banks_.push_back (parseBank (BinaryData::drone_base3_fit_v001_dronebank_json,
                                 BinaryData::drone_base3_fit_v001_dronebank_jsonSize));
    banks_.push_back (parseBank (BinaryData::drone_base4_fit_v001_dronebank_json,
                                 BinaryData::drone_base4_fit_v001_dronebank_jsonSize));
    banks_.push_back (parseBank (BinaryData::drone_base5_fit_v001_dronebank_json,
                                 BinaryData::drone_base5_fit_v001_dronebank_jsonSize));
    banks_.push_back (parseBank (BinaryData::drone_base6_fit_v001_dronebank_json,
                                 BinaryData::drone_base6_fit_v001_dronebank_jsonSize));
    banks_.push_back (parseBank (BinaryData::drone_base7_fit_v001_dronebank_json,
                                 BinaryData::drone_base7_fit_v001_dronebank_jsonSize));
    banks_.push_back (parseBank (BinaryData::drone_base8_fit_v001_dronebank_json,
                                 BinaryData::drone_base8_fit_v001_dronebank_jsonSize));
    banks_.push_back (parseBank (BinaryData::drone_base9_fit_v001_dronebank_json,
                                 BinaryData::drone_base9_fit_v001_dronebank_jsonSize));
    banks_.push_back (parseBank (BinaryData::drone_base10_fit_v001_dronebank_json,
                                 BinaryData::drone_base10_fit_v001_dronebank_jsonSize));
    banks_.push_back (parseBank (BinaryData::drone_base11_fit_v001_dronebank_json,
                                 BinaryData::drone_base11_fit_v001_dronebank_jsonSize));
    banks_.push_back (parseBank (BinaryData::drone_base12_fit_v001_dronebank_json,
                                 BinaryData::drone_base12_fit_v001_dronebank_jsonSize));

    banks_.erase (std::remove_if (banks_.begin(), banks_.end(), [] (const DroneBank& bank)
                                  { return ! bank.isValid(); }),
                  banks_.end());

    if (banks_.empty())
    {
        DroneBank fallback;
        fallback.name = "Fallback";
        fallback.modelType = "sine";
        fallback.durationSeconds = 1.0;
        fallback.partials.push_back ({ 110.0, 0.0, -12.0, {} });
        banks_.push_back (std::move (fallback));
    }
}

const DroneBank& DroneBankLibrary::getBank (int index) const noexcept
{
    const auto bounded = juce::jlimit (0, size() - 1, index);
    return banks_[static_cast<size_t> (bounded)];
}

juce::StringArray DroneBankLibrary::getNames() const
{
    juce::StringArray names;
    for (const auto& bank : banks_)
        names.add (bank.name.isNotEmpty() ? bank.name : "Drone");
    return names;
}

DroneBank DroneBankLibrary::parseBank (const char* data, int size)
{
    DroneBank bank;
    const juce::String json (juce::CharPointer_UTF8 (data), static_cast<size_t> (size));
    const auto parsed = juce::JSON::parse (json);
    if (! parsed.isObject())
        return bank;

    auto* object = parsed.getDynamicObject();
    if (object == nullptr)
        return bank;

    bank.name = object->getProperty ("name").toString();
    bank.modelType = object->getProperty ("model_type").toString();
    bank.analysisSampleRate = readInt (object->getProperty ("analysis_sample_rate"), 44100);
    bank.durationSeconds = readDouble (object->getProperty ("duration_seconds"), 1.0);

    if (auto* basis = object->getProperty ("basis").getDynamicObject())
    {
        bank.basisType = basis->getProperty ("type").toString();
        bank.basisOrder = readInt (basis->getProperty ("order"), 0);
    }

    if (auto* partials = object->getProperty ("partials").getArray())
    {
        bank.partials.reserve (static_cast<size_t> (partials->size()));
        for (const auto& item : *partials)
        {
            if (auto* partialObject = item.getDynamicObject())
            {
                DronePartial partial;
                partial.frequencyHz = readDouble (partialObject->getProperty ("freq_hz"), 440.0);
                partial.phaseRadians = readDouble (partialObject->getProperty ("phase_rad"), 0.0);
                partial.amplitudeDb = readDouble (partialObject->getProperty ("amp_base"), -60.0);
                partial.amplitudeCoefficients =
                    readDoubleArray (partialObject->getProperty ("amp_coefficients"));

                if (partial.frequencyHz > 0.0 && dbToAmplitude (partial.amplitudeDb) > 0.0)
                    bank.partials.push_back (std::move (partial));
            }
        }
    }

    return bank;
}
