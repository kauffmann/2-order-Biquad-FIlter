#include "MultiFilter.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr double pi = std::numbers::pi_v<double>;

double rms(const std::vector<float>& samples, std::size_t start)
{
    double sum = 0.0;
    for (std::size_t i = start; i < samples.size(); ++i)
        sum += static_cast<double>(samples[i]) * samples[i];

    return std::sqrt(sum / static_cast<double>(samples.size() - start));
}

double measureSine(MultiFilter::FilterType type, double frequency, double gain = 0.0)
{
    MultiFilter filter;
    filter.setSamplingRate(sampleRate);
    filter.setCutoffFrequency(1000.0f);
    filter.setResonance(0.707f);
    filter.setGain(static_cast<float>(gain));
    filter.setFilterType(static_cast<int>(type));

    constexpr std::size_t sampleCount = 48000;
    constexpr std::size_t warmup = 24000;
    std::vector<float> output(sampleCount);

    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        float sample = static_cast<float>(
            std::sin(2.0 * pi * frequency * static_cast<double>(i) / sampleRate));
        filter.processSample(sample);
        output[i] = sample;
    }

    return rms(output, warmup) / std::sqrt(0.5);
}

void expectFinite(MultiFilter::FilterType type, double cutoff, double q, double gain)
{
    MultiFilter filter;
    filter.setSamplingRate(sampleRate);
    filter.setCutoffFrequency(static_cast<float>(cutoff));
    filter.setResonance(static_cast<float>(q));
    filter.setGain(static_cast<float>(gain));
    filter.setFilterType(static_cast<int>(type));

    std::mt19937 generator(42);
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

    for (int i = 0; i < 100000; ++i)
    {
        float sample = distribution(generator);
        filter.processSample(sample);
        ASSERT_TRUE(std::isfinite(sample));
        ASSERT_LE(std::abs(sample), 1.0e6f);
    }
}
}

TEST(MultiFilterTest, EveryFilterTypeProducesFiniteOutput)
{
    for (int type = MultiFilter::LowPass; type <= MultiFilter::LowShelf; ++type)
        expectFinite(static_cast<MultiFilter::FilterType>(type), 1000.0, 0.707, 6.0);
}

TEST(MultiFilterTest, LowPassAndHighPassHaveExpectedFrequencyBehavior)
{
    EXPECT_GT(measureSine(MultiFilter::LowPass, 100.0), measureSine(MultiFilter::LowPass, 10000.0));
    EXPECT_LT(measureSine(MultiFilter::HighPass, 100.0), measureSine(MultiFilter::HighPass, 10000.0));
}

TEST(MultiFilterTest, BandPassAndNotchAreCenteredAtCutoff)
{
    const auto bandPassAtCutoff = measureSine(MultiFilter::BandPass, 1000.0);
    const auto bandPassAway = measureSine(MultiFilter::BandPass, 100.0);
    const auto notchAtCutoff = measureSine(MultiFilter::Notch, 1000.0);

    EXPECT_GT(bandPassAtCutoff, bandPassAway);
    EXPECT_LT(notchAtCutoff, 0.1);
}

TEST(MultiFilterTest, ShelvesRespectGainAndZeroDbIsUnity)
{
    EXPECT_NEAR(measureSine(MultiFilter::LowShelf, 100.0, 6.0), std::pow(10.0, 6.0 / 20.0), 0.08);
    EXPECT_NEAR(measureSine(MultiFilter::HighShelf, 10000.0, -6.0), std::pow(10.0, -6.0 / 20.0), 0.08);
    EXPECT_NEAR(measureSine(MultiFilter::LowShelf, 100.0, 0.0), 1.0, 0.02);
    EXPECT_NEAR(measureSine(MultiFilter::HighShelf, 10000.0, 0.0), 1.0, 0.02);
}

TEST(MultiFilterTest, CutoffAndQEdgeValuesRemainFinite)
{
    expectFinite(MultiFilter::LowPass, -100.0, -1.0, 0.0);
    expectFinite(MultiFilter::HighPass, 1000000.0, 1000.0, 0.0);
}

TEST(MultiFilterTest, SampleRateResetClearsHistory)
{
    MultiFilter filter;
    filter.setSamplingRate(sampleRate);
    filter.setFilterType(MultiFilter::LowPass);

    float impulse = 1.0f;
    filter.processSample(impulse);
    ASSERT_NE(impulse, 0.0f);

    filter.setSamplingRate(44100.0);
    float silence = 0.0f;
    filter.processSample(silence);
    EXPECT_NEAR(silence, 0.0f, std::numeric_limits<float>::epsilon());
}

TEST(MultiFilterTest, CutoffSmoothingEventuallyReachesTarget)
{
    MultiFilter filter;
    filter.setSamplingRate(sampleRate);
    filter.setFilterType(MultiFilter::LowPass);
    filter.setCutoffFrequency(12000.0f);

    for (int i = 0; i < 48000; ++i)
    {
        float sample = 0.0f;
        filter.processSample(sample);
        ASSERT_TRUE(std::isfinite(sample));
    }

    const auto lowFrequencyGain = measureSine(MultiFilter::LowPass, 100.0);
    EXPECT_NEAR(lowFrequencyGain, 1.0, 0.02);
}
