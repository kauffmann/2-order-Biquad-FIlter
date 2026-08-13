/*
  ==============================================================================

    MultiFilter.h - Biquad filter with shared coefficient support
  
    Author:  Michael kauffmann

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "FilterCoefficients.h"

/* Use: 
  
  1. Remember to instantiate 2 instances, one for each channel to avoid artefacts.
  2. Pass a reference to FilterCoefficients to the constructor.
  3. If parameters change, the FilterCoefficients must be updated before processing.
   */



class MultiFilter
{
public:
    /** Filter type enum - matches FilterCoefficients */
    enum FilterType
    {
        LowPass = 0,
        HighPass,
        BandPass,
        Notch,
        HighShelf,
        LowShelf
    };

    /** Constructor with shared coefficient cache.
     * 
     * @param coeffCache  Reference to the shared FilterCoefficients object
     */
    explicit MultiFilter(FilterCoefficients& coeffCache) 
        : coefficients(&coeffCache),
          smoothedCutoffFreq(1000.0)
    {
        // Reset the smoothed value with proper sample rate and smoothing time
        smoothedCutoffFreq.reset(44100.0, 0.05); // Smooth over 50 ms
        
        // Initialize with default coefficients from cache
        resetToDefaultCoefficients();
    }

    void setSamplingRate(double sampleRate)
    {
        samplingRate = sampleRate;
        smoothedCutoffFreq.reset(sampleRate, 0.05); // Smooth over 50 ms
    }

    void setCutoffFrequency(float cutoffFreq)
    {
        smoothedCutoffFreq.setTargetValue(cutoffFreq);
        currentCutoff = cutoffFreq;
    }

    void setResonans(float resonans)
    {
        currentQ = resonans;
    }

    void setGain(float gain)
    {
        currentGainDB = gain;
    }

    void setFilterType(int typeValue)
    {
        currentFilterType = static_cast<FilterType>(typeValue);
    }

    /** Get the current filter type as an integer. */
    int getFilterType() const noexcept { return static_cast<int>(currentFilterType); }

    /** Get the current sampling rate. */
    double getSamplingRate() const noexcept { return samplingRate; }

    /** Process a single audio sample.
     * 
     * This uses the coefficients from the shared FilterCoefficients cache.
     * If smoothing is enabled for the cutoff frequency, it will use the smoothed
     * cutoff value, otherwise it uses the current coefficients from the cache.
     */
    void processSample(float& input)
    {
        // Check if we need to apply smoothing to cutoff frequency
        if (useSmoothing && smoothedCutoffFreq.isSmoothing())
        {
            // Get smoothed cutoff and recalculate coefficients for this filter instance
            currentCutoff = smoothedCutoffFreq.getNextValue();
            updateLocalCoefficients();
            
            // Use local smoothed coefficients
            double output = b0 * input + b1 * prevX1 + b2 * prevX2 - a1 * prevY1 - a2 * prevY2;
            updateRegisters(input, output);
        }
        else
        {
            // Use shared coefficients from the cache (no smoothing or smoothing complete)
            auto coeffs = coefficients->get();
            double output = coeffs.b0 * input + coeffs.b1 * prevX1 + coeffs.b2 * prevX2 
                          - coeffs.a1 * prevY1 - coeffs.a2 * prevY2;
            updateRegisters(input, output);
        }
    }

    /** Enable or disable cutoff frequency smoothing.
     * 
     * When enabled, each filter instance will smoothly transition to new cutoff values.
     * When disabled, coefficients are taken directly from the shared cache.
     * 
     * @param enable  true to enable smoothing, false to disable
     */
    void setSmoothingEnabled(bool enable) noexcept { useSmoothing = enable; }

    /** Check if smoothing is enabled. */
    bool isSmoothingEnabled() const noexcept { return useSmoothing; }

private:
    // Reference to shared coefficient cache
    FilterCoefficients* coefficients;
    
    // Local state for each filter instance (per-channel)
    double samplingRate = 44100.0;
    double currentCutoff = 1000.0;  // Current cutoff (target or smoothed)
    double currentQ = 0.707;         // Current Q factor
    double currentGainDB = 0.0;      // Current gain in dB
    FilterType currentFilterType = LowPass; // Current filter type
    
    // Local smoothed cutoff for per-channel smoothing
    juce::SmoothedValue<double> smoothedCutoffFreq;
    
    // Local coefficient cache for when smoothing is active
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;
    
    // Registers: previous input/output samples (per-channel state)
    double prevX1 = 0.0, prevX2 = 0.0, prevY1 = 0.0, prevY2 = 0.0;
    
