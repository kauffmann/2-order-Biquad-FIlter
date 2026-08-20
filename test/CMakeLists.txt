cmake_minimum_required(VERSION 4.2 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(dsp_tests
    MultiFilterTests.cpp
    PluginProcessorTests.cpp
)

add_dependencies(dsp_tests MultiFilter)

target_link_libraries(dsp_tests
    PRIVATE
        MultiFilter
        GTest::gtest_main
)

target_include_directories(dsp_tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/plugin/Source"
        "${CMAKE_BINARY_DIR}/plugin/MultiFilter_artefacts/JuceLibraryCode"
        "${JUCE_SOURCE_DIR}/modules"
)

target_compile_definitions(dsp_tests
    PRIVATE
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_SHARED_CODE=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JucePlugin_IsMidiEffect=0
        JucePlugin_IsSynth=0
        JucePlugin_WantsMidiInput=0
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_Enable_ARA=0
        JucePlugin_Name="MultiFilter"
)

target_compile_options(dsp_tests PRIVATE -Wall -Wextra -Wpedantic)

include(GoogleTest)
gtest_discover_tests(dsp_tests)

add_executable(dsp_response_export
    response_export.cpp
)

add_dependencies(dsp_response_export MultiFilter)

target_link_libraries(dsp_response_export
    PRIVATE
        MultiFilter
)

target_include_directories(dsp_response_export
    PRIVATE
        "${CMAKE_SOURCE_DIR}/plugin/Source"
        "${CMAKE_BINARY_DIR}/plugin/MultiFilter_artefacts/JuceLibraryCode"
        "${JUCE_SOURCE_DIR}/modules"
)

target_compile_definitions(dsp_response_export
    PRIVATE
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_SHARED_CODE=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JucePlugin_IsMidiEffect=0
        JucePlugin_IsSynth=0
        JucePlugin_WantsMidiInput=0
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_Enable_ARA=0
        JucePlugin_Name="MultiFilter"
)

target_compile_options(dsp_response_export PRIVATE -Wall -Wextra -Wpedantic)

set(DSP_ANALYSIS_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/python/plot_response.py")
add_custom_target(dsp_plot
    COMMAND dsp_response_export "${CMAKE_CURRENT_BINARY_DIR}/filter_response.csv"
    COMMAND "${Python3_EXECUTABLE}" "${DSP_ANALYSIS_SCRIPT}"
            "${CMAKE_CURRENT_BINARY_DIR}/filter_response.csv"
    DEPENDS dsp_response_export "${DSP_ANALYSIS_SCRIPT}"
    USES_TERMINAL
)
