#pragma once

#include <array>

namespace ParameterIds
{
static constexpr auto bank = "bank";
static constexpr auto gain = "gain";
static constexpr auto tune = "tune";
static constexpr auto brightness = "brightness";
static constexpr auto motionDepth = "motionDepth";
static constexpr auto motionRate = "motionRate";
static constexpr auto roughness = "roughness";
static constexpr auto macroOsc = "macroOsc";
static constexpr auto resonator = "resonator";
static constexpr auto reverbMix = "reverbMix";
static constexpr auto reverbSize = "reverbSize";
static constexpr auto reverbDecay = "reverbDecay";
static constexpr auto stereoWidth = "stereoWidth";
static constexpr auto attack = "attack";
static constexpr auto release = "release";
static constexpr auto latch = "latch";

struct KnobParameterSpec
{
    const char* id;
    const char* name;
    float minimum;
    float maximum;
    float interval;
    float neutral;
    const char* unit;
};

inline constexpr std::array<KnobParameterSpec, 14> knobParameterSpecs {
    KnobParameterSpec { gain, "Gain", -36.0f, 12.0f, 0.01f, -12.0f, "dB" },
    KnobParameterSpec { tune, "Tune", -24.0f, 24.0f, 0.01f, 0.0f, "st" },
    KnobParameterSpec { brightness, "Brightness", -1.0f, 1.0f, 0.001f, 0.0f, "" },
    KnobParameterSpec { motionDepth, "Motion Depth", 0.0f, 2.0f, 0.001f, 1.0f, "" },
    KnobParameterSpec { motionRate, "Motion Rate", 0.0f, 2.0f, 0.001f, 1.0f, "x" },
    KnobParameterSpec { roughness, "Roughness", -1.0f, 1.0f, 0.001f, 0.0f, "" },
    KnobParameterSpec { macroOsc, "Macro Osc", -1.0f, 1.0f, 0.001f, 0.0f, "" },
    KnobParameterSpec { resonator, "Resonator", -1.0f, 1.0f, 0.001f, 0.0f, "" },
    KnobParameterSpec { reverbMix, "Plate Space", -1.0f, 1.0f, 0.001f, 0.0f, "" },
    KnobParameterSpec { reverbSize, "Space Size", -1.0f, 1.0f, 0.001f, 0.0f, "" },
    KnobParameterSpec { reverbDecay, "Space Decay", -1.0f, 1.0f, 0.001f, 0.0f, "" },
    KnobParameterSpec { stereoWidth, "Width", 0.0f, 0.70f, 0.001f, 0.35f, "" },
    KnobParameterSpec { attack, "Attack", 0.0f, 2.0f, 0.001f, 1.0f, "s" },
    KnobParameterSpec { release, "Release", 0.0f, 6.0f, 0.001f, 3.0f, "s" },
};
} // namespace ParameterIds
