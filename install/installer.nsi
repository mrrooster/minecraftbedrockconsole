;set the following sys env first
;
; MCBC_SRC - source folder
; MCBC_DIST - dist folder (where windeploytool put it's stuff)

SetCompressor /SOLID lzma 

!include "MUI2.nsh"

Name "Minecraft Bedrock Server Console"
OutFile "MCBedrockConsoleInstaller.exe"
Unicode True


InstallDir "$DESKTOP\MCBedrockConsole"
InstallDirRegKey HKCU "Software\MCBedrockConsole" ""

RequestExecutionLevel user

!define MUI_ABORTWARNING

;Pages

!define MUI_WELCOMEPAGE_TITLE $(WELCOME_TITLE)
!define MUI_WELCOMEPAGE_TEXT $(WELCOME_TEXT)
!InsertMacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "$%MCBC_SRC%\LICENCE"
;!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "Minecraft Console"
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\$(^Name)"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME $(START_MENU_FOLDER_TEXT)
Var StartMenuFolder
!insertmacro MUI_PAGE_STARTMENU "MCSC" $StartMenuFolder
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\BedrockConsole.exe"
!define MUI_FINISHPAGE_RUN_TEXT $(FINISH_RUN_TEXT)
!define MUI_FINISHPAGE_SHOWREADME_TEXT $(FINISH_README_TEXT)
!define MUI_FINISHPAGE_SHOWREADME https://www.minecraft.net/en-us/download/server/bedrock/
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED true
!define MUI_FINISHPAGE_LINK $(FINISH_LINK_TEXT)
!define MUI_FINISHPAGE_LINK_LOCATION https://github.com/mrrooster/minecraftbedrockconsole
!insertmacro MUI_PAGE_FINISH
	
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Languages
!insertmacro MUI_LANGUAGE "English"

; Sections
Section MinecraftBedrockServerConsole
  SetOutPath "$INSTDIR"
  File /r $%MCBC_DIST%\*.*
  File $%MCBC_SRC%\LICENCE
  File $%MCBC_SRC%\README.md
  File /r $%MCBC_SRC%\doc
  DetailPrint $(INSTALL_VC_REDIST_TEXT)
  ExecWait "$INSTDIR\vc_redist.x64.exe /install /quiet /norestart"
  Delete "$INSTDIR\vc_redist.x64.exe"
  ;Get Start Menu Folder from registry if available
  !insertmacro MUI_STARTMENU_WRITE_BEGIN MCSC
 
  ;Create shortcuts
  CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
  CreateShortCut "$SMPROGRAMS\$StartMenuFolder\$(^Name).lnk" "$INSTDIR\BedrockConsole.exe" "" "${Icon}"
  CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
 
  ;Store Start Menu Folder in registry
  !insertmacro MUI_STARTMENU_WRITE_END
  WriteRegStr HKCU "Software\$(^Name)" "" $INSTDIR
  WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

;Language strings
LangString DESC_MinecraftBedrockServerConsole ${LANG_ENGLISH} "Minecraft Bedrock Server Console."
LangString WELCOME_TITLE ${LANG_ENGLISH} "Minecraft Bedrock Server Console Installer"
LangString WELCOME_TEXT ${LANG_ENGLISH} "This will install the Minecraft Server Console.$\r$\n$\r$\nYou will also require Minecraft Server (Bedrock Edition)."
LangString FINISH_README_TEXT ${LANG_ENGLISH} "Open the Minecraft server download page"
LangString FINISH_RUN_TEXT ${LANG_ENGLISH} "Start the program"
LangString FINISH_LINK_TEXT ${LANG_ENGLISH} "Visit the project page on Github"
LangString INSTALL_VC_REDIST_TEXT ${LANG_ENGLISH} "Installing the Visual C runtime..."
LangString START_MENU_FOLDER_TEXT ${LANG_ENGLISH} "Start menu folder"

;Assign language strings to sections
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${MinecraftBedrockServerConsole} $(DESC_MinecraftBedrockServerConsole)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir /R /REBOOTOK "$INSTDIR"
  !insertmacro MUI_STARTMENU_GETFOLDER MCSC $StartMenuFolder
  RMDir /r "$SMPROGRAMS\$StartMenuFolder"
  DeleteRegKey HKCU "Software\$(^Name)"
SectionEnd
