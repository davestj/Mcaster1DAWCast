# Mcaster1Tuner — VST3 Chromatic Tuner Plugin
# Copyright (c) 2026 David St. John <davestj@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#
# Install (macOS):
#   cp -r build/VST3/Release/Mcaster1Tuner.vst3 ~/Library/Audio/Plug-Ins/VST3/

cmake_minimum_required(VERSION 3.20)
project(Mcaster1Tuner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")

# ── Fetch VST3 SDK ──────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        v3.7.14_build_55
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

# Enable VSTGUI for custom editor, disable SDK examples/tests
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_ADD_VSTGUI ON CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_PLUGINS_SAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_HOSTING_SAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)

message(STATUS "Fetching VST3 SDK v3.7.14...")
FetchContent_MakeAvailable(vst3sdk)

# VSTGUI is enabled via SMTG_ENABLE_VSTGUI_SUPPORT above; the SDK's own
# CMakeLists.txt calls smtg_enable_vstgui_support() and creates the
# vstgui_support target for us.

# Suppress C++17 deprecation warnings in VSTGUI (wstring_convert, codecvt)
# that trigger -Werror on newer Xcode SDKs.
if(TARGET vstgui)
    target_compile_options(vstgui PRIVATE -Wno-deprecated-declarations)
endif()
if(TARGET vstgui_uidescription)
    target_compile_options(vstgui_uidescription PRIVATE -Wno-deprecated-declarations)
endif()
if(TARGET vstgui_standalone)
    target_compile_options(vstgui_standalone PRIVATE -Wno-deprecated-declarations)
endif()

# ── Plugin target ───────────────────────────────────────────────────────────
smtg_add_vst3plugin(Mcaster1Tuner
    src/Mcaster1Tuner.cpp
    src/Mcaster1TunerController.cpp
    src/Mcaster1TunerEditor.cpp
    src/Mcaster1TunerEntry.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/macmain.cpp
)

target_include_directories(Mcaster1Tuner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${vst3sdk_SOURCE_DIR}/vstgui4
)

target_link_libraries(Mcaster1Tuner PRIVATE
    sdk
    vstgui_support
)

# macOS bundle info
if(APPLE)
    set_target_properties(Mcaster1Tuner PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "vst3"
        MACOSX_BUNDLE_BUNDLE_NAME "Mcaster1Tuner"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.mcaster1.tuner"
    )
endif()

# Install target
install(TARGETS Mcaster1Tuner
    LIBRARY DESTINATION "~/Library/Audio/Plug-Ins/VST3"
    BUNDLE  DESTINATION "~/Library/Audio/Plug-Ins/VST3"
)
