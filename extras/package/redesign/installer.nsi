; Reframe installer, 2026-10-12. GPL-2.0-or-later.
; Uses NSIS Modern UI 2, the same wizard framework as VLC's installer.
Unicode true
!include MUI2.nsh
!include LogicLib.nsh
!include x64.nsh

!define PRODUCT "Reframe"
!define VERSION "3.0.24-r6"
!define REGKEY "Software\Reframe"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Reframe"
Name "${PRODUCT}"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\Reframe"
InstallDirRegKey HKLM "${REGKEY}" "InstallDir"
RequestExecutionLevel admin
ManifestDPIAware true
SetCompressor /SOLID lzma
SetOverwrite on
SetDateSave on
BrandingText "Reframe - unofficial VLC fork"
VIProductVersion "3.0.24.6"
VIAddVersionKey /LANG=1033 "ProductName" "Reframe"
VIAddVersionKey /LANG=1033 "FileDescription" "Reframe media player setup"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "VideoLAN and VLC authors; fork implementation webmaster94"

!define MUI_ABORTWARNING
!define MUI_ICON "${STAGE}\reframe.ico"
!define MUI_UNICON "${STAGE}\reframe.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${STAGE}\reframe-branding.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${STAGE}\reframe-branding.bmp"
!define MUI_WELCOMEPAGE_TITLE "Welcome to Reframe Setup"
!define MUI_WELCOMEPAGE_TEXT "Reframe is an unofficial VLC media player fork with an interface adapted from Juxtopposed's VLC Redesign.$\r$\n$\r$\nThis wizard installs Reframe alongside your existing VLC installation.$\r$\n$\r$\nClose Reframe before updating it."
; This installer is elevated. Exec would make the media player inherit that token,
; including when it reads ordinary user settings or decodes untrusted media.
!define MUI_FINISHPAGE_TEXT "Reframe has been installed.$\r$\n$\r$\nStart Reframe from the Start Menu or its desktop shortcut after closing this installer."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE}\COPYING.GPL3.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${Unless} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "Reframe requires 64-bit Windows."
    Quit
  ${EndUnless}
  SetRegView 64
  SetShellVarContext all
FunctionEnd

Section "Media player (required)" Player
  SectionIn RO
  !include "${INSTALL_MANIFEST}"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "${REGKEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "${REGKEY}" "Version" "${VERSION}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "Reframe"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "webmaster94 - unofficial VLC fork"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" '$\"$INSTDIR\uninstall.exe$\"'
  WriteRegStr HKLM "${UNINSTALL_KEY}" "QuietUninstallString" '$\"$INSTDIR\uninstall.exe$\" /S'
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\reframe.exe"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "URLInfoAbout" "https://github.com/webmaster94/vlc-redesign"
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section "Start Menu shortcuts" StartMenu
  CreateDirectory "$SMPROGRAMS\Reframe"
  CreateShortcut "$SMPROGRAMS\Reframe\Reframe.lnk" "$INSTDIR\reframe.exe"
  CreateShortcut "$SMPROGRAMS\Reframe\Uninstall Reframe.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section /o "Desktop shortcut" Desktop
  CreateShortcut "$DESKTOP\Reframe.lnk" "$INSTDIR\reframe.exe"
SectionEnd

; Register only the fork's Open With entries. Never replace extension defaults.
!macro RegisterType EXT
  WriteRegStr HKCR "Reframe.${EXT}" "" "${EXT} media (Reframe)"
  WriteRegStr HKCR "Reframe.${EXT}\DefaultIcon" "" '$\"$INSTDIR\reframe.exe$\",0'
  WriteRegStr HKCR "Reframe.${EXT}\shell\open\command" "" '$\"$INSTDIR\reframe.exe$\" --started-from-file $\"%1$\"'
  WriteRegStr HKCR ".${EXT}\OpenWithProgids" "Reframe.${EXT}" ""
!macroend
Section /o "Add to Open With for media files" OpenWith
  !include "${ASSOCIATIONS_INSTALL}"
  WriteRegStr HKCR "Applications\reframe.exe" "FriendlyAppName" "Reframe"
  WriteRegStr HKCR "Applications\reframe.exe\shell\open\command" "" '$\"$INSTDIR\reframe.exe$\" --started-from-file $\"%1$\"'
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${Player} "Installs the player, playback plugins, license notices, and credits."
  !insertmacro MUI_DESCRIPTION_TEXT ${StartMenu} "Creates Reframe shortcuts in its own Start Menu folder."
  !insertmacro MUI_DESCRIPTION_TEXT ${Desktop} "Creates a Reframe desktop shortcut."
  !insertmacro MUI_DESCRIPTION_TEXT ${OpenWith} "Adds Reframe to Open With. Existing default media applications remain selected."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function un.onInit
  SetRegView 64
  SetShellVarContext all
FunctionEnd
!macro UnregisterType EXT
  DeleteRegValue HKCR ".${EXT}\OpenWithProgids" "Reframe.${EXT}"
  DeleteRegKey HKCR "Reframe.${EXT}"
!macroend
Section "Uninstall"
  !include "${ASSOCIATIONS_UNINSTALL}"
  DeleteRegKey HKCR "Applications\reframe.exe"
  Delete "$INSTDIR\plugins\plugins.dat"
  !include "${UNINSTALL_MANIFEST}"
  Delete "$SMPROGRAMS\Reframe\Reframe.lnk"
  Delete "$SMPROGRAMS\Reframe\Uninstall Reframe.lnk"
  RMDir "$SMPROGRAMS\Reframe"
  Delete "$DESKTOP\Reframe.lnk"
  DeleteRegKey HKLM "${UNINSTALL_KEY}"
  DeleteRegKey HKLM "${REGKEY}"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  ; Reframe preferences and user files are retained. No recursive removal.
SectionEnd
