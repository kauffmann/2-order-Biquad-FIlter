/*
  ==============================================================================

    ParameterLayout.cpp - Implementation of the parameter layout for the filter plugin

  ==============================================================================
*/

#include "ParameterLayout.h"




juce::AudioProcessorValueTreeState::ParameterLayout FilterParameterLayout::createParameterLayout() const
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    constexpr auto versionHint = 1;

    // Add cutoff frequency parameter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"CUTOFF", versionHint},
        "Cutoff Frequency",
        juce::NormalisableRange<float>(20.0f, 15000.0f, 0.01f, 0.3f),
        1000.0f, juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) -> juce::String
        {
            juce::String valueToText =
                juce::String(value, 1) + "Hz";

            return valueToText;
        } )); // default: 1000Hz

    // Add resonance parameter (Q factor)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"RESONANCE", versionHint},
        "Resonance",
        juce::NormalisableRange<float>(0.707f, 10.707f, 0.01f),
        0.707f, juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) -> juce::String
        {
            juce::String valueToText =
                juce::String((value -0.707) * 10, 1) + " %";

            return valueToText;
        })); // default: 0.707 (Butterworth Q)

    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"GAIN", versionHint}, "Shelf Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"FILTER", versionHint}, "Filter Type",
        juce::StringArray{ "LowPass", "HighPass", "BPF", "Notch", "HighShelf", "LowShelf" }, 0));

    return { params.begin(), params.end() };
}
