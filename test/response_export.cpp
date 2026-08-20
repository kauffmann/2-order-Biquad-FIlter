#include "MultiFilter.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: dsp_response_export <csv-path>\n";
        return 2;
    }

    std::ofstream output(argv[1]);
    if (!output)
    {
        std::cerr << "Could not open output file: " << argv[1] << '\n';
        return 1;
    }

    constexpr double sampleRate = 48000.0;
    constexpr double pi = std::numbers::pi_v<double>;
    constexpr int sampleCount = 48000;
    constexpr int warmup = 24000;

    output << "filter,frequency_hz,magnitude_db\n";

    for (int type = MultiFilter::LowPass; type <= MultiFilter::LowShelf; ++type)
    {
        for (int frequency = 20; frequency <= 20000; frequency = frequency < 1000 ? frequency + 100 : frequency + 1000)
        {
            MultiFilter filter;
            filter.setSamplingRate(sampleRate);
            filter.setCutoffFrequency(1000.0f);
            filter.setResonance(0.707f);
            filter.setGain(6.0f);
            filter.setFilterType(type);

            double sum = 0.0;
            for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
            {
                float sample = static_cast<float>(
                    std::sin(2.0 * pi * frequency * sampleIndex / sampleRate));
                filter.processSample(sample);
                if (sampleIndex >= warmup)
                    sum += static_cast<double>(sample) * sample;
            }

            const auto magnitude = std::sqrt(sum / static_cast<double>(sampleCount - warmup))
                / std::sqrt(0.5);
            const auto magnitudeDb = 20.0 * std::log10(std::max(magnitude, 1.0e-12));
            output << type << ',' << frequency << ',' << magnitudeDb << '\n';
        }
    }

    return 0;
}
