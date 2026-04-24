# Mcaster1DAWCast — Windows CMake build
#
# Companion build for the macOS/Linux Autotools build (which stays authoritative
# on those platforms). This file drives:
#
#   - Visual Studio 2022 solution generation (one .vcxproj per module)
#   - Ninja builds for CI
#   - vcpkg manifest-mode dependency resolution (see vcpkg.json)
#
# Cross-platform source is consumed read-only from ../DAWCast/.
# Windows-specific code (platform backends, side apps, plugins, installers)
# lives entirely under src/windows/.

cmake_minimum_required(VERSION 3.24)

project(Mcaster1DAWCast
    VERSION     1.0.0
    DESCRIPTION "Multi-Channel DAW for Broadcasting, Webcasting, Podcasting & Video Editing"
    HOMEPAGE_URL "https://mcaster1.com/dawcast"
    LANGUAGES   CXX C
)

if(NOT WIN32)
    message(FATAL_ERROR
        "src/windows/CMakeLists.txt targets Windows only. "
        "On macOS or Linux, use the top-level autotools build (./autogen.sh && ./configure && make).")
endif()

# ── Global C++ settings ──────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    add_compile_options(/W3 /permissive- /Zc:__cplusplus /MP /utf-8 /bigobj)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
        _USE_MATH_DEFINES       # M_PI / M_E / M_SQRT2 etc. in <cmath> — DSP code relies on these
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE _UNICODE
    )
endif()

# ── Qt: AUTOMOC / AUTORCC / AUTOUIC ──────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Paths ────────────────────────────────────────────────────────────────────
set(DAWCAST_WINDOWS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(DAWCAST_CORE_ROOT    ${CMAKE_CURRENT_SOURCE_DIR}/../DAWCast)
set(DAWCAST_REPO_ROOT    ${CMAKE_CURRENT_SOURCE_DIR}/../..)

get_filename_component(DAWCAST_CORE_ROOT "${DAWCAST_CORE_ROOT}" ABSOLUTE)
get_filename_component(DAWCAST_REPO_ROOT "${DAWCAST_REPO_ROOT}" ABSOLUTE)

if(NOT EXISTS "${DAWCAST_CORE_ROOT}/main.cpp")
    message(FATAL_ERROR "Cross-platform source not found at ${DAWCAST_CORE_ROOT}")
endif()

message(STATUS "DAWCast core source: ${DAWCAST_CORE_ROOT}")
message(STATUS "Repo root:           ${DAWCAST_REPO_ROOT}")

# ── Dependencies (vcpkg manifest mode) ───────────────────────────────────────
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Widgets Network Concurrent Svg Sql
)
# QtMultimedia / QtCharts are intentionally NOT dependencies — DAWCast routes
# all audio/video through PortAudio + FFmpeg directly, and the spectrum
# analyzer paints through QPainter. Pulling them in would drag qtdeclarative
# into the build, which overflows Windows MAX_PATH from deep repo paths.

qt_standard_project_setup()

