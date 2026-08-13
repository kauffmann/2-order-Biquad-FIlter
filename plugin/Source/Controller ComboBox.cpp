#pragma once

#include <JuceHeader.h>
#include "Controller ComboBox.h"


ControllerComboBox::ControllerComboBox( juce::AudioProcessorValueTreeState& stateControl, const juce::String& parameterID)
: juce::ComboBox(parameterID), mAttachment(stateControl, parameterID, *this)
{
    addListener(this);
}

ControllerComboBox::~ControllerComboBox() { removeListener(this); }



void ControllerComboBox::comboBoxChanged (ComboBox* comboBoxThatHasChanged)
{

    //Nothing here...so why have it?
    

}


