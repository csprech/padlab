cmake_minimum_required(VERSION 3.22)
project(Padlab VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
  message(FATAL_ERROR "JUCE not found. Run scripts/bootstrap.sh first.")
endif()
add_subdirectory(external/JUCE)

# The source lists (the core, the engine and panel, the processor for hosts) live in padlab-module.cmake, so
# the Tabletop studio compiles exactly what the app does.
include(padlab-module.cmake)

set(PADLAB_JUCE_DEFS
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_USE_MP3AUDIOFORMAT=0
  JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1
)

option(PADLAB_BUILD_APP "Build the GUI app" ON)

# ---------------------------------------------------------------- app
if(PADLAB_BUILD_APP)
juce_add_gui_app(Padlab
  PRODUCT_NAME "Padlab"
  COMPANY_NAME "Padlab Research"
  BUNDLE_ID "org.padlab.app"
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Padlab needs audio input to record samples.")

target_sources(Padlab PRIVATE
  ${PADLAB_MODULE_SOURCES}
  src/Main.cpp)
padlab_add_assets(Padlab)

target_include_directories(Padlab PRIVATE src)
target_compile_definitions(Padlab PRIVATE ${PADLAB_JUCE_DEFS}
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Padlab,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Padlab,JUCE_VERSION>")

target_link_libraries(Padlab PRIVATE
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_audio_devices
  juce::juce_audio_formats
  juce::juce_dsp
  juce::juce_gui_extra
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------- tests
juce_add_console_app(PadlabTests PRODUCT_NAME "PadlabTests")
target_sources(PadlabTests PRIVATE
  ${PADLAB_CORE_SOURCES}
  tests/TestMain.cpp
  tests/TimingTests.cpp
  tests/TransportTests.cpp
  tests/SamplerTests.cpp
  tests/ChopTests.cpp
  tests/ProjectIOTests.cpp
  tests/SampleOpsTests.cpp
  tests/MidiClockTests.cpp
  tests/SeqEditTests.cpp)
target_include_directories(PadlabTests PRIVATE src)
target_compile_definitions(PadlabTests PRIVATE ${PADLAB_JUCE_DEFS}
  PADLAB_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
target_link_libraries(PadlabTests PRIVATE
  juce::juce_audio_basics
  juce::juce_audio_formats
  juce::juce_dsp
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME PadlabTests COMMAND PadlabTests)
