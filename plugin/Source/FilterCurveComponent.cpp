/*
  ==============================================================================

    FilterCurveComponent.cpp - Implementation of the filter curve visualization

  ==============================================================================
*/

#include "FilterCurveComponent.h"


//==============================================================================
FilterCurveComponent::FilterCurveComponent(FilterAudioProcessor& processor)
    : mProcessor(processor),
      mSampleRate(44100.0f)
{
    // Register as listener to parameter changes.
    mProcessor.getApvts().addParameterListener("CUTOFF", this);
    mProcessor.getApvts().addParameterListener("RESONANCE", this);
    mProcessor.getApvts().addParameterListener("GAIN", this);
    mProcessor.getApvts().addParameterListener("FILTER", this);

    // Get initial sample rate (use default if not available yet)
    if (auto* audioProcessor = dynamic_cast<juce::AudioProcessor*>(&mProcessor))
    {
        float sampleRate = static_cast<float>(audioProcessor->getSampleRate());
        if (sampleRate > 0.0f)
            mSampleRate = sampleRate;
    }

    // Start timer for thread-safe UI updates
    startTimerHz(60);

    mCutoffFreq = mProcessor.getApvts().getRawParameterValue("CUTOFF");
    mResonance = mProcessor.getApvts().getRawParameterValue("RESONANCE");
    mGain = mProcessor.getApvts().getRawParameterValue("GAIN");
    mFilterType = mProcessor.getApvts().getRawParameterValue("FILTER");

    generateCurvePath();

}

FilterCurveComponent::~FilterCurveComponent()
{
    // Remove parameter listeners
    mProcessor.getApvts().removeParameterListener("CUTOFF", this);
    mProcessor.getApvts().removeParameterListener("RESONANCE", this);
    mProcessor.getApvts().removeParameterListener("GAIN", this);
    mProcessor.getApvts().removeParameterListener("FILTER", this);
}

//==============================================================================
void FilterCurveComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float width = static_cast<float>(bounds.getWidth());
    float height = static_cast<float>(bounds.getHeight());

    // Don't draw if we have fewer than 2 points
    if (mCurvePoints.size() < 2 )
        return;

    // Create fill path (curve + lines down to bottom)
    juce::Path fillPath;

    // Start at first curve point
    fillPath.startNewSubPath(mCurvePoints[0]);

    // Add all curve points
    for (size_t i = 1; i < mCurvePoints.size(); ++i)
    {
        fillPath.lineTo(mCurvePoints[i]);

    }

    // Add lines down to bottom-right
    fillPath.lineTo(bounds.getBottomRight());

    // Add line to bottom-left
    fillPath.lineTo(bounds.getBottomLeft());

    // Close path back to first point
    fillPath.closeSubPath();

    // Apply gradient fill
    auto fillBounds = fillPath.getBounds();
    auto gradient = juce::ColourGradient::vertical(
        juce::Colours::blueviolet.withAlpha(0.8f),
        juce::Colours::transparentBlack,
        fillBounds
    );
    gradient.addColour(0.9f, juce::Colours::transparentBlack.withAlpha(0.1f));

    g.setGradientFill(gradient);
    g.fillPath(fillPath);



    // Draw solid line on top of fill
    g.setColour(juce::Colours::blueviolet);
    g.strokePath(mCurvePath, juce::PathStrokeType(2.0f));

}


void FilterCurveComponent::resized()
{
    generateCurvePath();
}

void FilterCurveComponent::setBounds(const juce::Rectangle<int>& bounds)
{
    // juce::Component::setBounds(bounds);
    // // Regenerate curve when bounds change
    // if (bounds.getWidth() > 0 && bounds.getHeight() > 0)
    // {
    //     generateCurvePath();
    // }
}

void FilterCurveComponent::setBounds(int x, int y, int width, int height)
{
    juce::Component::setBounds(x, y, width, height);
    // Regenerate curve when bounds change
    if (width > 0 && height > 0)
    {
        generateCurvePath();
    }
}

//==============================================================================
float FilterCurveComponent::freqToX(float frequency, float width) const
{
    // Logarithmic mapping
    float logFreq = std::log10(std::max(frequency, MIN_FREQ));
    float logMin = std::log10(MIN_FREQ);
    float logMax = std::log10(MAX_FREQ);
    
    float normalized = (logFreq - logMin) / (logMax - logMin);
    return normalized * width;
}

