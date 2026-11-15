cmake_minimum_required(VERSION 3.22)

# "Cloudveil" is the working name of the project: the repository, the build targets and the
# classes carry it. What a user sees is the product name below.
project(Cloudveil VERSION 1.2.0)

set(DALUNE_PRODUCT_NAME "D.A LUNE")
set(DALUNE_COMPANY_NAME "D.A Production")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")
endif()

# Windows: link the C++ runtime into the plug-in, so that it also loads on machines that do not
# have the Visual C++ redistributable installed. (Has no effect with other compilers.)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(CLOUDVEIL_FORMATS AU VST3)
else()
    set(CLOUDVEIL_FORMATS VST3)
endif()

juce_add_plugin(Cloudveil
    VERSION                     ${PROJECT_VERSION}
    COMPANY_NAME                "${DALUNE_COMPANY_NAME}"
    COMPANY_WEBSITE             "https://github.com"
    BUNDLE_ID                   "com.daproduction.dalune"
    PLUGIN_MANUFACTURER_CODE    Dapr
    PLUGIN_CODE                 Lune
    FORMATS                     ${CLOUDVEIL_FORMATS}
    PRODUCT_NAME                "${DALUNE_PRODUCT_NAME}"
    DESCRIPTION                 "Dreamy Shimmer & Modulated Cloud Reverb"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Reverb
    AU_MAIN_TYPE                kAudioUnitType_Effect
)

target_sources(Cloudveil
    PRIVATE
        Source/DreamyReverbDSP.h
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
)

target_compile_definitions(Cloudveil
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Cloudveil
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# The reverb engine on its own, without JUCE: a small program that checks what the engine does
# (see Tests/EngineChecks.cpp). The CI runs it on every platform it builds for.
add_executable(CloudveilEngineChecks Tests/EngineChecks.cpp)
set_target_properties(CloudveilEngineChecks PROPERTIES
    # (the generator expression keeps Visual Studio and Xcode from adding a Release/ folder)
    RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/checks>"
)

if(APPLE)
    # Re-apply ad-hoc signature after JUCE's VST3 manifest tool modifies moduleinfo.json
    add_custom_command(TARGET Cloudveil_VST3 POST_BUILD
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:Cloudveil_VST3>"
        COMMENT "Ad-hoc signing Cloudveil VST3 bundle"
        VERBATIM
    )
    add_custom_command(TARGET Cloudveil_AU POST_BUILD
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:Cloudveil_AU>"
        COMMENT "Ad-hoc signing Cloudveil AU bundle"
        VERBATIM
    )
endif()
