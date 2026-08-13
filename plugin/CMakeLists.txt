cmake_minimum_required(VERSION 4.2 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)



  
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")


# Print the include directory path for debugging
#message(STATUS "Include Dir: ${INCLUDE_DIR}")



# Adds a plugin target (that's basically what the Projucer does). This way we don't add_executable, juce helpers here takes care of that behind the scene.
# JUCE default is that target/library code is DDL. So any source file I add to target is DDL (Dynamic linked library). It is possible to add static libraries that this target will link to.
# But for plugins/DAW project standard is DDL for plugin to work.
juce_add_plugin(${PROJECT_NAME}
        PRODUCT_NAME
        "MultiFilter"
        VERSION
        0.0.0 # 0 forces AU hosts to always refresh.
        COMPANY_NAME
        "audio_head"
        PLUGIN_MANUFACTURER_CODE
        Auhe # good practice to use company name as source to make strong unique code, that differ from other companies.
        PLUGIN_CODE
        Mufi # # good practice to use plugin name as source to make strong unique code, that differ from other companies.
        FORMATS
        Standalone
        AU
        VST3
        IS_SYNTH
        FALSE
        NEEDS_MIDI_INPUT
        FALSE
        NEEDS_MIDI_OUTPUT
        FALSE
        VST3_AUTO_MANIFEST
        FALSE
        COPY_PLUGIN_AFTER_BUILD
        TRUE
)



juce_generate_juce_header(${PROJECT_NAME})





# ensure you employ CONFIGURE_DEPENDS so the build system picks up changes
#file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")
#target_sources(${PROJECT_NAME} PRIVATE SourceFiles )

# Appease the CMake gods and avoid globs, manually add files like so:
target_sources(${PROJECT_NAME} 
    PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ParameterLayout.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ParameterLayout.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Controller Slider.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Controller Slider.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Controller ComboBox.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Controller ComboBox.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/CustomLookAndFeel.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/CustomLookAndFeel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginEditor.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginEditor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/FilterCurveComponent.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/FilterCurveComponent.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/FilterCoefficients.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/MultiFilter.h"
)

 # Sets the include directories of the plugin project(used by preprocessor to put headers .h in source files).
target_include_directories(${PROJECT_NAME}
    PUBLIC
    "${INCLUDE_DIR}"
    "${JUCE_SOURCE_DIR}/modules"
    "${CMAKE_BINARY_DIR}/assets"
     

)


# Link against PluginAssets for binary data
target_link_libraries(${PROJECT_NAME}
        PUBLIC
        PluginAssets
)
 



# Links to all necessary dependencies. 
# Compiler use this when link in build process. 
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    juce_analytics
    juce_animation  #JUCE 8 stuff
    juce_audio_basics
    juce_audio_devices
    juce_audio_formats
    juce_audio_plugin_client
    juce_audio_processors
    juce_audio_utils
    juce_box2d
    juce_core
    juce_cryptography
    juce_data_structures
    juce_dsp
    juce_events
    juce_graphics
    juce_gui_basics
    juce_gui_extra
    juce_midi_ci
    juce_opengl
    juce_osc
    juce_product_unlocking
    juce_video
    juce_gui_extra
        


    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        )

# These definitions are recommended by JUCE.
target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        #JUCE_DISPLAY_SPLASH_SCREEN=0
)







# Enables all warnings and treats warnings as errors.
# This needs to be set up only for your projects, not 3rd party
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /Wall)
    # Silences some deprecation warnings that (hopefully) will be fixed in a future release of JUCE.
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
            _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# In Visual Studio this command provides a nice grouping of source files in "filters".
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/..)


# Set the executable for debugging. Set in ALL_BUILD properties debug target instead.      
#set_target_properties(${PROJECT_NAME} PROPERTIES
 #   VS_DEBUGGER_COMMAND "C:/ProgramData/Ableton/Live 10 Suite/Program/Ableton Live 10 Suite.exe"
    #VS_DEBUGGER_COMMAND_ARGUMENTS "\"C:/Path/To/Your/ProjectFile.als\""
  #  VS_DEBUGGER_WORKING_DIRECTORY "C:/ProgramData/Ableton/Live 10 Suite/Program"
#)