
#include "Controller Slider.h"


ControllerSlider::ControllerSlider ( juce::AudioProcessorValueTreeState& state,
                                     const juce::String& parameterID,
                                     const juce::String& parameterLabel)
: Slider(parameterLabel), mAttachment(state, parameterID, *this)
{
    setSliderStyle(SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::TextEntryBoxPosition::NoTextBox, false, 0, 0);
    setPopupDisplayEnabled(true, true, nullptr);


    

}

ControllerSlider::~ControllerSlider() {}



