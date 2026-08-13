/*
  ==============================================================================

    FilterCoefficients.h - Thread-safe coefficient cache for biquad filter

    This class provides a shared, thread-safe container for filter coefficients
    that can be used by both audio processing (MultiFilter) and UI visualization
    (FilterCurveComponent).

    Thread Safety:
    - Uses std::atomic for lock-free, realtime-safe coefficient access
    - Writers call update() which atomically swaps the entire coefficient set
    - Readers call get() which atomically loads a consistent snapshot
    - Multiple concurrent readers are safe (they get their own copy)

    Realtime Safety:
    - No dynamic memory allocation on audio thread
    - No mutexes or locks - atomic operations only
    - Deterministic timing

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// Forward declaration to avoid circular dependency
class MultiFilter;

/** Thread-safe container for biquad filter coefficients. */
class FilterCoefficients
{
public:
    /** Structure holding all filter coefficients and their source parameters. */
    struct Coefficients
    {
        // Normalized coefficients (a0 is always 1.0)
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
        
        // Source parameters (for debugging/information)
        double sampleRate = 44100.0;
        double cutoffFrequency = 1000.0;
        double Q = 0.707;
        double gainDB = 0.0;
        int filterType = 0;  // MultiFilter::FilterType
    };

    /** Default constructor. */
    FilterCoefficients() = default;

    /** Update coefficients based on filter parameters.
     * 
     * This is called when filter parameters change. It calculates new coefficients
     * and atomically swaps them with the current set.
     * 
     * @param samplingRate     The audio sample rate
     * @param cutoffFreq       The cutoff frequency in Hz
     * @param resonance        The Q/resonance factor
     * @param gain             The gain in dB
     * @param type             The filter type (from MultiFilter::FilterType)
     */
    void update(double samplingRate, double cutoffFreq, double resonance,
                double gain, int type) noexcept
    {
        Coefficients newCoeffs;
        newCoeffs.sampleRate = samplingRate;
        newCoeffs.cutoffFrequency = cutoffFreq;
        newCoeffs.Q = resonance;
        newCoeffs.gainDB = gain;
        newCoeffs.filterType = type;
        
        calculateCoefficients(newCoeffs);
        
        // Atomic store with release ordering - ensures all writes to newCoeffs
        // are visible before the pointer becomes visible to other threads
        coefficients.store(newCoeffs, std::memory_order_release);
    }

    /** Get the current coefficients (thread-safe, lock-free).
     * 
     * Returns a copy of the current coefficients. This is safe to call from
     * any thread (audio or UI) concurrently.
     * 
     * @return A snapshot of the current coefficients
     */
    Coefficients get() const noexcept
    {
        // Atomic load with acquire ordering - ensures we see all writes
        // that happened before the store in update()
        return coefficients.load(std::memory_order_acquire);
    }

    /** Calculate magnitude response at a given frequency using current coefficients.
     * 
     * This is used by FilterCurveComponent to draw the frequency response curve.
     * 
     * @param frequency    The frequency to calculate magnitude at (in Hz)
     * @param sampleRate   The sample rate for the frequency calculation
     * @return Magnitude in dB
     */
    float calculateMagnitudeAt(float frequency, float sampleRate) const noexcept
    {
        auto coeffs = get();
        
        // Fallback to safe default if sample rate is invalid
        if (sampleRate <= 0.0f)
            return -30.0f;
        
        // Calculate omega for this frequency
        double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
        
        // Calculate magnitude response using the stored normalized coefficients
        // For a biquad filter: H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
        // Evaluated at z = e^(j*omega), magnitude = |H(e^(j*omega))|
        double cos_omega = std::cos(omega);
        double sin_omega = std::sin(omega);
        double cos_2omega = std::cos(2.0 * omega);
        double sin_2omega = std::sin(2.0 * omega);
        
        // Numerator: b0 + b1*e^(-j*omega) + b2*e^(-j*2*omega)
        // Real part: b0 + b1*cos(omega) + b2*cos(2*omega)
        // Imag part: -b1*sin(omega) - b2*sin(2*omega)
        double numReal = coeffs.b0 + coeffs.b1 * cos_omega + coeffs.b2 * cos_2omega;
        double numImag = -coeffs.b1 * sin_omega - coeffs.b2 * sin_2omega;
        
        // Denominator: 1 + a1*e^(-j*omega) + a2*e^(-j*2*omega)
        // Real part: 1 + a1*cos(omega) + a2*cos(2*omega)
        // Imag part: -a1*sin(omega) - a2*sin(2*omega)
        double denReal = 1.0 + coeffs.a1 * cos_omega + coeffs.a2 * cos_2omega;
        double denImag = -coeffs.a1 * sin_omega - coeffs.a2 * sin_2omega;
        
        // Magnitude squared = |numerator|^2 / |denominator|^2
        double magSquared = (numReal * numReal + numImag * numImag) / 
                          (denReal * denReal + denImag * denImag);
        
        // Handle edge cases
        if (magSquared <= 0.0 || std::isnan(magSquared) || std::isinf(magSquared))
            return -30.0f;  // Very low value for invalid cases
        
        // Clamp to prevent numerical issues with log10
        magSquared = std::max(1e-15, std::min(magSquared, 1e15));
        
        // Convert to dB: 20 * log10(|H|)
        return static_cast<float>(20.0 * std::log10(std::sqrt(magSquared)));
    }

