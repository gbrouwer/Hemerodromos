#pragma once

#include <JuceHeader.h>

struct DronePartial
{
    double frequencyHz = 440.0;
    double phaseRadians = 0.0;
    double amplitudeDb = -60.0;
    std::vector<double> amplitudeCoefficients;
    std::vector<double> frequencyLogRatioCoefficients;
};

struct DroneBank
{
    juce::String name;
    juce::String displayName;
    juce::String sourceStem;
    juce::String fitVersion;
    juce::String profileLabel;
    juce::String modelType;
    int analysisSampleRate = 44100;
    double durationSeconds = 1.0;
    int basisOrder = 0;
    juce::String basisType = "static";
    double stereoWidth = 0.35;
    std::vector<DronePartial> partials;

    bool isValid() const noexcept { return ! partials.empty() && durationSeconds > 0.0; }
};

class DroneBankLibrary
{
public:
    DroneBankLibrary();

    int size() const noexcept { return static_cast<int> (banks_.size()); }
    const DroneBank& getBank (int index) const noexcept;
    juce::StringArray getNames() const;
    juce::StringArray getSourceStems() const;
    juce::StringArray getSourceNames() const;
    juce::StringArray getProfileLabelsForSource (const juce::String& sourceStem) const;
    int findBankIndex (const juce::String& sourceStem, const juce::String& profileLabel) const noexcept;
    static juce::StringArray getEmbeddedChoiceNames();

private:
    std::vector<DroneBank> banks_;

    static DroneBank parseBank (const char* data, int size, const juce::String& originalFilename);
};
