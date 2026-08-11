#pragma once

#include <JuceHeader.h>


class ControllerSlider : public juce::Slider
{

public:
    ControllerSlider( juce::AudioProcessorValueTreeState& state,
                     const juce::String& parameterID,
                     const juce::String& parameterLabel);

    ~ControllerSlider();






private:
    juce::AudioProcessorValueTreeState::SliderAttachment mAttachment;





    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControllerSlider)

};
