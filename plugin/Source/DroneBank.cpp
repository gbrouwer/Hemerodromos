#include "DroneBank.h"

#include "BinaryData.h"

namespace
{
struct BankResource
{
    juce::String resourceName;
    juce::String originalFilename;
    juce::String sourceStem;
    juce::String fitVersion;
    juce::String profileLabel;
    int bankNumber = 0;
};

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

int readBankNumber (const juce::String& filename) noexcept
{
    const auto marker = juce::String ("drone_base");
    const auto start = filename.indexOf (marker);
    if (start < 0)
        return 0;

    const auto firstDigit = start + marker.length();
    auto numberText = juce::String();
    for (auto index = firstDigit; index < filename.length(); ++index)
    {
        const auto character = filename[index];
        if (! juce::CharacterFunctions::isDigit (character))
            break;
        numberText << character;
    }

    return numberText.isNotEmpty() ? numberText.getIntValue() : 0;
}

juce::String assetStemForFilename (const juce::String& filename)
{
    auto stem = juce::File (filename).getFileName();
    stem = stem.upToFirstOccurrenceOf (".dronebank.json", false, true);
    return stem;
}

juce::String sourceStemForFilename (const juce::String& filename)
{
    auto stem = assetStemForFilename (filename);
    return stem.upToFirstOccurrenceOf ("_fit_", false, true);
}

juce::String fitVersionForFilename (const juce::String& filename)
{
    auto stem = assetStemForFilename (filename);
    const auto marker = juce::String ("_fit_");
    const auto markerIndex = stem.indexOf (marker);
    if (markerIndex < 0)
        return {};

    return stem.substring (markerIndex + marker.length()).trim();
}

juce::String profileLabelFromFitVersion (juce::String fitVersion, bool includeDefaultVersion)
{
    fitVersion = fitVersion.trim();
    if (fitVersion.isEmpty())
        return {};

    auto lower = fitVersion.toLowerCase();
    if (! includeDefaultVersion && lower == "v001")
        return {};

    auto profile = fitVersion;
    const auto underscore = lower.indexOfChar ('_');
    if (underscore > 0 && lower.startsWithChar ('v'))
        profile = fitVersion.substring (underscore + 1);
    else if (includeDefaultVersion)
        return fitVersion.toUpperCase();

    auto normalized = profile.toLowerCase().replaceCharacter ('-', '_');
    if (normalized == "mrstft" || normalized == "multi_stft" || normalized == "multi_resolution_stft")
        return "MR-STFT";
    if (normalized == "srstft" || normalized == "single_stft" || normalized == "single_resolution_stft")
        return "SR-STFT";

    profile = profile.replaceCharacters ("_-", "  ").trim();
    if (profile.isEmpty())
        return fitVersion.toUpperCase();

    juce::StringArray words;
    words.addTokens (profile, " ", "");
    words.removeEmptyStrings();

    juce::String label;
    for (auto word : words)
    {
        word = word.toLowerCase();
        word = word.length() <= 3 ? word.toUpperCase()
                                  : word.substring (0, 1).toUpperCase() + word.substring (1);
        if (label.isNotEmpty())
            label << " ";
        label << word;
    }
    return label;
}

juce::String displayNameForSourceStem (juce::String stem)
{
    const auto bankNumber = readBankNumber (stem);
    if (bankNumber > 0)
        return "Drone " + juce::String (bankNumber);

    stem = stem.replaceCharacters ("_-", "  ").trim();
    if (stem.isEmpty())
        return "Drone";

    juce::StringArray words;
    words.addTokens (stem, " ", "");
    words.removeEmptyStrings();

    juce::String displayName;
    for (auto word : words)
    {
        word = word.toLowerCase();
        if (word.isNotEmpty())
            word = word.substring (0, 1).toUpperCase() + word.substring (1);

        if (displayName.isNotEmpty())
            displayName << " ";
        displayName << word;
    }
    return displayName.isNotEmpty() ? displayName : "Drone";
}

int countSourceStem (const std::vector<BankResource>& resources, const juce::String& sourceStem)
{
    return static_cast<int> (std::count_if (resources.begin(), resources.end(), [&sourceStem] (const auto& resource)
    {
        return resource.sourceStem == sourceStem;
    }));
}

juce::String displayNameForResource (const BankResource& resource,
                                     const std::vector<BankResource>& resources)
{
    auto name = displayNameForSourceStem (resource.sourceStem);
    auto profileLabel = resource.profileLabel;
    if (profileLabel.isEmpty() && countSourceStem (resources, resource.sourceStem) > 1)
        profileLabel = profileLabelFromFitVersion (resource.fitVersion, true);

    if (profileLabel.isNotEmpty())
        name << " " << profileLabel;

    return name;
}

std::vector<BankResource> collectBankResources()
{
    std::vector<BankResource> resources;
    resources.reserve (static_cast<size_t> (BinaryData::namedResourceListSize));

    for (auto index = 0; index < BinaryData::namedResourceListSize; ++index)
    {
        const auto resourceName = juce::String (BinaryData::namedResourceList[index]);
        const auto* original = BinaryData::getNamedResourceOriginalFilename (resourceName.toRawUTF8());
        if (original == nullptr)
            continue;

        const auto originalFilename = juce::String (original);
        if (! originalFilename.endsWithIgnoreCase (".dronebank.json"))
            continue;

        const auto bankNumber = readBankNumber (originalFilename);
        const auto fitVersion = fitVersionForFilename (originalFilename);
        resources.push_back ({ resourceName,
                               originalFilename,
                               sourceStemForFilename (originalFilename),
                               fitVersion,
                               profileLabelFromFitVersion (fitVersion, false),
                               bankNumber });
    }

    std::sort (resources.begin(), resources.end(), [] (const BankResource& left, const BankResource& right)
    {
        if (left.bankNumber > 0 && right.bankNumber > 0 && left.bankNumber != right.bankNumber)
            return left.bankNumber < right.bankNumber;

        if ((left.bankNumber > 0) != (right.bankNumber > 0))
            return left.bankNumber > 0;

        if (left.sourceStem != right.sourceStem)
            return left.sourceStem < right.sourceStem;

        return left.fitVersion < right.fitVersion;
    });
    return resources;
}

int countBankSourceStem (const std::vector<DroneBank>& banks, const juce::String& sourceStem)
{
    return static_cast<int> (std::count_if (banks.begin(), banks.end(), [&sourceStem] (const auto& bank)
    {
        return bank.sourceStem == sourceStem;
    }));
}

juce::String displayNameForBank (const DroneBank& bank, const std::vector<DroneBank>& banks)
{
    auto sourceStem = bank.sourceStem;
    if (sourceStem.isEmpty())
        sourceStem = bank.name;

    auto name = displayNameForSourceStem (sourceStem);
    if (name.isEmpty())
        return "Drone";

    auto profileLabel = bank.profileLabel;
    if (profileLabel.isEmpty() && sourceStem.isNotEmpty() && countBankSourceStem (banks, sourceStem) > 1)
        profileLabel = profileLabelFromFitVersion (bank.fitVersion, true);

    if (profileLabel.isNotEmpty())
        name << " " << profileLabel;
    else if (sourceStem == bank.name && bank.displayName.isNotEmpty())
        name = bank.displayName;

    return name.replaceCharacter ('_', ' ');
}

juce::String profileChoiceLabelForBank (const DroneBank& bank)
{
    auto label = bank.profileLabel;
    if (label.isEmpty())
        label = profileLabelFromFitVersion (bank.fitVersion, true);

    return label.isNotEmpty() ? label : "Default";
}
} // namespace