find_package(portaudio          CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(TagLib             CONFIG REQUIRED)

# FFmpeg: vcpkg's port ships a cmake wrapper that exports FFMPEG_INCLUDE_DIRS
# and FFMPEG_LIBRARIES directly; it does not use find_package COMPONENTS.
find_package(FFMPEG REQUIRED)

# libyaml via pkg-config fallback — vcpkg builds it but doesn't ship a CMake config.
find_path(LIBYAML_INCLUDE_DIR NAMES yaml.h REQUIRED)
find_library(LIBYAML_LIBRARY  NAMES yaml   REQUIRED)

# Optional native codec libs — defined as INTERFACE targets when found so the
# module libraries can link them without repeating find_package chains.
find_package(SndFile  CONFIG QUIET)
find_package(soxr     CONFIG QUIET)
find_package(FLAC     CONFIG QUIET)
find_package(Ogg      CONFIG QUIET)
find_package(Vorbis   CONFIG QUIET)
find_package(Opus     CONFIG QUIET)
find_package(OpusEnc       QUIET)        # libopusenc — separate from libopus
find_package(yaml-cpp CONFIG QUIET)      # used by dsp/mc1/patchbay preset_manager
find_path(OPUSENC_INCLUDE_DIR NAMES opusenc.h PATH_SUFFIXES opus)
find_library(OPUSENC_LIBRARY  NAMES opusenc)
find_package(mpg123   CONFIG QUIET)
find_package(WebP     CONFIG QUIET)
find_package(OpenSSL        QUIET)
find_package(ZLIB           QUIET)

# ── HAVE_* defines (mirror configure.ac feature flags) ───────────────────────
add_library(dawcast_compile_config INTERFACE)
target_compile_definitions(dawcast_compile_config INTERFACE
    HAVE_PORTAUDIO
    HAVE_TAGLIB
    HAVE_SQLITE3
    HAVE_AVFORMAT
    HAVE_LIBYAML
    DAWCAST_WINDOWS=1
    DAWCAST_MACOS=0
    DAWCAST_LINUX=0
)
if(SndFile_FOUND)
    target_compile_definitions(dawcast_compile_config INTERFACE HAVE_SNDFILE)
endif()
if(soxr_FOUND)
    target_compile_definitions(dawcast_compile_config INTERFACE HAVE_SOXR)
endif()
if(FLAC_FOUND)
    target_compile_definitions(dawcast_compile_config INTERFACE HAVE_FLAC)
endif()
if(Vorbis_FOUND)
    target_compile_definitions(dawcast_compile_config INTERFACE HAVE_VORBIS HAVE_VORBISENC)
endif()
if(Opus_FOUND)
    target_compile_definitions(dawcast_compile_config INTERFACE HAVE_OPUS)
endif()
if(OPUSENC_LIBRARY)
    target_compile_definitions(dawcast_compile_config INTERFACE HAVE_OPUSENC)
    message(STATUS "libopusenc found at ${OPUSENC_LIBRARY}")
endif()
if(mpg123_FOUND)
    target_compile_definitions(dawcast_compile_config INTERFACE HAVE_MPG123)
endif()
if(WebP_FOUND)
    target_compile_definitions(dawcast_compile_config INTERFACE HAVE_WEBP)
endif()

# ── Per-module shared include paths ──────────────────────────────────────────
add_library(dawcast_includes INTERFACE)
target_include_directories(dawcast_includes INTERFACE
    ${DAWCAST_CORE_ROOT}
    ${DAWCAST_CORE_ROOT}/core
    ${DAWCAST_CORE_ROOT}/audio_engine
    ${DAWCAST_CORE_ROOT}/video_engine
    ${DAWCAST_CORE_ROOT}/timeline
    ${DAWCAST_CORE_ROOT}/dsp
    ${DAWCAST_CORE_ROOT}/dsp/mc1
    ${DAWCAST_CORE_ROOT}/vfx
    ${DAWCAST_CORE_ROOT}/podcast
    ${DAWCAST_CORE_ROOT}/broadcast
    ${DAWCAST_CORE_ROOT}/codec
    ${DAWCAST_CORE_ROOT}/graphics
    ${DAWCAST_CORE_ROOT}/widgets
    ${DAWCAST_CORE_ROOT}/platform
    ${DAWCAST_CORE_ROOT}/plugins
    ${DAWCAST_CORE_ROOT}/config
    ${DAWCAST_CORE_ROOT}/ai
    ${DAWCAST_WINDOWS_ROOT}/platform
    ${FFMPEG_INCLUDE_DIRS}
    ${LIBYAML_INCLUDE_DIR}
)

# OpusCodec.cpp includes <opusenc.h>, but vcpkg installs it under
# include/opus/opusenc.h. Brew on macOS exposes /usr/local/include/opus via
# pkg-config; replicate that here.
if(OPUSENC_INCLUDE_DIR)
    target_include_directories(dawcast_includes INTERFACE ${OPUSENC_INCLUDE_DIR})
endif()

# ── Helper: glob sources from a cross-platform module directory ──────────────
# Recursive so nested trees (e.g. dsp/mc1/fx_ui/) get picked up. Headers must
# be in the source list for AUTOMOC to scan them — Q_OBJECT classes that live
# only in headers (~50 dialog widgets under dsp/mc1/fx_ui/) silently produce
# no MOC output otherwise, leading to LNK2001 on metaObject/qt_metacast.
# Excludes .mm files (Objective-C++, macOS-only).
function(dawcast_module_sources out_var dir)
    file(GLOB_RECURSE _cpp CONFIGURE_DEPENDS "${DAWCAST_CORE_ROOT}/${dir}/*.cpp")
    file(GLOB_RECURSE _hdr CONFIGURE_DEPENDS "${DAWCAST_CORE_ROOT}/${dir}/*.h")
    set(${out_var} ${_cpp} ${_hdr} PARENT_SCOPE)
endfunction()

# ── Module static libraries (one .vcxproj per target in the generated .sln) ──
function(dawcast_add_module name subdir)
    dawcast_module_sources(_srcs ${subdir})
    add_library(${name} STATIC ${_srcs})
    target_link_libraries(${name}
        PUBLIC
            dawcast_includes
            dawcast_compile_config
            # Link the full Qt6 module set every module is allowed to use.
            # Static-lib targets carry zero runtime cost for unused modules
            # — only the exe link decides what actually ships.
            Qt6::Core Qt6::Gui Qt6::Widgets
            Qt6::Network Qt6::Sql Qt6::Concurrent Qt6::Svg
    )
    set_target_properties(${name} PROPERTIES FOLDER "DAWCast/Modules")
endfunction()

dawcast_add_module(dawcast_core         core)
dawcast_add_module(dawcast_audio_engine audio_engine)
dawcast_add_module(dawcast_video_engine video_engine)
dawcast_add_module(dawcast_timeline     timeline)
dawcast_add_module(dawcast_dsp          dsp)
dawcast_add_module(dawcast_vfx          vfx)
dawcast_add_module(dawcast_podcast      podcast)
dawcast_add_module(dawcast_broadcast    broadcast)
dawcast_add_module(dawcast_codec        codec)
dawcast_add_module(dawcast_graphics     graphics)
dawcast_add_module(dawcast_widgets      widgets)
dawcast_add_module(dawcast_config       config)
dawcast_add_module(dawcast_ai           ai)

# plugins/ — exclude PluginScannerAU.mm (macOS AudioUnit scanner)
file(GLOB _plugin_srcs CONFIGURE_DEPENDS "${DAWCAST_CORE_ROOT}/plugins/*.cpp" "${DAWCAST_CORE_ROOT}/plugins/*.h")
add_library(dawcast_plugins STATIC ${_plugin_srcs})
target_link_libraries(dawcast_plugins
    PUBLIC dawcast_includes dawcast_compile_config Qt6::Core Qt6::Gui Qt6::Widgets)
set_target_properties(dawcast_plugins PROPERTIES FOLDER "DAWCast/Modules")

# platform_common — worker_pool.cpp only (the .mm files are macOS-only)
add_library(dawcast_platform_common STATIC
    ${DAWCAST_CORE_ROOT}/platform/worker_pool.cpp
    ${DAWCAST_CORE_ROOT}/platform/worker_pool.h
    ${DAWCAST_CORE_ROOT}/platform/compat.h
)
target_link_libraries(dawcast_platform_common
    PUBLIC dawcast_includes dawcast_compile_config Qt6::Core)
set_target_properties(dawcast_platform_common PROPERTIES FOLDER "DAWCast/Modules")

# ── Windows-specific platform backend (subdir adds dawcast_platform_windows) ──
add_subdirectory(platform)

# ── Main executable ──────────────────────────────────────────────────────────
qt_add_executable(mcaster1dawcast WIN32
    ${DAWCAST_CORE_ROOT}/main.cpp
    ${DAWCAST_CORE_ROOT}/app.cpp
    ${DAWCAST_CORE_ROOT}/app.h
    ${DAWCAST_CORE_ROOT}/resources/mcaster1dawcast.qrc
    resources/version.rc
    resources/app.manifest
)

target_link_libraries(mcaster1dawcast PRIVATE
    dawcast_core
    dawcast_audio_engine
    dawcast_video_engine
    dawcast_timeline
    dawcast_dsp
    dawcast_vfx
    dawcast_podcast
    dawcast_broadcast
    dawcast_codec
    dawcast_graphics
    dawcast_widgets
    dawcast_config
    dawcast_ai
    dawcast_plugins
    dawcast_platform_common
    dawcast_platform_windows

    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
    Qt6::Concurrent Qt6::Svg Qt6::Sql

    portaudio
    unofficial::sqlite3::sqlite3
    TagLib::tag
    ${FFMPEG_LIBRARIES}
    ${LIBYAML_LIBRARY}
)

if(OPUSENC_LIBRARY)
    target_link_libraries(mcaster1dawcast PRIVATE ${OPUSENC_LIBRARY})
endif()
if(yaml-cpp_FOUND)
    target_link_libraries(mcaster1dawcast PRIVATE yaml-cpp::yaml-cpp)
endif()

# Optional codec libs — when found, linked directly because the corresponding
# HAVE_* defines compile-in calls into them (Mp3Codec, FlacCodec, etc.).
if(mpg123_FOUND)
    target_link_libraries(mcaster1dawcast PRIVATE MPG123::libmpg123)
endif()
if(FLAC_FOUND)
    target_link_libraries(mcaster1dawcast PRIVATE FLAC::FLAC)
endif()
if(Vorbis_FOUND)
    target_link_libraries(mcaster1dawcast PRIVATE Vorbis::vorbis Vorbis::vorbisenc Vorbis::vorbisfile)
endif()
if(Opus_FOUND)
    target_link_libraries(mcaster1dawcast PRIVATE Opus::opus)
endif()
if(SndFile_FOUND)
    target_link_libraries(mcaster1dawcast PRIVATE SndFile::sndfile)
endif()
if(soxr_FOUND)
    target_link_libraries(mcaster1dawcast PRIVATE soxr)
endif()

set_target_properties(mcaster1dawcast PROPERTIES
    OUTPUT_NAME          "Mcaster1DAWCast"
    WIN32_EXECUTABLE     TRUE
    VS_DPI_AWARE         "PerMonitor"
    FOLDER               "DAWCast"
)

# ── Deploy Qt + vcpkg DLLs next to the exe (for F5 in VS) ────────────────────
# windeployqt handles Qt; vcpkg's applocal.ps1 handles the rest.
find_program(WINDEPLOYQT_EXE windeployqt HINTS ${Qt6_DIR}/../../../tools/Qt6/bin)
if(WINDEPLOYQT_EXE)
    add_custom_command(TARGET mcaster1dawcast POST_BUILD
        COMMAND ${WINDEPLOYQT_EXE}
            --no-translations --no-system-d3d-compiler --no-opengl-sw
            --verbose 0
            $<TARGET_FILE:mcaster1dawcast>
        COMMENT "Deploying Qt runtime (windeployqt)")
endif()

# ── Side apps and installer subdirectories (optional, off by default) ────────
option(DAWCAST_BUILD_SIDE_APPS "Build Windows side apps (BatchCLI, DeviceProbe, StreamTest)" OFF)
if(DAWCAST_BUILD_SIDE_APPS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps/CMakeLists.txt)
    add_subdirectory(apps)
endif()

option(DAWCAST_BUILD_PLUGINS "Build VST3/ASIO plugin hosts (requires Steinberg SDKs)" OFF)
if(DAWCAST_BUILD_PLUGINS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/plugins/CMakeLists.txt)
    add_subdirectory(plugins)
endif()

# ── VS IDE niceties ──────────────────────────────────────────────────────────
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT mcaster1dawcast)

message(STATUS "")
message(STATUS "Mcaster1DAWCast — Windows build configured")
message(STATUS "  Qt6: ${Qt6_VERSION} at ${Qt6_DIR}")
message(STATUS "  FFmpeg include: ${FFMPEG_INCLUDE_DIRS}")
message(STATUS "  Triplet:        ${VCPKG_TARGET_TRIPLET}")
message(STATUS "  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "")
