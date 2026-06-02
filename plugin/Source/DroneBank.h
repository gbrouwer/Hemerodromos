#pragma once

#include <JuceHeader.h>

struct DronePartial
{
    double frequencyHz = 440.0;
    double phaseRadians = 0.0;
    double amplitudeDb = -60.0;
    std::vector<double> amplitudeCoefficients;
};

struct DroneBank
{
    juce::String name;
    juce::String modelType;
    int analysisSampleRate = 44100;
    double durationSeconds = 1.0;
    int basisOrder = 0;
    juce::String basisType = "static";
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

private:
    std::vector<DroneBank> banks_;

    static DroneBank parseBank (const char* data, int size);
};
