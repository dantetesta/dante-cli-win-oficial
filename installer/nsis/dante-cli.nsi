; Dante CLI — NSIS installer script
;
; Compile (Mac/Linux/Windows):
;   makensis -DDIST_DIR=../../dist/Release -DOUT_DIR=../../dist/installer dante-cli.nsi
;
; Requires: NSIS 3.x

Unicode true
ManifestSupportedOS all
ManifestDPIAware true

!define APPNAME            "Dante CLI"
!define APPVERSION         "1.0.31"
!define COMPANYNAME        "Dante"
!define APPEXE             "Dante CLI.exe"
!define APPID              "DanteCLI"
!define APPURL             "https://dante.cli"

!ifndef DIST_DIR
  !define DIST_DIR "..\..\dist\Release"
!endif
!ifndef OUT_DIR
  !define OUT_DIR "..\..\dist\installer"
!endif

Name "${APPNAME}"
OutFile "${OUT_DIR}\DanteCLI-Setup-${APPVERSION}-x64.exe"
InstallDir "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey HKLM "Software\${COMPANYNAME}\${APPID}" "InstallDir"
RequestExecutionLevel admin
ShowInstDetails show
ShowUninstDetails show

VIProductVersion "1.0.31.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "FileVersion" "1.0.31.0"
VIAddVersionKey "ProductVersion" "1.0.31"
VIAddVersionKey "FileDescription" "${APPNAME} installer"
VIAddVersionKey "LegalCopyright" "(c) ${COMPANYNAME}. MIT License."
VIAddVersionKey "CompanyName" "${COMPANYNAME}"

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "..\..\resources\icons\app.ico"
!define MUI_UNICON "..\..\resources\icons\app.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP|MB_OK "${APPNAME} requer Windows 10/11 de 64 bits."
    Abort
  ${EndIf}
FunctionEnd

Section "Aplicativo principal" SecCore
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${DIST_DIR}\*.*"

  WriteRegStr HKLM "Software\${COMPANYNAME}\${APPID}" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\${COMPANYNAME}\${APPID}" "Version" "${APPVERSION}"

  ; Add/Remove Programs entry
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}" \
                "DisplayName" "${APPNAME}"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}" \
                "DisplayIcon" "$INSTDIR\${APPEXE}"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}" \
                "DisplayVersion" "${APPVERSION}"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}" \
                "Publisher" "${COMPANYNAME}"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}" \
                "URLInfoAbout" "${APPURL}"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}" \
                "InstallLocation" "$INSTDIR"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}" \
                "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}" \
                "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}" \
                "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}" \
                "NoRepair" 1

  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}" \
                "EstimatedSize" "$0"

  WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "Atalho no menu Iniciar" SecStartMenu
  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut  "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"  "$INSTDIR\${APPEXE}"
  CreateShortcut  "$SMPROGRAMS\${APPNAME}\Desinstalar.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section "Atalho na área de trabalho" SecDesktop
  CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${APPEXE}"
SectionEnd

Section /o "Associar com terminais do sistema" SecAssoc
  ; Reserved for future protocol/file association registrations.
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${SecCore}      "Arquivos principais do Dante CLI (obrigatório)."
!insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} "Cria atalhos no menu Iniciar."
!insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop}   "Cria um atalho na área de trabalho."
!insertmacro MUI_DESCRIPTION_TEXT ${SecAssoc}     "Reserva associações futuras."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  Delete "$DESKTOP\${APPNAME}.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\Desinstalar.lnk"
  RMDir  "$SMPROGRAMS\${APPNAME}"

  ; Remove app files
  RMDir /r "$INSTDIR"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}"
  DeleteRegKey HKLM "Software\${COMPANYNAME}\${APPID}"
SectionEnd
