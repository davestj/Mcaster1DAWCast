# Mcaster1DAWCast — Windows platform backend
# Mirrors src/DAWCast/platform/*.mm with MSVC-friendly C++ equivalents.

add_library(dawcast_platform_windows STATIC
    compat_windows.h
    windows_audio.h     windows_audio.cpp
    windows_video.h     windows_video.cpp
    windows_paths.h     windows_paths.cpp
    windows_process.h   windows_process.cpp
)

target_link_libraries(dawcast_platform_windows
    PUBLIC
        dawcast_includes
        dawcast_compile_config
        Qt6::Core Qt6::Gui
    PRIVATE
        # Windows SDK libs
        mmdevapi        # WASAPI device enumeration
        ole32           # COM
        oleaut32        # BSTR / VARIANT
        propsys         # IPropertyStore
        avrt            # AvSetMmThreadCharacteristics (pro-audio thread priority)
        uuid            # IID_* symbols
        shell32         # SHGetKnownFolderPath
        user32
        winmm
)

set_target_properties(dawcast_platform_windows PROPERTIES
    FOLDER "DAWCast/Modules")
