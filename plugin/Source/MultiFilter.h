/*
  ==============================================================================

    MultiFilter.h

    Author:  Michael Kauffmann

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <cmath>

/*
  MultiFilter: 2nd-order biquad filter with lock-free coefficient publishing.

  Design:
  - Coefficients are published using a double-buffer + atomic index (active buffer).
    Writers (audio-thread) compute the full coefficient set into the inactive buffer
    and then atomically flip the active index (memory_order_release). Readers load
    the index with memory_order_acquire and read a consistent snapshot of coefficients.

  - Parameter setters are non-blocking and thread-safe: they store target values
    into atomics. The audio thread consumes those targets, applies smoothing via
    juce::SmoothedValue, computes coefficients, and publishes them. This prevents
    message-thread publication of unsmoothed coefficients and avoids audio pops.

  - Optimization (Option A): recompute & publish only when parameter changes exceed
    small thresholds, and ensure coefficients are published at block start to keep
    UI and audio in sync.

  References:
  - RBJ Audio EQ Cookbook: https://www.w3.org/TR/audio-eq-cookbook/
  - std::atomic (acquire/release semantics): https://en.cppreference.com/w/cpp/atomic/atomic
  - JUCE realtime guidelines: https://docs.juce.com/master/tutorial_audio_plugin.html
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

    MultiFilter()
        : samplingRate(44100.0), cutoffFrequency(1000.0), Q(0.707), gainDB(0.0), filterType(LowPass)
    {
        // initialize buffers with unity/no-op filter
        std::array<double, COEFF_COUNT> init{};
        init[B0] = 1.0;
        init[A0] = 1.0;
        coeffBuffers[0] = init;
        coeffBuffers[1] = init;
        activeCoeffBuffer.store(0, std::memory_order_relaxed);

        // initialize targets to defaults
        mTargetCutoff.store(static_cast<double>(cutoffFrequency), std::memory_order_relaxed);
        mTargetQ.store(static_cast<double>(Q), std::memory_order_relaxed);
        mTargetGain.store(static_cast<double>(gainDB), std::memory_order_relaxed);
        mTargetFilterType.store(static_cast<int>(filterType), std::memory_order_relaxed);

        smoothedCutoff.reset(samplingRate, 0.05);
        smoothedCutoff.setCurrentAndTargetValue(cutoffFrequency);

        // initialize published state mirrors
        lastPublishedCutoff = cutoffFrequency;
        lastPublishedQ = Q;
        lastPublishedGain = gainDB;
        lastPublishedType = static_cast<int>(filterType);
    }

    // Public setters: only publish targets to atomics. Audio thread will consume.
    void setSamplingRate(double sampleRate)
    {
        samplingRate = sampleRate;
        smoothedCutoff.reset(samplingRate, 0.05); // Smooth over 50 ms
        // ensure smoothed value uses current cutoff as base
        smoothedCutoff.setCurrentAndTargetValue(cutoffFrequency);
    }

    void setCutoffFrequency(float cutoffFreq)
    {
        mTargetCutoff.store(static_cast<double>(cutoffFreq), std::memory_order_relaxed);
    }

    void setResonans(float resonans)
    {
        mTargetQ.store(static_cast<double>(resonans), std::memory_order_relaxed);
    }

    void setGain(float gain)
    {
        mTargetGain.store(static_cast<double>(gain), std::memory_order_relaxed);
    }

    void setFilterType(int typeValue)
    {
        mTargetFilterType.store(typeValue, std::memory_order_relaxed);
    }

    // Called at block start by the host thread (via processBlock) to ensure a
    // coefficient snapshot is available for the upcoming block (keeps UI & audio in sync).
    void ensureCoefficientsPublishedForBlock()
    {
        // Read parameter targets and update internal state accordingly (non-smoothing for Q/gain/type)
        double targetCut = mTargetCutoff.load(std::memory_order_relaxed);
        smoothedCutoff.setTargetValue(targetCut);

        double currentCut = smoothedCutoff.getCurrentValue();
        // Update Q/gain/type from targets so compute uses latest values
        double targetQ = mTargetQ.load(std::memory_order_relaxed);
        if (std::abs(targetQ - Q) > 1e-12)
            Q = targetQ;

        double targetGain = mTargetGain.load(std::memory_order_relaxed);
        if (std::abs(targetGain - static_cast<double>(gainDB)) > 1e-12)
            gainDB = static_cast<float>(targetGain);

        int targetType = mTargetFilterType.load(std::memory_order_relaxed);
        if (targetType != static_cast<int>(filterType))
            filterType = static_cast<FilterType>(targetType);

        // Decide whether to publish based on thresholds
        if (std::abs(currentCut - lastPublishedCutoff) > cutoffEpsilon
            || std::abs(Q - lastPublishedQ) > paramEpsilon
            || std::abs(static_cast<double>(gainDB) - lastPublishedGain) > paramEpsilon
            || targetType != lastPublishedType)
        {
            // Sync cutoffFrequency to current value used for computation
            cutoffFrequency = currentCut;
            computeAndPublishCoefficients();
        }
    }

    // Process a single sample (audio thread). This is where smoothing and
    // coefficient publishing happen. Real-time safe: no locks, no allocations.
    void processSample(float& input)
    {
        // 1) Consume parameter targets and update internal state as needed.
        bool needRecompute = false;

        // Cutoff: advance smoothing one step on audio thread
        double targetCut = mTargetCutoff.load(std::memory_order_relaxed);
        smoothedCutoff.setTargetValue(targetCut);

        if (smoothedCutoff.isSmoothing())
        {
            cutoffFrequency = smoothedCutoff.getNextValue();
            if (std::abs(cutoffFrequency - lastPublishedCutoff) > cutoffEpsilon)
                needRecompute = true;
        }
        else
        {
            double next = smoothedCutoff.getCurrentValue();
            if (std::abs(next - lastPublishedCutoff) > cutoffEpsilon)
            {
                cutoffFrequency = next;
                needRecompute = true;
            }
        }

        // Q (resonance)
        double targetQ = mTargetQ.load(std::memory_order_relaxed);
        if (std::abs(targetQ - Q) > 1e-12)
        {
            Q = targetQ;
            if (std::abs(Q - lastPublishedQ) > paramEpsilon)
                needRecompute = true;
        }

        // Gain
        double targetGain = mTargetGain.load(std::memory_order_relaxed);
        if (std::abs(targetGain - static_cast<double>(gainDB)) > 1e-12)
        {
            gainDB = static_cast<float>(targetGain);
            if (std::abs(static_cast<double>(gainDB) - lastPublishedGain) > paramEpsilon)
                needRecompute = true;
        }

        // Filter type
        int targetType = mTargetFilterType.load(std::memory_order_relaxed);
        if (targetType != static_cast<int>(filterType))
        {
            filterType = static_cast<FilterType>(targetType);
            if (targetType != lastPublishedType)
                needRecompute = true;
        }

        // Recompute and publish coefficients if needed (audio thread)
        if (needRecompute)
            computeAndPublishCoefficients();

        // 2) Load coefficients snapshot once (acquire the active buffer index)
        int idx = activeCoeffBuffer.load(std::memory_order_acquire);
        const auto& c = coeffBuffers[idx];

        double b0 = c[B0];
        double b1 = c[B1];
        double b2 = c[B2];
        double a1 = c[A1];
        double a2 = c[A2];

        // Biquad processing
        double output = b0 * input + b1 * prevX1 + b2 * prevX2 - a1 * prevY1 - a2 * prevY2;

        prevX2 = prevX1;
        prevX1 = input;
        prevY2 = prevY1;
        prevY1 = output;

        input = static_cast<float>(output);
    }

    // Return a snapshot of the current coefficients for UI (message thread):
    // loads active index with acquire semantics and copies the buffer.
    std::array<double, COEFF_COUNT> getCoefficients() const
    {
        std::array<double, COEFF_COUNT> out;
        int idx = activeCoeffBuffer.load(std::memory_order_acquire);
        out = coeffBuffers[idx];
        return out;
    }

private:
    double samplingRate; 
    double cutoffFrequency; 
    double Q; // quality factor

    float gainDB;
    FilterType filterType;

    // double-buffered coefficients
    std::array<std::array<double, COEFF_COUNT>, 2> coeffBuffers;
    std::atomic<int> activeCoeffBuffer{0}; // index 0 or 1

    // Parameter targets written from message thread
    std::atomic<double> mTargetCutoff{1000.0};
    std::atomic<double> mTargetQ{0.707};
    std::atomic<double> mTargetGain{0.0};
    std::atomic<int>    mTargetFilterType{static_cast<int>(FilterType::LowPass)};

    // last published parameter mirrors (to avoid excessive republishes)
    double lastPublishedCutoff{0.0};
    double lastPublishedQ{0.0};
    double lastPublishedGain{0.0};
    int    lastPublishedType{0};

    // thresholds
    static constexpr double cutoffEpsilon = 0.01; // Hz
    static constexpr double paramEpsilon = 1e-6;  

    // Registers: previous input/output samples
    double prevX1{0.0}, prevX2{0.0}, prevY1{0.0}, prevY2{0.0};

    juce::SmoothedValue<double> smoothedCutoff;

    // Compute coefficients based on current parameters and publish into inactive buffer,
    // then atomically flip the active buffer index.
    void computeAndPublishCoefficients()
    {
        // Compute RBJ coefficients locally
        double omega = 2.0 * juce::MathConstants<double>::pi * cutoffFrequency / samplingRate;
        double alpha = std::sin(omega) / (2.0 * Q);
        double cos_omega = std::cos(omega);

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
                double A = std::pow(10.0, gainDB / 40.0);
                b0 = A * ((A + 1.0) - (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha);
                b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cos_omega);
                b2 = A * ((A + 1.0) - (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha);
                a0 = (A + 1.0) + (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha;
                a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cos_omega);
                a2 = (A + 1.0) + (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha;
            }
            break;

            case HighShelf:
            {
                double A = std::pow(10.0, gainDB / 40.0);
                b0 = A * ((A + 1.0) + (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha);
                b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cos_omega);
                b2 = A * ((A + 1.0) + (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha);
                a0 = (A + 1.0) - (A - 1.0) * cos_omega + 2.0 * std::sqrt(A) * alpha;
                a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cos_omega);
                a2 = (A + 1.0) - (A - 1.0) * cos_omega - 2.0 * std::sqrt(A) * alpha;
            }
            break;

            default:
                break;
        }

        // Normalize
        double nb0 = b0 / a0;
        double nb1 = b1 / a0;
        double nb2 = b2 / a0;
        double na1 = a1 / a0;
        double na2 = a2 / a0;

        // Publish into inactive buffer and flip
        int inactive = 1 - activeCoeffBuffer.load(std::memory_order_relaxed);
        auto& buf = coeffBuffers[inactive];
        buf[B0] = nb0;
        buf[B1] = nb1;
        buf[B2] = nb2;
        buf[A0] = a0;   // store raw a0 for completeness (not used in processing)
        buf[A1] = na1;
        buf[A2] = na2;

        // Ensure published buffer is visible before flipping
        activeCoeffBuffer.store(inactive, std::memory_order_release);

        // update last published mirrors
        lastPublishedCutoff = cutoffFrequency;
        lastPublishedQ = Q;
        lastPublishedGain = static_cast<double>(gainDB);
        lastPublishedType = static_cast<int>(filterType);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiFilter)
};