    // Whether to use smoothing for this filter instance
    bool useSmoothing = true;

    /** Initialize with default coefficients from the cache. */
    void resetToDefaultCoefficients()
    {
        auto coeffs = coefficients->get();
        b0 = coeffs.b0; b1 = coeffs.b1; b2 = coeffs.b2;
        a1 = coeffs.a1; a2 = coeffs.a2;
        currentCutoff = coeffs.cutoffFrequency;
        currentQ = coeffs.Q;
        currentGainDB = coeffs.gainDB;
        currentFilterType = static_cast<FilterType>(coeffs.filterType);
        samplingRate = coeffs.sampleRate;
    }

    /** Update local coefficients when smoothing is active.
     * 
     * This recalculates coefficients based on the current smoothed parameters.
     * Note: This is only used when useSmoothing is true and smoothing is active.
     */
    void updateLocalCoefficients()
    {
        double omega = 2.0 * juce::MathConstants<double>::pi * currentCutoff / samplingRate;
        double alpha = std::sin(omega) / (2.0 * currentQ);
        double cos_omega = std::cos(omega);
        double A = std::pow(10.0, currentGainDB / 40.0);
        
        double b0_temp = 1.0, b1_temp = 0.0, b2_temp = 0.0;
        double a0_temp = 1.0, a1_temp = 0.0, a2_temp = 0.0;
        
        switch (currentFilterType)
        {
            case LowPass:
                b0_temp = (1.0 - cos_omega) / 2.0;
                b1_temp = 1.0 - cos_omega;
                b2_temp = (1.0 - cos_omega) / 2.0;
                a0_temp = 1.0 + alpha;
                a1_temp = -2.0 * cos_omega;
                a2_temp = 1.0 - alpha;
                break;

            case HighPass:
                b0_temp = (1.0 + cos_omega) / 2.0;
                b1_temp = -(1.0 + cos_omega);
                b2_temp = (1.0 + cos_omega) / 2.0;
                a0_temp = 1.0 + alpha;
                a1_temp = -2.0 * cos_omega;
                a2_temp = 1.0 - alpha;
                break;

            case BandPass:
                b0_temp = alpha;
                b1_temp = 0.0;
                b2_temp = -alpha;
                a0_temp = 1.0 + alpha;
                a1_temp = -2.0 * cos_omega;
                a2_temp = 1.0 - alpha;
                break;

            case Notch:
                b0_temp = 1.0;
                b1_temp = -2.0 * cos_omega;
                b2_temp = 1.0;
                a0_temp = 1.0 + alpha;
                a1_temp = -2.0 * cos_omega;
                a2_temp = 1.0 - alpha;
                break;

            case LowShelf:
                b0_temp = A * ((A + 1.0) - (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha);
                b1_temp = 2.0 * A * ((A - 1.0) - (A + 1.0) * cos_omega);
                b2_temp = A * ((A + 1.0) - (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha);
                a0_temp = (A + 1.0) + (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha;
                a1_temp = -2.0 * ((A - 1.0) + (A + 1.0) * cos_omega);
                a2_temp = (A + 1.0) + (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha;
                break;

            case HighShelf:
                b0_temp = A * ((A + 1.0) + (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha);
                b1_temp = -2.0 * A * ((A - 1.0) + (A + 1.0) * cos_omega);
                b2_temp = A * ((A + 1.0) + (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha);
                a0_temp = (A + 1.0) - (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha;
                a1_temp = 2.0 * ((A - 1.0) - (A + 1.0) * cos_omega);
                a2_temp = (A + 1.0) - (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha;
                break;

            default:
                b0_temp = 1.0; b1_temp = 0.0; b2_temp = 0.0;
                a0_temp = 1.0; a1_temp = 0.0; a2_temp = 0.0;
                break;
        }
        
        // Normalize by a0
        if (a0_temp != 0.0)
        {
            b0 = b0_temp / a0_temp;
            b1 = b1_temp / a0_temp;
            b2 = b2_temp / a0_temp;
            a1 = a1_temp / a0_temp;
            a2 = a2_temp / a0_temp;
        }
        else
        {
            b0 = 1.0; b1 = 0.0; b2 = 0.0;
            a1 = 0.0; a2 = 0.0;
        }
    }

    /** Update the delay registers after processing a sample. */
    void updateRegisters(float input, double output) noexcept
    {
        prevX2 = prevX1;
        prevX1 = input;
        prevY2 = prevY1;
        prevY1 = output;
        input = static_cast<float>(output);
    }

    
};
