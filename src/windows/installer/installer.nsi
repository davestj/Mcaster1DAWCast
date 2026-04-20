; Mcaster1DAWCast — NSIS Installer Script
;
; All `File` and MUI_PAGE_LICENSE paths are resolved relative to the makensis
; working directory, which build-installer.bat sets to the staging/ folder
; *after* pre-staging the exe, Qt plugins, vcpkg DLLs, themes, configs, docs,
; LICENSE, README, and app-icon.ico. This keeps the .nsi free of fragile
; "../../../" paths and makes the installer reproducible from any CI runner.
;
; Portable install — no admin prompt, lands under %USERPROFILE%\Mcaster1.

!include "MUI2.nsh"
!include "FileFunc.nsh"

; ── General ─────────────────────────────────────────────────────────
Name "Mcaster1DAWCast"
OutFile "Mcaster1DAWCast-Setup.exe"
Unicode True
RequestExecutionLevel user
InstallDir "$PROFILE\Mcaster1\Mcaster1DAWCast"

; ── Interface ───────────────────────────────────────────────────────
!define MUI_ABORTWARNING
!define MUI_ICON   "app-icon.ico"   ; pre-staged by build-installer.bat
!define MUI_UNICON "app-icon.ico"

; ── Pages ───────────────────────────────────────────────────────────
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"   ; pre-staged by build-installer.bat
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── Core Section (Required) ─────────────────────────────────────────
Section "Mcaster1DAWCast (required)" SecCore
    SectionIn RO
    SetOutPath "$INSTDIR"

    ; Main executable and every DLL that vcpkg/windeployqt deployed
    ; alongside it (Qt, FFmpeg, PortAudio, TagLib, OpenSSL, codec libs).
    File "Mcaster1DAWCast.exe"
    File /r "*.dll"

    ; Qt plugin trees — /nonfatal on the ones that may be absent depending
    ; on which Qt modules are linked.
    SetOutPath "$INSTDIR\platforms"
    File /nonfatal /r "platforms\*.*"
    SetOutPath "$INSTDIR\styles"
    File /nonfatal /r "styles\*.*"
    SetOutPath "$INSTDIR\imageformats"
    File /nonfatal /r "imageformats\*.*"
    SetOutPath "$INSTDIR\multimedia"
    File /nonfatal /r "multimedia\*.*"
    SetOutPath "$INSTDIR\tls"
    File /nonfatal /r "tls\*.*"
    SetOutPath "$INSTDIR\sqldrivers"
    File /nonfatal /r "sqldrivers\*.*"
    SetOutPath "$INSTDIR\iconengines"
    File /nonfatal /r "iconengines\*.*"

    ; Application resources (themes, project/preset YAML, docs).
    SetOutPath "$INSTDIR\themes"
    File /r "themes\*.*"
    SetOutPath "$INSTDIR\configs"
    File /r "configs\*.*"
    SetOutPath "$INSTDIR\docs"
    File /nonfatal /r "docs\*.*"

    ; License + readme.
    SetOutPath "$INSTDIR"
    File "LICENSE"
    File /nonfatal "README.md"

    ; Uninstaller.
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Add/Remove Programs entry (HKCU — no admin).
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "DisplayName"     "Mcaster1DAWCast"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "DisplayIcon"     "$INSTDIR\Mcaster1DAWCast.exe,0"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "Publisher"       "Mcaster1 / David St. John"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "URLInfoAbout"    "https://mcaster1.com/dawcast"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "DisplayVersion"  "1.0.0-alpha"
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "NoRepair" 1

    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "EstimatedSize" "$0"
SectionEnd

; ── Shortcuts Section (Optional) ────────────────────────────────────
Section "Desktop & Start Menu Shortcuts" SecShortcuts
    CreateDirectory "$SMPROGRAMS\Mcaster1\Mcaster1DAWCast"
    CreateShortCut "$SMPROGRAMS\Mcaster1\Mcaster1DAWCast\Mcaster1DAWCast.lnk" \
        "$INSTDIR\Mcaster1DAWCast.exe"
    CreateShortCut "$SMPROGRAMS\Mcaster1\Mcaster1DAWCast\Uninstall.lnk" \
        "$INSTDIR\Uninstall.exe"
    CreateShortCut "$DESKTOP\Mcaster1DAWCast.lnk" \
        "$INSTDIR\Mcaster1DAWCast.exe"
SectionEnd

; ── File Associations (Optional) ────────────────────────────────────
; .dcproj = Mcaster1DAWCast project file
Section "File associations (.dcproj)" SecAssoc
    WriteRegStr HKCU "Software\Classes\.dcproj"                        "" "Mcaster1DAWCast.Project"
    WriteRegStr HKCU "Software\Classes\Mcaster1DAWCast.Project"        "" "Mcaster1DAWCast Project"
    WriteRegStr HKCU "Software\Classes\Mcaster1DAWCast.Project\DefaultIcon" \
        "" "$INSTDIR\Mcaster1DAWCast.exe,0"
    WriteRegStr HKCU "Software\Classes\Mcaster1DAWCast.Project\shell\open\command" \
        "" "$\"$INSTDIR\Mcaster1DAWCast.exe$\" $\"%1$\""
SectionEnd

; ── Descriptions ────────────────────────────────────────────────────
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecCore}      "Core application, Qt plugins, runtime DLLs, themes, presets (required)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} "Create Desktop and Start Menu shortcuts"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecAssoc}     "Associate .dcproj files with Mcaster1DAWCast"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ── Uninstaller ─────────────────────────────────────────────────────
Section "Uninstall"
    ; Plugin + resource trees.
    RMDir /r "$INSTDIR\platforms"
    RMDir /r "$INSTDIR\styles"
    RMDir /r "$INSTDIR\imageformats"
    RMDir /r "$INSTDIR\multimedia"
    RMDir /r "$INSTDIR\tls"
    RMDir /r "$INSTDIR\sqldrivers"
    RMDir /r "$INSTDIR\iconengines"
    RMDir /r "$INSTDIR\themes"
    RMDir /r "$INSTDIR\configs"
    RMDir /r "$INSTDIR\docs"

    ; Top-level files (exe + all DLLs + license + uninstaller).
    Delete "$INSTDIR\Mcaster1DAWCast.exe"
    Delete "$INSTDIR\*.dll"
    Delete "$INSTDIR\LICENSE"
    Delete "$INSTDIR\README.md"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
    RMDir "$PROFILE\Mcaster1"   ; only succeeds if empty — leaves sibling apps intact

    ; Shortcuts.
    Delete "$DESKTOP\Mcaster1DAWCast.lnk"
    RMDir /r "$SMPROGRAMS\Mcaster1\Mcaster1DAWCast"
    RMDir "$SMPROGRAMS\Mcaster1"

    ; Registry.
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast"
    DeleteRegKey HKCU "Software\Classes\.dcproj"
    DeleteRegKey HKCU "Software\Classes\Mcaster1DAWCast.Project"
SectionEnd
