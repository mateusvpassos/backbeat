cmake_minimum_required(VERSION 3.25)

project(Backbeat VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Runtime estático: o .exe roda sem instalar o VC++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---------------------------------------------------------------------------
# JUCE 8.0.15 (já traz o ASIO SDK da Steinberg, licença GPLv3).
# Usa external/JUCE se existir (clone local, mais rápido); senão baixa via FetchContent.
# ---------------------------------------------------------------------------
set(BACKBEAT_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE" CACHE PATH "Pasta com o código do JUCE")

if(EXISTS "${BACKBEAT_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${BACKBEAT_JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(BACKBEAT_COMMON_DEFS
    JUCE_ASIO=1                 # ASIO com o SDK embutido no JUCE
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_LV2=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------
juce_add_gui_app(Backbeat
    PRODUCT_NAME "Backbeat"
    COMPANY_NAME "mateusvpassos"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.svg"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.svg"
    VERSION ${PROJECT_VERSION})

# Ícone e fontes da família (Geist, OFL) embutidos no executável.
juce_add_binary_data(BackbeatAssets
    HEADER_NAME BackbeatAssets.h
    NAMESPACE BackbeatAssets
    SOURCES
        assets/icon.svg
        assets/fonts/Geist-Regular.ttf
        assets/fonts/Geist-Medium.ttf
        assets/fonts/Geist-SemiBold.ttf
        assets/fonts/GeistMono-Medium.ttf)

target_sources(Backbeat PRIVATE
    src/Main.cpp
    src/engine/AudioEngine.cpp
    src/engine/DriftResampler.cpp
    src/engine/PluginHost.cpp
    src/capture/ProcessLoopbackCapture.cpp
    src/capture/ChromeCapture.cpp
    src/engine/MidiInputRouter.cpp
    src/input/GlobalHotkeys.cpp
    src/mapping/DrumCatalog.cpp
    src/mapping/MappingProfile.cpp
    src/mapping/ProfileManager.cpp
    src/rec/DiskWriter.cpp
    src/rec/MixExporter.cpp
    src/rec/Recorder.cpp
    src/rec/TakeFiles.cpp
    src/util/Settings.cpp
    src/util/DrumNames.cpp
    src/ui/MainComponent.cpp
    src/ui/PluginWindow.cpp
    src/ui/LevelMeter.cpp
    src/ui/BackbeatLookAndFeel.cpp
    src/ui/ChannelStrip.cpp
    src/ui/AboutWindow.cpp
    src/ui/BeatIndicator.cpp
    src/ui/MappingWindow.cpp
    src/ui/PadsPage.cpp
    src/ui/KeysPage.cpp)

target_include_directories(Backbeat PRIVATE src)

# Código-fonte em UTF-8. Textos com acento vão como L"..." (o juce::String trata char* como ASCII).
target_compile_options(Backbeat PRIVATE /utf-8)

target_compile_definitions(Backbeat PRIVATE
    ${BACKBEAT_COMMON_DEFS}
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Backbeat,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Backbeat,JUCE_VERSION>")

target_link_libraries(Backbeat
    PRIVATE
        BackbeatAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_devices
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Testes (sem dispositivo real): fila lock-free, configurações, mapeamento, e a gravação
# sincronizada rodando o AudioEngine com um dispositivo e um plugin falsos.
# ---------------------------------------------------------------------------
juce_add_console_app(BackbeatTests PRODUCT_NAME "BackbeatTests")

target_sources(BackbeatTests PRIVATE
    tests/TestMain.cpp
    tests/RecordingTests.cpp
    tests/HotkeyTests.cpp
    tests/DriftTests.cpp
    tests/MetronomeTests.cpp
    src/engine/AudioEngine.cpp
    src/engine/DriftResampler.cpp
    src/input/GlobalHotkeys.cpp
    src/mapping/DrumCatalog.cpp
    src/mapping/MappingProfile.cpp
    src/mapping/ProfileManager.cpp
    src/rec/DiskWriter.cpp
    src/rec/MixExporter.cpp
    src/rec/TakeFiles.cpp
    src/util/Settings.cpp
    src/util/DrumNames.cpp)

target_include_directories(BackbeatTests PRIVATE src)
target_compile_options(BackbeatTests PRIVATE /utf-8)
target_compile_definitions(BackbeatTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
    JUCE_PLUGINHOST_VST3=0 JUCE_PLUGINHOST_AU=0 JUCE_PLUGINHOST_LADSPA=0 JUCE_PLUGINHOST_LV2=0)
target_link_libraries(BackbeatTests
    PRIVATE juce::juce_core juce::juce_audio_basics juce::juce_audio_formats
            juce::juce_audio_devices juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
