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

# ==================================================================================================
# The installer (Installer/): a program with the plug-in inside it. It shows a welcome page and
# the install location, copies the plug-in there, and says when it is done.
#
#   Windows   "D.A LUNE Installer.exe"   carries the VST3. It asks for administrator rights when
#                                        started, because C:\Program Files\Common Files\VST3
#                                        cannot be written to without them.
#   macOS     "D.A LUNE Installer.app"   carries the Audio Unit and the VST3.
# ==================================================================================================

juce_add_gui_app(CloudveilInstaller
    PRODUCT_NAME    "${DALUNE_PRODUCT_NAME} Installer"
    COMPANY_NAME    "${DALUNE_COMPANY_NAME}"
    BUNDLE_ID       "com.daproduction.dalune.installer"
    VERSION         ${PROJECT_VERSION}
    ICON_BIG        "${CMAKE_CURRENT_SOURCE_DIR}/Installer/Assets/icon.png"
)

# The pictures and the typefaces, as data in the program. The pictures are made from the design,
# Installer/Design/Installer.png, by Installer/Design/make-assets.py.
juce_add_binary_data(CloudveilInstallerData
    HEADER_NAME InstallerData.h
    NAMESPACE   InstallerData
    SOURCES
        Installer/Assets/picture.png
        Installer/Assets/patchText.png
        Installer/Assets/patchNoButtons.png
        Installer/Assets/patchFinish.png
        Installer/Assets/RedHatDisplayRegular.ttf
        Installer/Assets/RedHatDisplayMedium.ttf
        Installer/Assets/RedHatDisplaySemiBold.ttf
        Installer/Assets/RedHatDisplayBold.ttf
        Installer/Assets/RedHatDisplayExtraBold.ttf
)

# The names and the version, for the installer's texts
set(DALUNE_INSTALLER_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/installer")
configure_file(Installer/InstallerBuildConfig.h.in "${DALUNE_INSTALLER_GENERATED}/InstallerBuildConfig.h" @ONLY)

target_sources(CloudveilInstaller
    PRIVATE
        Installer/Source/InstallerAssets.h
        Installer/Source/InstallerAssets.cpp
        Installer/Source/InstallerConfig.h
        Installer/Source/InstallerEngine.h
        Installer/Source/InstallerEngine.cpp
        Installer/Source/InstallerMain.cpp
        Installer/Source/InstallerPicture.h
        Installer/Source/InstallerPicture.cpp
        Installer/Source/InstallerShell.h
        Installer/Source/InstallerView.h
        Installer/Source/InstallerView.cpp
        Installer/Source/InstallerWindow.h
        Installer/Source/PayloadWindows.cpp
        Installer/Source/SystemMouse.cpp
        Installer/Source/SystemWindow.cpp
)

target_include_directories(CloudveilInstaller PRIVATE "${DALUNE_INSTALLER_GENERATED}")

target_compile_definitions(CloudveilInstaller
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(CloudveilInstaller
    PRIVATE
        CloudveilInstallerData
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(APPLE)
    # (for Installer/Source/SystemMouse.cpp, which the installer's self-test uses)
    find_library(DALUNE_APPLICATION_SERVICES ApplicationServices REQUIRED)
    target_link_libraries(CloudveilInstaller PRIVATE "${DALUNE_APPLICATION_SERVICES}")
endif()

# The plug-in as the installer carries it: a zip with one folder per format, and in each the
# plug-in as it is to arrive in the target folder. It is packed anew whenever the installer is
# built, after the plug-in itself (its signature and its VST3 module description included).
set(DALUNE_PAYLOAD_ZIP "${DALUNE_INSTALLER_GENERATED}/payload.zip")
set(DALUNE_PAYLOAD_FROM "$<GENEX_EVAL:$<TARGET_PROPERTY:Cloudveil,LIBRARY_OUTPUT_DIRECTORY>>")

set(DALUNE_PAYLOAD_CONTENTS "VST3/${DALUNE_PRODUCT_NAME}.vst3")
set(DALUNE_PAYLOAD_TARGETS Cloudveil_VST3)

if(APPLE)
    list(APPEND DALUNE_PAYLOAD_CONTENTS "AU/${DALUNE_PRODUCT_NAME}.component")
    list(APPEND DALUNE_PAYLOAD_TARGETS Cloudveil_AU)
endif()

add_custom_target(CloudveilInstallerPayload
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DALUNE_INSTALLER_GENERATED}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${DALUNE_PAYLOAD_ZIP}"
    COMMAND "${CMAKE_COMMAND}" -E chdir "${DALUNE_PAYLOAD_FROM}"
            "${CMAKE_COMMAND}" -E tar cf "${DALUNE_PAYLOAD_ZIP}" --format=zip -- ${DALUNE_PAYLOAD_CONTENTS}
    BYPRODUCTS "${DALUNE_PAYLOAD_ZIP}"
    COMMENT "Packing the plug-in for the installer"
    VERBATIM
)
add_dependencies(CloudveilInstallerPayload ${DALUNE_PAYLOAD_TARGETS})
add_dependencies(CloudveilInstaller CloudveilInstallerPayload)

if(WIN32)
    # The zip becomes a resource of the .exe
    configure_file(Installer/payload.rc.in "${DALUNE_INSTALLER_GENERATED}/payload.rc" @ONLY)
    target_sources(CloudveilInstaller PRIVATE "${DALUNE_INSTALLER_GENERATED}/payload.rc")
    set_source_files_properties("${DALUNE_INSTALLER_GENERATED}/payload.rc" PROPERTIES OBJECT_DEPENDS "${DALUNE_PAYLOAD_ZIP}")

    # Windows asks for administrator rights before the installer starts
    if(MSVC)
        set_property(TARGET CloudveilInstaller APPEND_STRING PROPERTY
            LINK_FLAGS " /MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
    endif()
elseif(APPLE)
    # The zip goes into the .app, which is then signed (ad hoc) as a whole
    add_custom_command(TARGET CloudveilInstaller POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:CloudveilInstaller>/Resources"
        COMMAND "${CMAKE_COMMAND}" -E copy "${DALUNE_PAYLOAD_ZIP}" "$<TARGET_BUNDLE_CONTENT_DIR:CloudveilInstaller>/Resources/payload.zip"
        COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:CloudveilInstaller>"
        COMMENT "Putting the plug-in into the installer and signing it"
        VERBATIM
    )
endif()
