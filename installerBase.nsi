;-------------------------------------------------------------------------------
; Includes
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"
!include "x64.nsh"

;-------------------------------------------------------------------------------
; Constants
!define PRODUCT_NAME "MikuMikuWorld for UntitledCharts"
!define PRODUCT_DESCRIPTION "MikuMikuWorld for UntitledCharts"
!define COPYRIGHT "Copyright (c) MikuMikuWorld contributors"
!define PRODUCT_VERSION "{version}.0"
!define SETUP_VERSION "{version}.0"

;-------------------------------------------------------------------------------
; Attributes
Name "MikuMikuWorld for UntitledCharts"
OutFile "build/mmw4uc-{version}-setup.exe"
InstallDir "$LOCALAPPDATA\Programs\mmw4uc"
InstallDirRegKey HKCU "Software\mmw4uc" ""
RequestExecutionLevel user

;-------------------------------------------------------------------------------
; Version Info
VIProductVersion "${PRODUCT_VERSION}"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "FileDescription" "${PRODUCT_DESCRIPTION}"
VIAddVersionKey "LegalCopyright" "${COPYRIGHT}"
VIAddVersionKey "FileVersion" "${SETUP_VERSION}"

;-------------------------------------------------------------------------------
; Modern UI Appearance
!define MUI_ICON "MikuMikuWorld\mmw_icon.ico"

;-------------------------------------------------------------------------------
; Variables
Var StartMenuFolder

;-------------------------------------------------------------------------------
; Installer Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY

!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\mmw4uc"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder

!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

;-------------------------------------------------------------------------------
; Uninstaller Pages
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_DIRECTORY
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

;-------------------------------------------------------------------------------
; Languages
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Japanese"

;-------------------------------------------------------------------------------
; Installer Sections
Section "MikuMikuWorld for UntitledCharts" Mmw4uc
	SetOutPath "$INSTDIR"
  File /r "build\MikuMikuWorld\*.*"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\MikuMikuWorld for UntitledCharts.lnk" "$INSTDIR\MikuMikuWorld.exe"
    CreateShortcut "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  !insertmacro MUI_STARTMENU_WRITE_END

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\mmw4uc" \
                   "DisplayName" "MikuMikuWorld for UntitledCharts"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\mmw4uc" \
                   "UninstallString" "$\"$INSTDIR\uninstall.exe$\""

  WriteRegStr HKCU "Software\Classes\.usc" "" "MikuMikuWorld.usc"
  WriteRegStr HKCU "Software\Classes\MikuMikuWorld.usc" "" "Universal Sekai Chart File"
  WriteRegStr HKCU "Software\Classes\MikuMikuWorld.usc\DefaultIcon" "" "$INSTDIR\MikuMikuWorld.exe"
  WriteRegStr HKCU "Software\Classes\MikuMikuWorld.usc\shell\open\command" "" "$INSTDIR\MikuMikuWorld.exe $\"%1$\""

  WriteRegStr HKCU "Software\Classes\.unchmmws" "" "MikuMikuWorld.unchmmws"
  WriteRegStr HKCU "Software\Classes\MikuMikuWorld.unchmmws" "" "MMW4UC Chart File"
  WriteRegStr HKCU "Software\Classes\MikuMikuWorld.unchmmws\DefaultIcon" "" "$INSTDIR\MikuMikuWorld.exe"
  WriteRegStr HKCU "Software\Classes\MikuMikuWorld.unchmmws\shell\open\command" "" "$INSTDIR\MikuMikuWorld.exe $\"%1$\""
SectionEnd

;-------------------------------------------------------------------------------
; Uninstaller Sections
Section "Uninstall"
	RMDir /r "$INSTDIR"
  Delete "$INSTDIR\Uninstall.exe"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\mmw4uc"
  DeleteRegKey HKCU "Software\Classes\.usc"
  DeleteRegKey HKCU "Software\Classes\MikuMikuWorld.usc"
  DeleteRegKey HKCU "Software\Classes\.unchmmws"
  DeleteRegKey HKCU "Software\Classes\MikuMikuWorld.unchmmws"

  RMDir /r "$SMPROGRAMS\$StartMenuFolder"
SectionEnd
