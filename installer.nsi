; Stock Exchange Analyser Installer
; NSIS script — produces a single setup.exe

Unicode True
RequestExecutionLevel admin

; ---------- metadata ----------
!define PRODUCT_NAME "Stock Exchange Analyser"
!define PRODUCT_VERSION "1.0"
!define PRODUCT_PUBLISHER "Amogh Shetty"
!define PRODUCT_WEB_SITE "https://github.com/amoghshetty-hilol/C-PBL-Stock_Analyzer"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\stock_analyser.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "Stock_Analyser_Setup.exe"
InstallDir "$PROGRAMFILES64\Stock Exchange Analyser"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails hide
ShowUnInstDetails hide

; ---------- compression ----------
SetCompressor /SOLID lzma
SetCompressorDictSize 32

; ---------- MUI ----------
!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\stock_analyser.exe"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ---------- sections ----------
Section "Install" SEC01
  SetOutPath "$INSTDIR"

  ; Main executable
  File "stock_analyser.exe"

  ; Sample data CSVs
  File "AAPL.csv"
  File "GOOG.csv"
  File "MSFT.csv"
  File "NVDA.csv"

  ; Data-fetching scripts
  File "fetch_stock.py"

  ; Docs
  File "README.md"

  ; Exports directory placeholder
  CreateDirectory "$INSTDIR\exports"

  ; Source code with folder structure preserved
  SetOutPath "$INSTDIR\src"
  File "src\main.c"
  File "src\parser.c"
  File "src\analytics.c"
  SetOutPath "$INSTDIR\include"
  File "include\common.h"
  File "include\parser.h"
  File "include\analytics.h"
  SetOutPath "$INSTDIR"
  File "Makefile"
  File "installer.nsi"

  ; Shortcuts
  CreateDirectory "$SMPROGRAMS\Stock Exchange Analyser"
  CreateShortCut "$SMPROGRAMS\Stock Exchange Analyser\Stock Exchange Analyser.lnk" "$INSTDIR\stock_analyser.exe" "" "$INSTDIR\stock_analyser.exe" 0
  CreateShortCut "$SMPROGRAMS\Stock Exchange Analyser\Uninstall.lnk" "$INSTDIR\uninst.exe" "" "$INSTDIR\uninst.exe" 0
  CreateShortCut "$DESKTOP\Stock Exchange Analyser.lnk" "$INSTDIR\stock_analyser.exe" "" "$INSTDIR\stock_analyser.exe" 0

  ; Uninstaller
  WriteUninstaller "$INSTDIR\uninst.exe"

  ; Registry — Add/Remove Programs
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\stock_analyser.exe"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\stock_analyser.exe"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd

; ---------- uninstall ----------
Section "Uninstall"
  Delete "$INSTDIR\stock_analyser.exe"
  Delete "$INSTDIR\AAPL.csv"
  Delete "$INSTDIR\GOOG.csv"
  Delete "$INSTDIR\MSFT.csv"
  Delete "$INSTDIR\NVDA.csv"
  Delete "$INSTDIR\fetch_stock.py"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\uninst.exe"

  ; Source code files
  Delete "$INSTDIR\src\*.c"
  Delete "$INSTDIR\include\*.h"
  Delete "$INSTDIR\Makefile"
  Delete "$INSTDIR\installer.nsi"
  RMDir "$INSTDIR\src"
  RMDir "$INSTDIR\include"

  ; Generated exports
  RMDir /r "$INSTDIR\exports"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\Stock Exchange Analyser\Stock Exchange Analyser.lnk"
  Delete "$SMPROGRAMS\Stock Exchange Analyser\Uninstall.lnk"
  RMDir "$SMPROGRAMS\Stock Exchange Analyser"
  Delete "$DESKTOP\Stock Exchange Analyser.lnk"

  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"
SectionEnd
