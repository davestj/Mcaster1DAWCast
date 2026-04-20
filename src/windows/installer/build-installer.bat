@echo off
REM Mcaster1DAWCast — Windows Installer Build Script
REM Stages Mcaster1DAWCast.exe + Qt DLLs + vcpkg DLLs + app resources,
REM then runs NSIS to produce Mcaster1DAWCast-Setup.exe.
REM
REM Expects a prior successful build via:
REM   cmake --preset vs2022-x64-release
REM   cmake --build --preset vs2022-x64-release
REM
REM Requires on PATH: windeployqt (from vcpkg Qt tools), makensis

setlocal EnableDelayedExpansion

set APP_NAME=Mcaster1DAWCast
set WINDOWS_ROOT=%~dp0..
set BUILD_DIR=%WINDOWS_ROOT%\build\vs2022-x64-release
set REPO_ROOT=%WINDOWS_ROOT%\..\..
set STAGING_DIR=%~dp0staging
set EXE_PATH=%BUILD_DIR%\Release\%APP_NAME%.exe

echo -- %APP_NAME% -- Windows Installer Build --
echo   Build dir:   %BUILD_DIR%
echo   Repo root:   %REPO_ROOT%
echo   Staging:     %STAGING_DIR%
echo.

REM Step 1: Verify build exists. VS generator puts the exe in Release\ subdir;
REM Ninja builds land it directly in the build dir. Check both.
if not exist "%EXE_PATH%" (
    set EXE_PATH=%BUILD_DIR%\%APP_NAME%.exe
)
if not exist "%EXE_PATH%" (
    echo ERROR: %APP_NAME%.exe not found under %BUILD_DIR%.
    echo        Run: cmake --build --preset vs2022-x64-release
    exit /b 1
)

REM Step 2: Clean staging directory.
if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
mkdir "%STAGING_DIR%"

REM Step 3: Copy executable + every DLL that CMake's post-build deployed
REM alongside it (vcpkg + windeployqt output).
echo Staging exe and runtime DLLs...
copy /Y "%EXE_PATH%" "%STAGING_DIR%\" >nul
for %%f in ("%BUILD_DIR%\Release\*.dll" "%BUILD_DIR%\*.dll") do (
    if exist "%%f" copy /Y "%%f" "%STAGING_DIR%\" >nul
)
for %%d in (platforms styles imageformats multimedia tls sqldrivers iconengines) do (
    if exist "%BUILD_DIR%\Release\%%d"  xcopy /E /I /Y "%BUILD_DIR%\Release\%%d"  "%STAGING_DIR%\%%d"  >nul
    if exist "%BUILD_DIR%\%%d"          xcopy /E /I /Y "%BUILD_DIR%\%%d"          "%STAGING_DIR%\%%d"  >nul
)

REM Step 4: Re-run windeployqt in case the post-build step missed anything
REM (e.g. when building from the VS IDE instead of via `cmake --build`).
where windeployqt >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Running windeployqt...
    windeployqt --release --no-translations --no-system-d3d-compiler ^
        --no-opengl-sw "%STAGING_DIR%\%APP_NAME%.exe"
) else (
    echo WARNING: windeployqt not on PATH -- skipping. Qt DLLs may be incomplete.
)

REM Step 5: Copy application resources from the repo root.
echo Staging resources...
xcopy /E /I /Y "%REPO_ROOT%\themes"  "%STAGING_DIR%\themes"  >nul
xcopy /E /I /Y "%REPO_ROOT%\configs" "%STAGING_DIR%\configs" >nul
xcopy /E /I /Y "%REPO_ROOT%\docs"    "%STAGING_DIR%\docs"    >nul
copy /Y "%REPO_ROOT%\LICENSE"                            "%STAGING_DIR%\"              >nul
copy /Y "%REPO_ROOT%\README.md"                          "%STAGING_DIR%\"              >nul
copy /Y "%REPO_ROOT%\image_resources\app-icon.ico"       "%STAGING_DIR%\app-icon.ico"  >nul
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: image_resources\app-icon.ico missing -- installer will use default NSIS icon.
)

REM Step 6: Build NSIS installer.
where makensis >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: makensis not on PATH. Install NSIS from https://nsis.sourceforge.io/
    exit /b 2
)
echo Building NSIS installer...
cd /d "%STAGING_DIR%"
makensis "%~dp0installer.nsi"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: makensis failed with code %ERRORLEVEL%.
    exit /b 3
)

echo.
echo -- Done. Installer: %STAGING_DIR%\%APP_NAME%-Setup.exe --
endlocal
