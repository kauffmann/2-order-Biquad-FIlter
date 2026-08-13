/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "FilterCoefficients.h"
#include "MultiFilter.h"
#include "ParameterLayout.h"

//==============================================================================
/**
*/
class FilterAudioProcessor  : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    FilterAudioProcessor();
    ~FilterAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    juce::AudioProcessorValueTreeState& getApvts() { return apvts; }
    
    FilterCoefficients& getFilterCoefficients() { return mFilterCoefficients; }

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // used by WrappedRasterAudioProcessorEditor to resize UI
    double getResizeFactor()
    {
        return mResizeFactor;
    }

    void setResizeFactor(double value)
    {
        mResizeFactor = value;
    }

private:
    // Shared coefficient cache - single source of truth for filter coefficients
    FilterCoefficients mFilterCoefficients;
    
    // Filter instances - use unique_ptr to handle construction with FilterCoefficients reference
    std::unique_ptr<MultiFilter> mFilterLeft;
    std::unique_ptr<MultiFilter> mFilterRight;
    MultiFilter* mFilter[2];

    juce::AudioProcessorValueTreeState apvts;

    juce::String getParamID(juce::AudioProcessorParameter* param)
    {
        if (auto paramWithID = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            return paramWithID->paramID;

        return param->getName(50);
    }

    
    // Listener callback when parameters change
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    FilterParameterLayout mParameterLayout;

    // UI resize parameter
    double mResizeFactor{ 1.2 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterAudioProcessor)
};
