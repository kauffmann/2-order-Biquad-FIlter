/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "Controller Slider.h"
#include "Controller ComboBox.h"
#include "FilterCurveComponent.h"

//==============================================================================
/**
*/
class PluginEditor  : public juce::Component
{
public:
    //==============================================================================
    PluginEditor (FilterAudioProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FilterAudioProcessor& processorRef;
    
    juce::Image backGround;
    CustomLookAndFeel customLookAndFeel;

    ControllerSlider mCutoffSlider;
    ControllerSlider mResonanceSlider;
    ControllerSlider mGainSlider;

    juce::Label cutoffLabel{"cutoff label", "CUTOFF"};
    juce::Label resonanceLabel{"resonance label", "RESONANCE"};
    juce::Label gainLabel{"gain label", "GAIN"};

    ControllerComboBox mFilterComboBox;

    juce::Label filterLabel{"filter label", "FILTER MODE"};
    
    // Filter curve visualization
    FilterCurveComponent mFilterCurve;

    juce::Image mLogo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};

class WrappedRasterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    WrappedRasterAudioProcessorEditor(FilterAudioProcessor&);
    void resized() override;

private:
    static constexpr int originalWidth{ 500};
    static constexpr int originalHeight{ 310};

    PluginEditor rasterComponent;

    FilterAudioProcessor& mProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WrappedRasterAudioProcessorEditor)
  };