/*
  ==============================================================================

    LowPassFilter.h
  
    Author:  Michael kauffmann

  ==============================================================================
*/

#pragma once



#include <JuceHeader.h>
#include <atomic>
#include <array>

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

    // Coefficient indices for readable access
    enum CoeffIndex
    {
        B0 = 0,
        B1 = 1,
        B2 = 2,
        A0 = 3,
        A1 = 4,
        A2 = 5,
        COEFF_COUNT = 6
    };

    // b0 was not init, so init to 1.0   why ??
    MultiFilter() : prevX1(0.0), prevX2(0.0), prevY1(0.0), prevY2(0.0)
    {
        // Initialize atomics to sensible defaults (unity gain)
        for (int i = 0; i < COEFF_COUNT; ++i)
            mCoeffs[i].store(0.0);
        mCoeffs[B0].store(1.0);
        mCoeffs[A0].store(1.0);
    }

    void setSamplingRate(double sampleRate)
    {
        samplingRate = sampleRate;
        smoothedCutoffFreq.reset(samplingRate, 0.05); // Smooth over 50 ms
        updateCoefficients();
    }

    void setCutoffFrequency(float cutoffFreq)
    {
        smoothedCutoffFreq.setTargetValue(cutoffFreq);
        cutoffFrequency = cutoffFreq;
        updateCoefficients();
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
        // Smooth the cutoff frequency and update coefficients only if there is a change.
        if (smoothedCutoffFreq.isSmoothing())
        {
            cutoffFrequency = smoothedCutoffFreq.getNextValue();
            updateCoefficients();
        }

        // Load coefficients once (atomic loads) to local doubles for faster access in inner loop
        double b0 = mCoeffs[B0].load(std::memory_order_relaxed);
        double b1 = mCoeffs[B1].load(std::memory_order_relaxed);
        double b2 = mCoeffs[B2].load(std::memory_order_relaxed);
        double a1 = mCoeffs[A1].load(std::memory_order_relaxed);
        double a2 = mCoeffs[A2].load(std::memory_order_relaxed);

        // Implementing the Biquad IIR filter equation. Is recursive as each iteration  store current levels in register and are recalled in next iteration. 
        // it is second order filter, as it uses 2 z^-1 delay blocks. A serie of difference equations. Kind of convolution, without flip and frame.
        
                      // feedforward                          // feedbackward. substract to avoid unstable filter.
        double output = b0 * input + b1 * prevX1 + b2 * prevX2 - a1 * prevY1 - a2 * prevY2;

        // Update previous register samples. A Biqard filter diagram/image support this.
        prevX2 = prevX1;   // 2 step put x1 in register
        prevX1 = input;   // 1 step put input in register
        prevY2 = prevY1;   // 2 step put y1 in register
        prevY1 = output;   // 1 step put output in register

        input = static_cast<float>(output);
        
    }

    // Return a snapshot of the current coefficients (atomic loads)
    std::array<double, COEFF_COUNT> getCoefficients() const
    {
        std::array<double, COEFF_COUNT> out;
        for (int i = 0; i < COEFF_COUNT; ++i)
            out[i] = mCoeffs[i].load(std::memory_order_relaxed);
        return out;
    }

private:
    

    double samplingRate = 44100.0; 
    double cutoffFrequency = 1000.0; 
    double Q = 0.707; // Default quality factor

    float gainDB = 0.0;
    FilterType filterType = LowPass; // Default filter type

    // Filter coefficients stored atomically to allow lock-free cross-thread reads
    mutable std::array<std::atomic<double>, COEFF_COUNT> mCoeffs;

    // Registers: previous input/output samples
    double prevX1, prevX2, prevY1, prevY2;

    juce::SmoothedValue<double> smoothedCutoffFreq;

    void updateCoefficients()
    {
        

        // Formulas from the Biquad Cookbook.  Adapted from Audio-EQ-Cookbook.txt, by Robert Bristow-Johnson https://www.w3.org/TR/audio-eq-cookbook/#formulae
        // https://github.com/shepazu/Audio-EQ-Cookbook/blob/master/Audio-EQ-Cookbook.txt

        //  Omega or greek w represents a frequency in terms of angular measure (in radians).  

        double omega = 2.0 * juce::MathConstants<double>::pi * cutoffFrequency / samplingRate;
        double alpha = sin(omega) / (2.0 * Q);
        double cos_omega = cos(omega);

        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a0 = 1.0, a1 = 0.0, a2 = 0.0;

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
            double A = pow(10, gainDB / 40.0);
            b0 = A * ((A + 1) - (A - 1) * cos_omega + 2 * std::sqrt(A) * alpha);
            b1 = 2 * A * ((A - 1) - (A + 1) * cos_omega);
            b2 = A * ((A + 1) - (A - 1) * cos_omega - 2 * std::sqrt(A) * alpha);
            a0 = (A + 1) + (A - 1) * cos_omega + 2 * std::sqrt(A) * alpha;
            a1 = -2 * ((A - 1) + (A + 1) * cos_omega);
            a2 = (A + 1) + (A - 1) * cos_omega - 2 * std::sqrt(A) * alpha;
        }
        break;

        case HighShelf:
        {
            double A = pow(10, gainDB / 40.0);
            b0 = A * ((A + 1) + (A - 1) * cos_omega + 2 * std::sqrt(A) * alpha);
            b1 = -2 * A * ((A - 1) + (A + 1) * cos_omega);
            b2 = A * ((A + 1) + (A - 1) * cos_omega - 2 * std::sqrt(A) * alpha);
            a0 = (A + 1) - (A - 1) * cos_omega + 2 * std::sqrt(A) * alpha;
            a1 = 2 * ((A - 1) - (A + 1) * cos_omega);
            a2 = (A + 1) - (A - 1) * cos_omega - 2 * std::sqrt(A) * alpha;
        }
        break;


        default:
            break;
        }


        // Normalize coefficients by a0
        double nb0 = b0 / a0;
        double nb1 = b1 / a0;
        double nb2 = b2 / a0;
        double na1 = a1 / a0;
        double na2 = a2 / a0;

        // Publish the coefficients atomically. We use relaxed ordering for performance; this is sufficient
        // for the UI which can tolerate transient inconsistencies for a single frame.
        mCoeffs[B0].store(nb0, std::memory_order_relaxed);
        mCoeffs[B1].store(nb1, std::memory_order_relaxed);
        mCoeffs[B2].store(nb2, std::memory_order_relaxed);
        mCoeffs[A0].store(a0, std::memory_order_relaxed); // raw a0 kept for completeness
        mCoeffs[A1].store(na1, std::memory_order_relaxed);
        mCoeffs[A2].store(na2, std::memory_order_relaxed);

        
    }

   

};