DroneBankLibrary::DroneBankLibrary()
{
    const auto resources = collectBankResources();
    banks_.reserve (resources.size());
    for (const auto& resource : resources)
    {
        auto dataSize = 0;
        if (const auto* data = BinaryData::getNamedResource (resource.resourceName.toRawUTF8(), dataSize))
            banks_.push_back (parseBank (data, dataSize, resource.originalFilename));
    }

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
        names.add (displayNameForBank (bank, banks_));
    return names;
}

juce::StringArray DroneBankLibrary::getSourceStems() const
{
    juce::StringArray stems;
    for (const auto& bank : banks_)
    {
        const auto sourceStem = bank.sourceStem.isNotEmpty() ? bank.sourceStem : bank.name;
        if (sourceStem.isNotEmpty() && ! stems.contains (sourceStem))
            stems.add (sourceStem);
    }

    if (stems.isEmpty())
        stems.add ("drone");

    return stems;
}

juce::StringArray DroneBankLibrary::getSourceNames() const
{
    juce::StringArray names;
    for (const auto& stem : getSourceStems())
        names.add (displayNameForSourceStem (stem).replaceCharacter ('_', ' '));

    if (names.isEmpty())
        names.add ("Drone");

    return names;
}

juce::StringArray DroneBankLibrary::getProfileLabelsForSource (const juce::String& sourceStem) const
{
    juce::StringArray labels;
    for (const auto& bank : banks_)
    {
        const auto bankSourceStem = bank.sourceStem.isNotEmpty() ? bank.sourceStem : bank.name;
        if (bankSourceStem != sourceStem)
            continue;

        const auto label = profileChoiceLabelForBank (bank);
        if (! labels.contains (label))
            labels.add (label);
    }

    if (labels.isEmpty())
        labels.add ("Default");

    return labels;
}

