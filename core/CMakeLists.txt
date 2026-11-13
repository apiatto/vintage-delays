# vintage_delay_core: shared DSP (no JUCE dependency) and shared UI look (JUCE).

add_library(vintage_delay_core INTERFACE)
target_include_directories(vintage_delay_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(vintage_delay_core INTERFACE cxx_std_17)
add_library(vdc::core ALIAS vintage_delay_core)
