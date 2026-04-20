# Mcaster1DAWCast — Windows Port

Windows-specific build, platform backends, side apps, plugins, and installer for Mcaster1DAWCast.
The macOS and Linux builds continue to use the top-level Autotools build (`./autogen.sh && ./configure && make`); this tree is **Windows-only**.

## Layout

```
src/windows/
├── vcpkg.json                  manifest — all deps, pinned baseline
├── vcpkg-configuration.json    vcpkg registry pin for reproducible builds
├── CMakeLists.txt              top-level build (generates per-module .vcxproj)
├── CMakePresets.json           vs2022-x64-{debug,release}, ninja-x64-{debug,release}
├── platform/                   Windows backends (WASAPI, MF, D3D11VA, known folders, subprocess)
├── apps/                       side apps (headless batch encoder, device probe, stream test)
├── plugins/                    VST3 / ASIO hosts (license-gated via CMake options)
├── installer/                  NSIS + WiX installer staging
├── resources/                  app.manifest (DPI, UTF-8), version.rc (VS_VERSION_INFO)
├── scripts/                    bootstrap-vcpkg.ps1, build-release.ps1
└── vcpkg/                      (gitignored) bootstrapped vcpkg checkout
```

The cross-platform core under `src/DAWCast/` is consumed read-only by this tree. Every module there becomes a static-library target here, and the generated `Mcaster1DAWCast.sln` gives you one `.vcxproj` per module plus the final executable.

## First build

```powershell
# From src/windows/
./scripts/bootstrap-vcpkg.ps1             # one-time, ~20 min for full dep tree
cmake --preset vs2022-x64-debug           # generates Mcaster1DAWCast.sln
cmake --build --preset vs2022-x64-debug
```

Or open `src/windows/` in Visual Studio 2022 via **File > Open > Folder** — VS reads `CMakePresets.json` automatically and surfaces the presets in the configuration dropdown.

## Link model

`x64-windows` (dynamic). Qt, FFmpeg, PortAudio, TagLib, and the codec libraries ship as DLLs next to `Mcaster1DAWCast.exe`. `windeployqt` runs automatically at build time; the installer under `src/windows/installer/` packages everything.

## Adding a source file

1. Drop it into the appropriate `src/DAWCast/<module>/` directory.
2. Re-run CMake configure — `file(GLOB CONFIGURE_DEPENDS)` picks it up.
3. Also add it to `src/DAWCast/Makefile.am` so the macOS/Linux build stays in sync.

## Platform-specific files

- `.mm` files (Objective-C++) are ignored by this tree — they're macOS-only.
- Windows-specific `.cpp` goes under `src/windows/platform/`, `src/windows/apps/`, or `src/windows/plugins/`.
