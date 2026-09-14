; Soraivy for OBS - Windows installer (NSIS 3, Modern UI 2).
; Built by .github/scripts/Package-Windows.ps1, which passes all !defines.
; Layout mirrors the CMake install staging:
;   <staging>/<product>/bin/64bit/*.dll  ->  <obs>/obs-plugins/64bit/
;   <staging>/<product>/data/*            ->  <obs>/data/obs-plugins/<product>/

!ifndef PRODUCT_NAME
  !define PRODUCT_NAME "soraivy-obs"
!endif
!ifndef PRODUCT_VERSION
  !define PRODUCT_VERSION "0.0.0"
!endif
!ifndef PRODUCT_PUBLISHER
  !define PRODUCT_PUBLISHER "Soraivy, Inc."
!endif
!ifndef STAGING_DIR
  !define STAGING_DIR "."
!endif
!ifndef OUT_FILE
  !define OUT_FILE "${PRODUCT_NAME}-Installer.exe"
!endif
!ifndef LICENSE_FILE
  !define LICENSE_FILE "LICENSE"
!endif

Unicode True
SetCompressor /SOLID lzma
RequestExecutionLevel admin

Name "Soraivy for OBS ${PRODUCT_VERSION}"
OutFile "${OUT_FILE}"
InstallDir "$PROGRAMFILES64\obs-studio"

!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_LICENSEPAGE_TEXT_TOP "Press Page Down to see the rest of the agreement. Once you are aware of your rights, click Next to continue."
!define MUI_FINISHPAGE_NOAUTOCLOSE

!insertmacro MUI_PAGE_LICENSE "${LICENSE_FILE}"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Install"
  SetOutPath "$INSTDIR\obs-plugins\64bit"
  File "${STAGING_DIR}\${PRODUCT_NAME}\bin\64bit\${PRODUCT_NAME}.dll"
  File /nonfatal "${STAGING_DIR}\${PRODUCT_NAME}\bin\64bit\${PRODUCT_NAME}.pdb"

  SetOutPath "$INSTDIR\data\obs-plugins\${PRODUCT_NAME}"
  File /nonfatal /r "${STAGING_DIR}\${PRODUCT_NAME}\data\*.*"

  WriteUninstaller "$INSTDIR\data\obs-plugins\${PRODUCT_NAME}\uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" \
    "DisplayName" "Soraivy for OBS ${PRODUCT_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" \
    "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" \
    "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" \
    "UninstallString" "$INSTDIR\data\obs-plugins\${PRODUCT_NAME}\uninstall.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\obs-plugins\64bit\${PRODUCT_NAME}.dll"
  Delete "$INSTDIR\obs-plugins\64bit\${PRODUCT_NAME}.pdb"
  RMDir /r "$INSTDIR\data\obs-plugins\${PRODUCT_NAME}"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
SectionEnd
