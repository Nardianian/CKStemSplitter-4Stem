cmake_minimum_required(VERSION 4.00)
project(CKStemSplitter VERSION 0.6.1)

# Richiede CMake 3.15+ per funzionare correttamente
cmake_policy(SET CMP0091 NEW)

# Imposta MT per le build Release e MTd per le build Debug
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.1
)
FetchContent_MakeAvailable(JUCE)

juce_set_aax_sdk_path (${AAX_SDK_PATH}C:/SDKs/aax-sdk-2-8-1)
juce_set_vst2_sdk_path (${VST2_SDK_PATH}C:/SDKs/VST_SDK/vstsdk2.4)

include_directories(C:/SDKs/ASIOSDK/common)
include_directories("C:/Program Files/JACK2/include")

juce_add_plugin(CKStemSplitter
    COMPANY_NAME "Commercial Kings"
    BUNDLE_ID "com.commercialkings.ckstemsplitter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ckng
    PLUGIN_CODE Ckss
    FORMATS VST3 VST Standalone LV2 AAX
    VST2_CATEGORY kPlugCategEffect
    VST3_CATEGORIES "Tools" "Fx"
    AAX_Category AAX_EPlugInCategory_Effect
    LV2URI https://github.com/Milkstyles/CKStemSplitter
    AAX_Identifier com.Milkstyles.CKStemSplitter
    PRODUCT_NAME "CK Stem Splitter"
    PLUGIN_NAME "CK Stem Splitter"
)

juce_generate_juce_header(CKStemSplitter)

target_sources(CKStemSplitter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/StemEngine.cpp
    Source/StemEngine.h
)

target_compile_definitions(CKStemSplitter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_JACK=1
    JUCE_ASIO=1
)

target_link_libraries(CKStemSplitter
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
