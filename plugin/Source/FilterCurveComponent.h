/*
  ==============================================================================

    FilterCurveComponent.h - Displays the frequency response curve of the filter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include "PluginProcessor.h"
#include "MultiFilter.h"
#include <cmath>

//class FilterAudioProcessor;

//==============================================================================
/**
    Component that visualizes the filter's frequency response curve.
    Draws a solid line with gradient fill below it, responding to parameter changes.
*/
class FilterCurveComponent : public juce::Component,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::Timer
{
public:
    //==============================================================================
    /** Constructor */
    FilterCurveComponent(FilterAudioProcessor& processor);
    
    /** Destructor */
    ~FilterCurveComponent() override;

    //==============================================================================
    /** @internal */
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void setBounds(const juce::Rectangle<int>& bounds);
    void setBounds(int x, int y, int width, int height);

private:
    //==============================================================================
    FilterAudioProcessor& mProcessor;
    juce::Path mCurvePath;
    float mSampleRate;
    
    // Cached parameters for efficient redrawing

    std::atomic<float>* mCutoffFreq;
    std::atomic<float>* mResonance;
    std::atomic<float>* mGain;
    std::atomic<float>* mFilterType;
    
    // Store curve points for fill path generation
    std::vector<juce::Point<float>> mCurvePoints;
    
    // Thread safety: flag to defer UI updates to message thread
    bool mNeedsRepaint{false};

    // Frequency and magnitude ranges
    static constexpr float MIN_FREQ = 50.0f;
    static constexpr float MAX_FREQ = 20000.0f;
    static constexpr float MIN_DB = -24.0f;
    static constexpr float MAX_DB = 24.0f;  // Increased from 12 to 24 to show high-Q peaks
    static constexpr int NUM_POINTS = 300;

    //==============================================================================
    /** Calculate magnitude response at a given frequency */
    float calculateMagnitudeAt(float frequency) const;
    
    /** Generate the curve path based on current filter parameters */
    void generateCurvePath();
    
    /** Map frequency to x-coordinate (logarithmic scale) */
    float freqToX(float frequency, float width) const;
    
    /** Map magnitude in dB to y-coordinate */
    float magToY(float magnitudeDB, float height) const;

    //==============================================================================
    /** Called when a parameter changes */
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    

    
    /** Update sample rate and regenerate curve */
    void setSampleRate(float sampleRate);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterCurveComponent)
};
