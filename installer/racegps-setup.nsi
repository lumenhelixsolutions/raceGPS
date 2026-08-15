; raceGPS Windows Installer
; NSIS Script — Professional game installer with preflight onboarding
; Requires: NSIS 3.x + nsProcess plugin

!define PRODUCT_NAME "raceGPS"
!define PRODUCT_VERSION "0.2.0"
!define PRODUCT_PUBLISHER "LumenHelix Solutions"
!define PRODUCT_WEB_SITE "https://github.com/lumenhelixsolutions/raceGPS"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\raceGPS.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"
!define MIN_RAM_MB "8192"
!define MIN_DISK_MB "5120"

; MUI 2
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!include "nsDialogs.nsh"

; Variables (kept minimal; no longer used by removed custom pages)

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Make finish page stable and simple
!define MUI_FINISHPAGE_NOAUTOCLOSE

; Pages - standard MUI only, no custom nsDialogs pages (to eliminate all crash risks on preflight/finish).
; All "preflight" checks and payload validation are in the build script (build-windows-installer.ps1) which prints a full report.
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Language
!insertmacro MUI_LANGUAGE "English"

; Installer sections
Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "raceGPS-v${PRODUCT_VERSION}-Win64-Setup.exe"
InstallDir "$PROGRAMFILES64\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show
RequestExecutionLevel admin

; (Preflight custom page and function removed entirely - now handled in build script for maximum stability.)

; ============================================================
; COMPONENTS
; ============================================================
Section "Game Files" SEC_GAME
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /nonfatal /r "..\apps\unreal-akron-beta\Build\Windows\*.*"
    SetOutPath "$INSTDIR\citypacks"
    File /r "..\citypacks\*.*"
SectionEnd

Section "Akron Citypack (Default)" SEC_CITYPACK
    SectionIn RO
    DetailPrint "Akron citypack bundled."
SectionEnd

Section "Visual C++ Redistributables" SEC_VCREDIST
    DetailPrint "Installing VC++ 2015-2022 Redistributables..."
    SetOutPath "$TEMP"
    NSISdl::download "https://aka.ms/vs/17/release/vc_redist.x64.exe" "$TEMP\vc_redist.x64.exe"
    Pop $R0
    ${If} $R0 == "success"
        ExecWait '"$TEMP\vc_redist.x64.exe" /install /quiet /norestart'
    ${Else}
        DetailPrint "WARNING: VC++ Redist download failed. Game may not run."
    ${EndIf}
SectionEnd

Section "Desktop Shortcut" SEC_SHORTCUT
    CreateShortcut "$DESKTOP\raceGPS.lnk" "$INSTDIR\raceGPS.exe" "" "$INSTDIR\raceGPS.exe" 0
SectionEnd

Section "Start Menu Shortcuts" SEC_STARTMENU
    CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
    CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\Play raceGPS.lnk" "$INSTDIR\raceGPS.exe"
    CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

; ============================================================
; POST-INSTALL
; ============================================================
Section -Post
    WriteUninstaller "$INSTDIR\uninst.exe"
    WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\raceGPS.exe"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\raceGPS.exe"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "NoModify" 1
    WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "NoRepair" 1
SectionEnd

; ============================================================
; UNINSTALLER
; ============================================================
Section Uninstall
    Delete "$INSTDIR\uninst.exe"
    Delete "$DESKTOP\raceGPS.lnk"
    RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"
    RMDir /r "$INSTDIR"
    DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
    DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
    SetAutoClose true
SectionEnd

; Standard MUI finish with auto-run option (stable, no custom nsDialogs).
!define MUI_FINISHPAGE_RUN "$INSTDIR\raceGPS.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch raceGPS"