int DroneBankLibrary::findBankIndex (const juce::String& sourceStem,
                                     const juce::String& profileLabel) const noexcept
{
    auto firstSourceMatch = -1;
    for (int index = 0; index < size(); ++index)
    {
        const auto& bank = banks_[static_cast<size_t> (index)];
        const auto bankSourceStem = bank.sourceStem.isNotEmpty() ? bank.sourceStem : bank.name;
        if (bankSourceStem != sourceStem)
            continue;

        if (firstSourceMatch < 0)
            firstSourceMatch = index;

        if (profileChoiceLabelForBank (bank) == profileLabel)
            return index;
    }

    return firstSourceMatch >= 0 ? firstSourceMatch : 0;
}

juce::StringArray DroneBankLibrary::getEmbeddedChoiceNames()
{
    juce::StringArray names;
    const auto resources = collectBankResources();
    for (const auto& resource : resources)
        names.add (displayNameForResource (resource, resources));

    if (names.isEmpty())
        names.add ("Drone");

    return names;
}

DroneBank DroneBankLibrary::parseBank (const char* data, int size, const juce::String& originalFilename)
{
    DroneBank bank;
    bank.sourceStem = sourceStemForFilename (originalFilename);
    bank.fitVersion = fitVersionForFilename (originalFilename);
    bank.profileLabel = profileLabelFromFitVersion (bank.fitVersion, false);

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
    bank.stereoWidth = readDouble (object->getProperty ("stereo_width"), 0.35);

    if (auto* metadata = object->getProperty ("metadata").getDynamicObject())
    {
        if (auto* soundbank = metadata->getProperty ("soundbank").getDynamicObject())
        {
            const auto displayName = soundbank->getProperty ("display_name").toString().trim();
            const auto sourceStem = soundbank->getProperty ("source_stem").toString().trim();
            const auto fitVersion = soundbank->getProperty ("fit_version").toString().trim();
            const auto profileLabel = soundbank->getProperty ("fit_profile_label").toString().trim();

            if (displayName.isNotEmpty())
                bank.displayName = displayName;
            if (sourceStem.isNotEmpty())
                bank.sourceStem = sourceStem;
            if (fitVersion.isNotEmpty())
                bank.fitVersion = fitVersion;
            if (profileLabel.isNotEmpty())
                bank.profileLabel = profileLabel;
        }
    }

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
                partial.frequencyLogRatioCoefficients =
                    readDoubleArray (partialObject->getProperty ("freq_log_ratio_coefficients"));

                if (partial.frequencyHz > 0.0 && dbToAmplitude (partial.amplitudeDb) > 0.0)
                    bank.partials.push_back (std::move (partial));
            }
        }
    }

    return bank;
}
