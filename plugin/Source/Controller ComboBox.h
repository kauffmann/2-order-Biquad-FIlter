#pragma once

#include <JuceHeader.h>





class ControllerComboBox : public juce::ComboBox, public ComboBox::Listener
{

public:


    ControllerComboBox( juce::AudioProcessorValueTreeState& stateControl, const juce::String& parameterID);
    ~ControllerComboBox() override;

    void comboBoxChanged (ComboBox* comboBoxThatHasChanged) override;






private:

    juce::AudioProcessorValueTreeState::ComboBoxAttachment mAttachment;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControllerComboBox)

};

