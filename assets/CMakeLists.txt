

cmake_minimum_required(VERSION 4.2 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)




# Glob all asset files (PNG, TTF, OTF, JPG, SVG) in the current directory
file(GLOB_RECURSE ASSETS_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.otf"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.jpg"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.svg"
)

# Print the list of files to verify paths (optional, for debugging)
message(STATUS "Assets files: ${ASSETS_FILES}")

# Check if any asset files were found, and then create library, I can link to in plugin/CmakeList
if(ASSETS_FILES)
    juce_add_binary_data(
            PluginAssets
            HEADER_NAME "PluginAssets.h"
            SOURCES ${ASSETS_FILES}
    )

else()
    message(FATAL_ERROR "No asset files found in directory: ${CMAKE_CURRENT_SOURCE_DIR}")
endif()