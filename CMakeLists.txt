cmake_minimum_required(VERSION 3.22)

project(VintageDelays VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

# JUCE: a local checkout if JUCE_DIR is given, otherwise the pinned release from GitHub.
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Plugin formats for every plugin in the repo.
set(VDC_PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND VDC_PLUGIN_FORMATS AU)
endif()

enable_testing()

add_subdirectory(core)
add_subdirectory(plugins/slopebox)
add_subdirectory(plugins/drumstation)

# Further plugins go in plugins/<name> and are added here.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/plugins/dl4/CMakeLists.txt)
    add_subdirectory(plugins/dl4)
endif()
