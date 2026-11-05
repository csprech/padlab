# Padlab as a module for hosts (the Tabletop studio): the sources to compile into the host's own target, which
# brings JUCE (juce_audio_utils, juce_audio_processors, juce_audio_formats, juce_dsp, juce_gui_extra). The app
# and the tests use the same lists. padlab_add_assets(<target>) adds the panel's font.
set(PADLAB_DIR ${CMAKE_CURRENT_LIST_DIR})
set(PADLAB_CORE_SOURCES
  ${PADLAB_DIR}/src/core/Timing.cpp
  ${PADLAB_DIR}/src/core/Transport.cpp
  ${PADLAB_DIR}/src/core/Sampler.cpp
  ${PADLAB_DIR}/src/core/DrumSynth.cpp
  ${PADLAB_DIR}/src/core/Chop.cpp
  ${PADLAB_DIR}/src/core/SampleOps.cpp
  ${PADLAB_DIR}/src/core/ProjectIO.cpp
  ${PADLAB_DIR}/src/core/Project.cpp
  ${PADLAB_DIR}/src/core/SeqEdit.cpp
  ${PADLAB_DIR}/src/core/Effects.cpp)
set(PADLAB_UI_SOURCES
  ${PADLAB_DIR}/src/engine/Engine.cpp
  ${PADLAB_DIR}/src/ui/Theme.cpp
  ${PADLAB_DIR}/src/ui/Materials.cpp
  ${PADLAB_DIR}/src/ui/DataWheel.cpp
  ${PADLAB_DIR}/src/ui/PanelParts.cpp
  ${PADLAB_DIR}/src/ui/PadGrid.cpp
  ${PADLAB_DIR}/src/ui/Screens.cpp
  ${PADLAB_DIR}/src/ui/MainScreen.cpp
  ${PADLAB_DIR}/src/ui/ProgramScreen.cpp
  ${PADLAB_DIR}/src/ui/SampleScreen.cpp
  ${PADLAB_DIR}/src/ui/SamplerScreen.cpp
  ${PADLAB_DIR}/src/ui/MixerScreen.cpp
  ${PADLAB_DIR}/src/ui/ExtraScreens.cpp
  ${PADLAB_DIR}/src/ui/HelpScreen.cpp
  ${PADLAB_DIR}/src/ui/EditScreen.cpp
  ${PADLAB_DIR}/src/ui/MainComponent.cpp)
set(PADLAB_MODULE_SOURCES ${PADLAB_CORE_SOURCES} ${PADLAB_UI_SOURCES} ${PADLAB_DIR}/src/plugin/PadlabProcessor.cpp)

function(padlab_add_assets target)
  if(NOT TARGET PadlabAssets)
    # (its own namespace and header, so it can't collide with a host's BinaryData)
    juce_add_binary_data(PadlabAssets NAMESPACE PadlabBinaryData HEADER_NAME PadlabBinaryData.h
      SOURCES ${PADLAB_DIR}/assets/fonts/VT323-Regular.ttf)
  endif()
  target_link_libraries(${target} PRIVATE PadlabAssets)
endfunction()
