#include "PluginProcessor.h"

#include <gtest/gtest.h>

#include <cmath>

namespace
{
float normalizedValue(juce::AudioProcessorValueTreeState& state, const char* id, float value)
{
    auto* parameter = state.getParameter(id);
    return parameter->convertTo0to1(value);
}

float processSine(FilterAudioProcessor& processor, double frequency, int channel = 0)
{
    juce::AudioBuffer<float> buffer(2, 48000);
    buffer.clear();
    auto* data = buffer.getWritePointer(channel);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        data[i] = static_cast<float>(
            std::sin(2.0 * juce::MathConstants<double>::pi * frequency * i / 48000.0));

    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    double sum = 0.0;
    for (int i = 24000; i < buffer.getNumSamples(); ++i)
        sum += static_cast<double>(data[i]) * data[i];

    return static_cast<float>(std::sqrt(sum / 24000.0) / std::sqrt(0.5));
}
}

TEST(FilterAudioProcessorTest, ExposesExpectedParameters)
{
    FilterAudioProcessor processor;
    auto& state = processor.getApvts();

    ASSERT_NE(state.getParameter("CUTOFF"), nullptr);
    ASSERT_NE(state.getParameter("RESONANCE"), nullptr);
    ASSERT_NE(state.getParameter("GAIN"), nullptr);
    ASSERT_NE(state.getParameter("FILTER"), nullptr);
    EXPECT_FLOAT_EQ(state.getRawParameterValue("CUTOFF")->load(), 1000.0f);
    EXPECT_FLOAT_EQ(state.getRawParameterValue("RESONANCE")->load(), 0.707f);
    EXPECT_FLOAT_EQ(state.getRawParameterValue("GAIN")->load(), 0.0f);
    EXPECT_FLOAT_EQ(state.getRawParameterValue("FILTER")->load(), 0.0f);
}

TEST(FilterAudioProcessorTest, ProcessesMonoAndStereoBuffers)
{
    FilterAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            EXPECT_TRUE(std::isfinite(buffer.getSample(channel, sample)));
}

TEST(FilterAudioProcessorTest, ParameterChangesAffectBothChannels)
{
    FilterAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    auto& state = processor.getApvts();

    state.getParameter("FILTER")->setValueNotifyingHost(
        normalizedValue(state, "FILTER", 1.0f));
    state.getParameter("CUTOFF")->setValueNotifyingHost(
        normalizedValue(state, "CUTOFF", 500.0f));

    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        buffer.setSample(0, i, 1.0f);
        buffer.setSample(1, i, 1.0f);
    }

    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        EXPECT_FLOAT_EQ(buffer.getSample(0, i), buffer.getSample(1, i));
}

TEST(FilterAudioProcessorTest, StateRoundTripRestoresParameters)
{
    FilterAudioProcessor source;
    auto& sourceState = source.getApvts();
    sourceState.getParameter("CUTOFF")->setValueNotifyingHost(
        normalizedValue(sourceState, "CUTOFF", 4200.0f));
    sourceState.getParameter("RESONANCE")->setValueNotifyingHost(
        normalizedValue(sourceState, "RESONANCE", 2.5f));
    sourceState.getParameter("GAIN")->setValueNotifyingHost(
        normalizedValue(sourceState, "GAIN", -6.0f));
    sourceState.getParameter("FILTER")->setValueNotifyingHost(
        normalizedValue(sourceState, "FILTER", 5.0f));

    juce::MemoryBlock stateData;
    source.getStateInformation(stateData);

    FilterAudioProcessor restored;
    restored.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
    auto& restoredState = restored.getApvts();

    EXPECT_NEAR(restoredState.getRawParameterValue("CUTOFF")->load(), 4200.0f, 0.01f);
    EXPECT_NEAR(restoredState.getRawParameterValue("RESONANCE")->load(), 2.5f, 0.01f);
    EXPECT_FLOAT_EQ(restoredState.getRawParameterValue("GAIN")->load(), -6.0f);
    EXPECT_FLOAT_EQ(restoredState.getRawParameterValue("FILTER")->load(), 5.0f);
}

TEST(FilterAudioProcessorTest, LowPassAndHighPassProduceDifferentResponses)
{
    FilterAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    auto& state = processor.getApvts();

    state.getParameter("FILTER")->setValueNotifyingHost(
        normalizedValue(state, "FILTER", 0.0f));
    const auto lowPassGain = processSine(processor, 10000.0);

    state.getParameter("FILTER")->setValueNotifyingHost(
        normalizedValue(state, "FILTER", 1.0f));
    const auto highPassGain = processSine(processor, 10000.0);

    EXPECT_LT(lowPassGain, highPassGain);
}