    /** Check if coefficients have been initialized.
     * 
     * @return true if coefficients have been set at least once
     */
    bool isInitialized() const noexcept
    {
        auto coeffs = get();
        // Check if sampleRate has been set from its default
        return coeffs.sampleRate != 44100.0 || coeffs.cutoffFrequency != 1000.0;
    }

private:
    /** Calculate coefficients from parameters using biquad filter formulas.
     * 
     * Formulas adapted from Audio-EQ-Cookbook.txt by Robert Bristow-Johnson
     * https://www.w3.org/TR/audio-eq-cookbook/#formulae
     * 
     * @param coeffs   Reference to Coefficients struct to be filled in
     */
    void calculateCoefficients(Coefficients& coeffs) const noexcept
    {
        double omega = 2.0 * juce::MathConstants<double>::pi * 
                      coeffs.cutoffFrequency / coeffs.sampleRate;
        double alpha = std::sin(omega) / (2.0 * coeffs.Q);
        double cos_omega = std::cos(omega);
        double A = std::pow(10.0, coeffs.gainDB / 40.0);
        
        // Temporary variables for unnormalized coefficients
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a0 = 1.0, a1 = 0.0, a2 = 0.0;
        
        // Calculate coefficients based on filter type
        switch (coeffs.filterType)
        {
            case 0: // LowPass
                b0 = (1.0 - cos_omega) / 2.0;
                b1 = 1.0 - cos_omega;
                b2 = (1.0 - cos_omega) / 2.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cos_omega;
                a2 = 1.0 - alpha;
                break;

            case 1: // HighPass
                b0 = (1.0 + cos_omega) / 2.0;
                b1 = -(1.0 + cos_omega);
                b2 = (1.0 + cos_omega) / 2.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cos_omega;
                a2 = 1.0 - alpha;
                break;

            case 2: // BandPass
                b0 = alpha;
                b1 = 0.0;
                b2 = -alpha;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cos_omega;
                a2 = 1.0 - alpha;
                break;

            case 3: // Notch
                b0 = 1.0;
                b1 = -2.0 * cos_omega;
                b2 = 1.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cos_omega;
                a2 = 1.0 - alpha;
                break;

            case 4: // LowShelf
                b0 = A * ((A + 1.0) - (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha);
                b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cos_omega);
                b2 = A * ((A + 1.0) - (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha);
                a0 = (A + 1.0) + (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha;
                a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cos_omega);
                a2 = (A + 1.0) + (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha;
                break;

            case 5: // HighShelf
                b0 = A * ((A + 1.0) + (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha);
                b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cos_omega);
                b2 = A * ((A + 1.0) + (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha);
                a0 = (A + 1.0) - (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha;
                a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cos_omega);
                a2 = (A + 1.0) - (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha;
                break;

            default:
                // Unknown filter type - use identity (all-pass)
                b0 = 1.0; b1 = 0.0; b2 = 0.0;
                a0 = 1.0; a1 = 0.0; a2 = 0.0;
                break;
        }
        
        // Normalize by a0 (to make a0 = 1.0 in the final coefficients)
        // This avoids division in the audio processing loop
        if (a0 != 0.0)
        {
            coeffs.b0 = b0 / a0;
            coeffs.b1 = b1 / a0;
            coeffs.b2 = b2 / a0;
            coeffs.a1 = a1 / a0;
            coeffs.a2 = a2 / a0;
        }
        else
        {
            // Fallback to identity if a0 is zero
            coeffs.b0 = 1.0; coeffs.b1 = 0.0; coeffs.b2 = 0.0;
            coeffs.a1 = 0.0; coeffs.a2 = 0.0;
        }
    }

    // The atomic coefficient storage
    std::atomic<Coefficients> coefficients;
};
