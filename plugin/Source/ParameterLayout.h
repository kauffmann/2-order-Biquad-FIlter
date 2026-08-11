/*
  ==============================================================================

    ParameterLayout.h - Defines the parameter layout for the filter plugin

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

struct FilterParameterLayout
{
    explicit  FilterParameterLayout(){}

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() const;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterParameterLayout)
};
