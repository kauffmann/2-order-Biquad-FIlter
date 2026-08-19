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

    // Create a temporary MultiFilter to calculate response
    MultiFilter tempFilter;
    tempFilter.setSamplingRate(static_cast<double>(mSampleRate));

    // Set filter parameters with safety clamping
    float cutoff = std::max(20.0f, mCutoffFreq->load());
    float resonance = std::max(0.1f, std::min(mResonance->load(), 20.0f));
    float gain = std::max(-24.0f, std::min(mGain->load(), 24.0f));
    int filterType = static_cast<int>( mFilterType->load());
    
    tempFilter.setCutoffFrequency(cutoff);
    tempFilter.setResonance(resonance);
    tempFilter.setGain(gain);
    tempFilter.setFilterType(filterType);
    
    // Get the filter coefficients (we need to access them)
    // Since MultiFilter doesn't have getters, we'll use the formulas directly
    // This is a workaround - in a better design, MultiFilter would expose getters

    // Calculate coefficients based on current parameters
    double omega = 2.0 * juce::MathConstants<double>::pi * cutoff / mSampleRate;
    double alpha = std::sin(omega) / (2.0 * resonance);
    double cos_omega = std::cos(omega);
    
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a0 = 1.0, a1 = 0.0, a2 = 0.0;



    MultiFilter::FilterType filterTypeEnum = static_cast<MultiFilter::FilterType>(filterType);

    // Yes, we are violating DRY, same logic also in MultiFilter.h ,  but small project, easy to overview and maintain if changes happens to logic.

    switch (filterTypeEnum)
    {
        case MultiFilter::LowPass:
            b0 = (1.0 - cos_omega) / 2.0;
            b1 = 1.0 - cos_omega;
            b2 = (1.0 - cos_omega) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_omega;
            a2 = 1.0 - alpha;
            break;
            
        case MultiFilter::HighPass:
            b0 = (1.0 + cos_omega) / 2.0;
            b1 = -(1.0 + cos_omega);
            b2 = (1.0 + cos_omega) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_omega;
            a2 = 1.0 - alpha;
            break;
            
        case MultiFilter::BandPass:
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_omega;
            a2 = 1.0 - alpha;
            break;
            
        case MultiFilter::Notch:
            b0 = 1.0;
            b1 = -2.0 * cos_omega;
            b2 = 1.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_omega;
            a2 = 1.0 - alpha;
            break;
            
        case MultiFilter::LowShelf:
        {
            double A = std::pow(10.0, gain / 40.0);
            b0 = A * ((A + 1.0) - (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cos_omega);
            b2 = A * ((A + 1.0) - (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha);
            a0 = (A + 1.0) + (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cos_omega);
            a2 = (A + 1.0) + (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha;
        }
        break;
            
        case MultiFilter::HighShelf:
        {
            double A = std::pow(10.0, gain / 40.0);
            b0 = A * ((A + 1.0) + (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cos_omega);
            b2 = A * ((A + 1.0) + (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha);
            a0 = (A + 1.0) - (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cos_omega);
            a2 = (A + 1.0) - (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha;
        }
        break;
            
        default:
            return 0.0f;
    }
    
    // Normalize coefficients by a0
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
    
    // Calculate omega for this frequency
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

    // Get filter type to determine stopband behavior
    int filterTypeInt = static_cast<int>(mFilterType->load());
    MultiFilter::FilterType filterTypeEnum = static_cast<MultiFilter::FilterType>(filterTypeInt);

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

    // Add flat stopband extensions based on filter type
    // For LowPass: extend right edge horizontally at stopband level
    // For HighPass: extend left edge horizontally at stopband level
    // For others: extend both sides appropriately

    const float stopbandY = height; // Bottom of component (MIN_DB maps to height)
    //   HAS NO EFFECT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // switch (filterTypeEnum)
    // {
    //     case MultiFilter::LowPass:
    //     case MultiFilter::LowShelf:
    //     {
    //         // For low filters: extend from last point horizontally to right edge at its y-level
    //         // This creates the flat stopband on the right
    //         float lastX = mCurvePoints.back().x;
    //         float lastY = mCurvePoints.back().y;
    //
    //         if (lastX < width)
    //         {
    //             mCurvePath.lineTo(width, lastY);
    //             mCurvePoints.emplace_back(width, lastY);
    //
    //         }
    //         break;
    //     }
    //
    //     case MultiFilter::HighPass:
    //     case MultiFilter::HighShelf:
    //     {
    //         // For high filters: extend from first point horizontally to left edge at its y-level
    //         // This creates the flat stopband on the left
    //         float firstX = mCurvePoints.front().x;
    //         float firstY = mCurvePoints.front().y;
    //         if (firstX > 0)
    //         {
    //             mCurvePath.lineTo(0, firstY);
    //             mCurvePoints.insert(mCurvePoints.begin(), juce::Point<float>(0, firstY));
    //         }
    //         break;
    //     }
    //
    //     case MultiFilter::BandPass:
    //     {
    //         // For bandpass: extend both sides
    //         // Left side: extend first point to left edge at its y-level
    //         float firstX = mCurvePoints.front().x;
    //         float firstY = mCurvePoints.front().y;
    //         if (firstX > 0)
    //         {
    //             mCurvePath.lineTo(0, firstY);
    //             mCurvePoints.insert(mCurvePoints.begin(), juce::Point<float>(0, firstY));
    //         }
    //         // Right side: extend last point to right edge at its y-level
    //         float lastX = mCurvePoints.back().x;
    //         float lastY = mCurvePoints.back().y;
    //         if (lastX < width)
    //         {
    //             mCurvePath.lineTo(width, lastY);
    //             mCurvePoints.emplace_back(width, lastY);
    //         }
    //         break;
    //     }
    //
    //     case MultiFilter::Notch:
    //     {
    //         // For notch: the curve naturally has dips, extend both sides
    //         float firstX = mCurvePoints.front().x;
    //         float firstY = mCurvePoints.front().y;
    //         if (firstX > 0)
    //         {
    //             mCurvePath.lineTo(0, firstY);
    //             mCurvePoints.insert(mCurvePoints.begin(), juce::Point<float>(0, firstY));
    //         }
    //         float lastX = mCurvePoints.back().x;
    //         float lastY = mCurvePoints.back().y;
    //         if (lastX < width)
    //         {
    //             mCurvePath.lineTo(width, lastY);
    //             mCurvePoints.emplace_back(width, lastY);
    //         }
    //         break;
    //     }
    //
    //     default:
    //         break;
    // }

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
