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
    // b0 was not init, so init to 1.0   why ??
    MultiFilter() : a0(1.0), a1(0.0), a2(0.0), b0(1.0), b1(0.0), b2(0.0), prevX1(0.0), prevX2(0.0), prevY1(0.0), prevY2(0.0)
    {}

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

private:
    

    double samplingRate = 44100.0; 
    double cutoffFrequency = 1000.0; 
    double Q = 0.707; // Default quality factor

    float gainDB = 0.0;
    FilterType filterType = LowPass; // Default filter type

    // Filter coefficients
    double a0, a1, a2, b0, b1, b2;

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
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;


        // could put coefficients in member vector, that can be returned by public getCoefficients(), used by FilterCurveComponent
        // that access Filter from PluginProcessor, needs a get reference to filter
        
       
    }


   

};