float FilterCurveComponent::magToY(float magnitudeDB, float height) const
{
    // Invert: higher magnitude = lower y position
    // Clamp magnitude to visible range
    magnitudeDB = std::max(std::min(magnitudeDB, MAX_DB), MIN_DB);
    float normalized = 1.0f - ((magnitudeDB - MIN_DB) / (MAX_DB - MIN_DB));
    return normalized * height;
}

//==============================================================================
float FilterCurveComponent::calculateMagnitudeAt(float frequency) const
{

    // Get coefficients from processor's filter (single source of truth)
    auto coeffs = mProcessor.getFilterCoefficients(0);

    // Map coefficients using MultiFilter::CoeffIndex enum
    double b0 = coeffs[MultiFilter::B0];
    double b1 = coeffs[MultiFilter::B1];
    double b2 = coeffs[MultiFilter::B2];
    double a1 = coeffs[MultiFilter::A1];
    double a2 = coeffs[MultiFilter::A2];
    
    // Calculate omega for this frequency using component's sample rate
    double testOmega = 2.0 * juce::MathConstants<double>::pi * frequency / mSampleRate;
    
    // Calculate magnitude response
    double numReal = b0 + b1 * std::cos(testOmega) + b2 * std::cos(2.0 * testOmega);
    double numImag = -b1 * std::sin(testOmega) - b2 * std::sin(2.0 * testOmega);
    double denReal = 1.0 + a1 * std::cos(testOmega) + a2 * std::cos(2.0 * testOmega);
    double denImag = -a1 * std::sin(testOmega) - a2 * std::sin(2.0 * testOmega);
    
    double magSquared = (numReal * numReal + numImag * numImag) / 
                       (denReal * denReal + denImag * denImag);
    
    // Handle potential NaN or negative values
    if (magSquared <= 0.0 || std::isnan(magSquared) || std::isinf(magSquared))
        return MIN_DB - 10.0f; // Very low value
    
    // Clamp magSquared to prevent extreme values
    magSquared = std::max(1e-10, std::min(magSquared, 1e10));
    
    // Convert to dB
    return static_cast<float>(20.0 * std::log10(std::sqrt(magSquared)));
}

//==============================================================================
void FilterCurveComponent::generateCurvePath()
{

    mCurvePath.clear();
    mCurvePoints.clear();

    auto bounds = getLocalBounds();
    float width = static_cast<float>(bounds.getWidth());
    float height = static_cast<float>(bounds.getHeight());

    // Safety check: ensure sample rate is valid
    if (mSampleRate <= 0.0f)
        mSampleRate = 44100.0f; // Fallback to default

    if (width <= 0 || height <= 0)
        return;



    // Generate points across the frequency spectrum
    for (int i = 0; i < NUM_POINTS; ++i)
    {
        // Use logarithmic spacing for better resolution at low frequencies
        float normalizedPos = static_cast<float>(i) / (NUM_POINTS - 1);
        float logFreq = MIN_FREQ * std::pow(MAX_FREQ / MIN_FREQ, normalizedPos);

        float freq = logFreq;
        float mag = calculateMagnitudeAt(freq);
        float x = freqToX(freq, width);
        float y = magToY(mag, height);

        // Clamp coordinates to valid range to prevent NaN/Inf issues
        if (!std::isfinite(x) || x < 0) x = 0;
        else if (x > width) x = width;
        if (!std::isfinite(y) || y < 0) y = 0;
        else if (y > height) y = height;

        if (i == 0)
        {
            mCurvePath.startNewSubPath(x, y);
            mCurvePoints.emplace_back(x, y);
        }
        else
        {
            mCurvePath.lineTo(x, y);
            mCurvePoints.emplace_back(x, y);

        }
    }
    DBG("Generated curve path");
    // Request repaint
    repaint();
}

//==============================================================================
void FilterCurveComponent::timerCallback()
{


    if (mNeedsRepaint)
    {
        mNeedsRepaint = false;
        generateCurvePath();

    }
}


// DEBUG state: use atomic instead - works, except combobox, who gets default 0 regardless of saved value..
//==============================================================================
void FilterCurveComponent::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Flag for timer callback to regenerate curve on UI thread
        mNeedsRepaint = true;
}



void FilterCurveComponent::setSampleRate(float sampleRate)
{
    if (sampleRate > 0.0f)
        mSampleRate = sampleRate;
    mNeedsRepaint = true;
}
