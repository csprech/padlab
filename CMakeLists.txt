cmake_minimum_required(VERSION 3.22)
project(Padlab VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
  message(FATAL_ERROR "JUCE not found. Run scripts/bootstrap.sh first.")
endif()
add_subdirectory(external/JUCE)

# Core engine sources: compiled into both the app and the test runner.
set(PADLAB_CORE_SOURCES
  src/core/Timing.cpp
  src/core/Transport.cpp
  src/core/Sampler.cpp
  src/core/DrumSynth.cpp
  src/core/Chop.cpp
  src/core/SampleOps.cpp
  src/core/ProjectIO.cpp
  src/core/Project.cpp
)

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
juce_add_binary_data(PadlabAssets SOURCES assets/fonts/VT323-Regular.ttf)

juce_add_gui_app(Padlab
  PRODUCT_NAME "Padlab"
  COMPANY_NAME "Padlab Research"
  BUNDLE_ID "org.padlab.app"
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Padlab needs audio input to record samples.")

target_sources(Padlab PRIVATE
  ${PADLAB_CORE_SOURCES}
  src/engine/Engine.cpp
  src/ui/Theme.cpp
  src/ui/Materials.cpp
  src/ui/DataWheel.cpp
  src/ui/PadGrid.cpp
  src/ui/Screens.cpp
  src/ui/MainScreen.cpp
  src/ui/ProgramScreen.cpp
  src/ui/SampleScreen.cpp
  src/ui/SamplerScreen.cpp
  src/ui/MixerScreen.cpp
  src/ui/MainComponent.cpp
  src/Main.cpp)

target_include_directories(Padlab PRIVATE src)
target_compile_definitions(Padlab PRIVATE ${PADLAB_JUCE_DEFS}
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Padlab,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Padlab,JUCE_VERSION>")

target_link_libraries(Padlab PRIVATE
  PadlabAssets
  juce::juce_audio_utils
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
  tests/ProjectIOTests.cpp)
target_include_directories(PadlabTests PRIVATE src)
target_compile_definitions(PadlabTests PRIVATE ${PADLAB_JUCE_DEFS})
target_link_libraries(PadlabTests PRIVATE
  juce::juce_audio_basics
  juce::juce_audio_formats
  juce::juce_dsp
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME PadlabTests COMMAND PadlabTests)
