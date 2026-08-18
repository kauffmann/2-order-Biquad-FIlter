/*
  ==============================================================================

    LowPassFilter.h
  
    Author:  Michael kauffmann

  ==============================================================================
*/

#pragma once



#include <JuceHeader.h>

/* Use: 
   
   1. Remember to instantiate 2 instances, one for each channel to avoid artefacts.
   Filters typically maintain internal state, such as previous input and output samples.
   If you share a single instance between channels, the state(like prevX1, prevY1, etc.)
   would be mixed and could lead to incorrect processing results, as the state for one channel would interfere with the other.
   
   2. If parameters change, the Filter must always call updateCoefficients() before processing.
   */
   


class MultiFilter
{
public:
    enum FilterType
    {
        LowPass = 0,
        HighPass,
        BandPass,
        Notch,
        HighShelf,
        LowShelf
    };
    // Initialize coefficients and state
    MultiFilter() : a0(1.0), a1(0.0), a2(0.0), b0(1.0), b1(0.0), b2(0.0), prevX1(0.0), prevX2(0.0), prevY1(0.0), prevY2(0.0)
    {}

    void setSamplingRate(double sampleRate)
    {
        samplingRate = sampleRate;

        // Reset smoothing with the audio sample rate and a 50ms smoothing time
        smoothedCutoffFreq.reset(samplingRate, 0.05); // Smooth over 50 ms

        // Ensure the smoothed value starts at the current cutoffFrequency to avoid jumps
        smoothedCutoffFreq.setCurrentAndTargetValue(cutoffFrequency);

        // Recalculate coefficients for the initial state
        updateCoefficients();
    }

    void setCutoffFrequency(float cutoffFreq)
    {
        // Only set the target value on the SmoothedValue. Do NOT immediately set
        // cutoffFrequency or call updateCoefficients() from the message/UI thread.
        // The audio thread will read the smoothed values in processSample() and
        // update coefficients there. This prevents abrupt coefficient changes that
        // cause audible clicks/pops when the user first moves the slider.
        smoothedCutoffFreq.setTargetValue(static_cast<double>(cutoffFreq));
    }


    void setResonans(float resonans)
    {
        Q = resonans;
        updateCoefficients();
    }

   

    void setGain(float gain)
    {
        gainDB = gain;
        updateCoefficients();
    }

    void setFilterType(int typeValue)
    {
        filterType = static_cast<FilterType>(typeValue);
        updateCoefficients();
    }

    void processSample(float& input)
    {
        // Always advance the smoothed value per-sample. If no smoothing is active
        // getNextValue() simply returns the same value. This is real-time safe.
        double newCutoff = smoothedCutoffFreq.getNextValue();

        // If cutoff actually changed, update the internal cutoff and recalc coeffs
        if (newCutoff != cutoffFrequency)
        {
            cutoffFrequency = newCutoff;
            updateCoefficients();
        }

        // Biquad IIR filter equation
        double output = b0 * input + b1 * prevX1 + b2 * prevX2 - a1 * prevY1 - a2 * prevY2;

        // Update previous samples
        prevX2 = prevX1;
        prevX1 = input;
        prevY2 = prevY1;
        prevY1 = output;

        input = static_cast<float>(output);
    }

private:
    double samplingRate = 44100.0; 
    double cutoffFrequency = 1000.0; 
    double Q = 0.707; // Default quality factor

    float gainDB = 0.0f;
    FilterType filterType = LowPass; // Default filter type

    // Filter coefficients
    double a0, a1, a2, b0, b1, b2;

    // Registers: previous input/output samples
    double prevX1, prevX2, prevY1, prevY2;

    juce::SmoothedValue<double> smoothedCutoffFreq;

    void updateCoefficients()
    {
        // Formulas from the Biquad Cookbook: https://www.w3.org/TR/audio-eq-cookbook/
        double omega = 2.0 * juce::MathConstants<double>::pi * cutoffFrequency / samplingRate;
        double alpha = std::sin(omega) / (2.0 * Q);
        double cos_omega = std::cos(omega);

        switch (filterType)
        {                                         
        case LowPass: 
            b0 = (1.0 - cos_omega) / 2.0;
            b1 = 1.0 - cos_omega;
            b2 = (1.0 - cos_omega) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_omega;
            a2 = 1.0 - alpha;
            break;

        case HighPass: 
            b0 = (1.0 + cos_omega) / 2.0;
            b1 = -(1.0 + cos_omega);
            b2 = (1.0 + cos_omega) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_omega;
            a2 = 1.0 - alpha;  
            break;

        case BandPass: 
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_omega;
            a2 = 1.0 - alpha;
            break;

        case Notch: 
            b0 = 1.0;
            b1 = -2.0 * cos_omega;
            b2 = 1.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_omega;
            a2 = 1.0 - alpha;
            break;

        case LowShelf:
        {
            double A = std::pow(10.0, gainDB / 40.0);
            b0 = A * ((A + 1) - (A - 1) * cos_omega + 2.0 * std::sqrt(A) * alpha);
            b1 = 2.0 * A * ((A - 1) - (A + 1) * cos_omega);
            b2 = A * ((A + 1) - (A - 1) * cos_omega - 2.0 * std::sqrt(A) * alpha);
            a0 = (A + 1) + (A - 1) * cos_omega + 2.0 * std::sqrt(A) * alpha;
            a1 = -2.0 * ((A - 1) + (A + 1) * cos_omega);
            a2 = (A + 1) + (A - 1) * cos_omega - 2.0 * std::sqrt(A) * alpha;
        }
        break;

        case HighShelf:
        {
            double A = std::pow(10.0, gainDB / 40.0);
            b0 = A * ((A + 1) + (A - 1) * cos_omega + 2.0 * std::sqrt(A) * alpha);
            b1 = -2.0 * A * ((A - 1) + (A + 1) * cos_omega);
            b2 = A * ((A + 1) + (A - 1) * cos_omega - 2.0 * std::sqrt(A) * alpha);
            a0 = (A + 1) - (A - 1) * cos_omega + 2.0 * std::sqrt(A) * alpha;
            a1 = 2.0 * ((A - 1) - (A + 1) * cos_omega);
            a2 = (A + 1) - (A - 1) * cos_omega - 2.0 * std::sqrt(A) * alpha;
        }
        break;

        default:
            break;
        }

        // Normalize coefficients by a0
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
    }
};
