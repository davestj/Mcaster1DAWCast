@echo off
REM Mcaster1DAWCast — Windows Installer Build Script
REM Requires: windeployqt, makensis, built Mcaster1DAWCast.exe

setlocal

set APP_NAME=Mcaster1DAWCast
set BUILD_DIR=..\..\build-release
set STAGING_DIR=%~dp0staging
set EXE_PATH=%BUILD_DIR%\%APP_NAME%.exe

echo ── %APP_NAME% — Windows Installer Build ──
echo.

REM Step 1: Verify build exists
if not exist "%EXE_PATH%" (
    echo ERROR: %EXE_PATH% not found. Build the project first.
    exit /b 1
)

REM Step 2: Create clean staging directory
if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
mkdir "%STAGING_DIR%"

REM Step 3: Copy executable
copy "%EXE_PATH%" "%STAGING_DIR%\"

REM Step 4: Run windeployqt
echo Running windeployqt...
windeployqt --release --no-translations --no-system-d3d-compiler ^
    --no-opengl-sw "%STAGING_DIR%\%APP_NAME%.exe"

REM Step 5: Copy application resources
xcopy /E /I /Y "..\..\themes" "%STAGING_DIR%\themes"
xcopy /E /I /Y "..\..\configs" "%STAGING_DIR%\configs"
xcopy /E /I /Y "..\..\docs" "%STAGING_DIR%\docs"
copy "..\..\LICENSE" "%STAGING_DIR%\"
copy "..\..\README.md" "%STAGING_DIR%\"

REM Step 6: Build NSIS installer
echo Building NSIS installer...
cd "%STAGING_DIR%"
makensis "%~dp0installer.nsi"

echo.
echo ── Done. Installer: %STAGING_DIR%\%APP_NAME%-Setup.exe ──
endlocal
