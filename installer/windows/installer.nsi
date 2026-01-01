; Mcaster1DAWCast — NSIS Installer Script
; Portable install to %USERPROFILE%\Mcaster1\Mcaster1DAWCast (no admin required)

!include "MUI2.nsh"
!include "FileFunc.nsh"

; ── General ─────────────────────────────────────────────────────────
Name "Mcaster1DAWCast"
OutFile "Mcaster1DAWCast-Setup.exe"
Unicode True
RequestExecutionLevel user
InstallDir "$PROFILE\Mcaster1\Mcaster1DAWCast"

; ── Variables ───────────────────────────────────────────────────────
Var StartMenuGroup

; ── Interface ───────────────────────────────────────────────────────
!define MUI_ABORTWARNING
!define MUI_ICON "..\..\image_resources\app-icon.ico"
!define MUI_UNICON "..\..\image_resources\app-icon.ico"

; ── Pages ───────────────────────────────────────────────────────────
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
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

    ; Main executable and DLLs
    File "Mcaster1DAWCast.exe"
    File "*.dll"

    ; Qt plugins
    SetOutPath "$INSTDIR\platforms"
    File /r "platforms\*.*"
    SetOutPath "$INSTDIR\styles"
    File /r "styles\*.*"
    SetOutPath "$INSTDIR\imageformats"
    File /r "imageformats\*.*"
    SetOutPath "$INSTDIR\multimedia"
    File /nonfatal /r "multimedia\*.*"
    SetOutPath "$INSTDIR\tls"
    File /nonfatal /r "tls\*.*"

    ; Application resources
    SetOutPath "$INSTDIR\themes"
    File /r "..\..\themes\*.*"
    SetOutPath "$INSTDIR\configs"
    File /r "..\..\configs\*.*"
    SetOutPath "$INSTDIR\docs"
    File /r "..\..\docs\*.*"

    ; License
    SetOutPath "$INSTDIR"
    File "..\..\LICENSE"
    File "..\..\README.md"

    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Registry entries for Add/Remove Programs (HKCU — no admin)
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "DisplayName" "Mcaster1DAWCast"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "Publisher" "Mcaster1"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast" \
        "DisplayVersion" "1.0.0-alpha"
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

; ── Descriptions ────────────────────────────────────────────────────
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecCore} "Core application files (required)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} "Create desktop and Start Menu shortcuts"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ── Uninstaller ─────────────────────────────────────────────────────
Section "Uninstall"
    ; Remove files
    RMDir /r "$INSTDIR\platforms"
    RMDir /r "$INSTDIR\styles"
    RMDir /r "$INSTDIR\imageformats"
    RMDir /r "$INSTDIR\multimedia"
    RMDir /r "$INSTDIR\tls"
    RMDir /r "$INSTDIR\themes"
    RMDir /r "$INSTDIR\configs"
    RMDir /r "$INSTDIR\docs"
    Delete "$INSTDIR\Mcaster1DAWCast.exe"
    Delete "$INSTDIR\*.dll"
    Delete "$INSTDIR\LICENSE"
    Delete "$INSTDIR\README.md"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"

    ; Remove shortcuts
    Delete "$DESKTOP\Mcaster1DAWCast.lnk"
    RMDir /r "$SMPROGRAMS\Mcaster1\Mcaster1DAWCast"

    ; Remove registry
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mcaster1DAWCast"
SectionEnd
