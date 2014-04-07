/*
Basic script for a all users/shared installer that runs the installed application as the correct user.
*/

!define S_NAME "UAC_Basic example"
Name "${S_NAME}"
OutFile "${S_NAME}.exe"
RequestExecutionLevel user ; << Required, you cannot use admin!
InstallDir "$ProgramFiles\${S_NAME}"

!include MUI2.nsh
!include UAC.nsh

!macro Init thing
uac_tryagain:
!insertmacro UAC_RunElevated
${Switch} $0
${Case} 0
	${IfThen} $1 = 1 ${|} Quit ${|} ;we are the outer process, the inner process has done its work, we are done
	${IfThen} $3 <> 0 ${|} ${Break} ${|} ;we are admin, let the show go on
	${If} $1 = 3 ;RunAs completed successfully, but with a non-admin user
		MessageBox mb_YesNo|mb_IconExclamation|mb_TopMost|mb_SetForeground "This ${thing} requires admin privileges, try again" /SD IDNO IDYES uac_tryagain IDNO 0
	${EndIf}
	;fall-through and die
${Case} 1223
	MessageBox mb_IconStop|mb_TopMost|mb_SetForeground "This ${thing} requires admin privileges, aborting!"
	Quit
${Case} 1062
	MessageBox mb_IconStop|mb_TopMost|mb_SetForeground "Logon service not running, aborting!"
	Quit
${Default}
	MessageBox mb_IconStop|mb_TopMost|mb_SetForeground "Unable to elevate , error $0"
	Quit
${EndSwitch}

SetShellVarContext all
!macroend

Function .onInit
!insertmacro Init "installer"
FunctionEnd

Function un.onInit
!insertmacro Init "uninstaller"
FunctionEnd

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION PageFinishRun
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"


Function PageFinishRun
; You would run "$InstDir\MyApp.exe" here but this example has no application to execute...
!insertmacro UAC_AsUser_ExecShell "" "$WinDir\notepad.exe" "" "" ""
FunctionEnd

Section
SetOutPath $InstDir
# TODO: File "MyApp.exe"
WriteUninstaller "$InstDir\Uninstall.exe"
SectionEnd

Section Uninstall
SetOutPath $Temp ; Make sure $InstDir is not the current directory so we can remove it
# TODO: Delete "$InstDir\MyApp.exe"
Delete "$InstDir\Uninstall.exe"
RMDir "$InstDir"
SectionEnd