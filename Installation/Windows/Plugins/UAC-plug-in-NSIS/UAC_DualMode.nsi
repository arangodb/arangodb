!addplugindir ".\release"

!define S_NAME "UAC_ModeSelection example"
!define APPNAME "UAC_Dummy_App"
!define S_DEFINSTDIR_USER "$LocalAppData\${APPNAME}"
!define S_DEFINSTDIR_ADMIN "$ProgramFiles\${APPNAME}"
!define UNINSTALLER_FULLPATH "$InstDir\Uninstaller.exe"

!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\orange-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\orange-uninstall.ico"

Name "${S_NAME}"
OutFile "${S_NAME}.exe"

!include MUI2.nsh
!include UAC.nsh
!include nsDialogs.nsh

!ifndef BCM_SETSHIELD
!define BCM_SETSHIELD 0x0000160C
!endif

!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_UNFINISHPAGE_NOAUTOCLOSE


!macro BUILD_LANGUAGES
!insertmacro MUI_LANGUAGE "English"
!macroend


/***************************************************
** Installer
***************************************************/
!macro SetInstMode m
StrCpy $InstMode ${m}
call InstModeChanged
!macroend

!macro BUILD_INSTALLER
!define MUI_COMPONENTSPAGE_NODESC
!define MUI_CUSTOMFUNCTION_GUIINIT GuiInit

!insertmacro MUI_PAGE_LICENSE History.txt
page custom InstModeSelectionPage_Create InstModeSelectionPage_Leave
!define MUI_PAGE_CUSTOMFUNCTION_PRE disableBack
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION FinishRun
!insertmacro MUI_PAGE_FINISH

var InstMode ;0=current,1=all users

Function InstModeChanged
SetShellVarContext CURRENT
${IfNotThen} ${Silent} ${|} StrCpy $InstDir "${S_DEFINSTDIR_USER}" ${|}
${If} $InstMode > 0
	SetShellVarContext ALL
	${IfNotThen} ${Silent} ${|} StrCpy $InstDir "${S_DEFINSTDIR_ADMIN}" ${|}
${EndIf}
FunctionEnd

Function .onInit
!insertmacro UAC_PageElevation_OnInit
${If} ${UAC_IsInnerInstance}
${AndIfNot} ${UAC_IsAdmin}
	SetErrorLevel 0x666666 ;special return value for outer instance so it knows we did not have admin rights
	Quit
${EndIf}

StrCpy $InstMode 0
${IfThen} ${UAC_IsAdmin} ${|} StrCpy $InstMode 1 ${|}
call InstModeChanged

${If} ${Silent}
${AndIf} $InstDir == "" ;defaults (for silent installs)
	SetSilent normal
	call InstModeChanged
	SetSilent silent
${EndIf}
FunctionEnd

Function GuiInit
!insertmacro UAC_PageElevation_OnGuiInit
FunctionEnd

Function disableBack
${If} ${UAC_IsInnerInstance}
	GetDlgItem $0 $HWNDParent 3
	EnableWindow $0 0
${EndIf}
FunctionEnd



Function RemoveNextBtnShield
GetDlgItem $0 $hwndParent 1
SendMessage $0 ${BCM_SETSHIELD} 0 0
FunctionEnd

Function InstModeSelectionPage_Create
!insertmacro MUI_HEADER_TEXT_PAGE "Select install type" "Blah blah blah blah"
GetFunctionAddress $8 InstModeSelectionPage_OnClick
nsDialogs::Create /NOUNLOAD 1018
Pop $9
${NSD_OnBack} RemoveNextBtnShield
${NSD_CreateLabel} 0 20u 75% 20u "Blah blah blah blah select install type..."
Pop $0
System::Call "advapi32::GetUserName(t.r0,*i${NSIS_MAX_STRLEN})i"
${NSD_CreateRadioButton} 0 40u 75% 15u "Single User ($0)"
Pop $0
nsDialogs::OnClick $0 $8
nsDialogs::SetUserData $0 0
SendMessage $0 ${BM_CLICK} 0 0
${NSD_CreateRadioButton} 0 60u 75% 15u "All users"
Pop $2
nsDialogs::OnClick $2 $8
nsDialogs::SetUserData $2 1
${IfThen} $InstMode <> 0 ${|} SendMessage $2 ${BM_CLICK} 0 0 ${|}
push $2 ;store allusers radio hwnd on stack
nsDialogs::Show
pop $2
FunctionEnd

Function InstModeSelectionPage_OnClick
pop $1
nsDialogs::GetUserData $1
pop $1
GetDlgItem $0 $hwndParent 1
SendMessage $0 ${BCM_SETSHIELD} 0 $1
FunctionEnd

Function InstModeSelectionPage_Leave
pop $0  ;get hwnd
push $0 ;and put it back
${NSD_GetState} $0 $9
${If} $9 = 0
	!insertmacro SetInstMode 0
${Else}
	!insertmacro SetInstMode 1
	${IfNot} ${UAC_IsAdmin}
		GetDlgItem $9 $HWNDParent 1
		System::Call user32::GetFocus()i.s
		EnableWindow $9 0 ;disable next button
		!insertmacro UAC_PageElevation_RunElevated
		EnableWindow $9 1
		System::Call user32::SetFocus(is) ;Do we need WM_NEXTDLGCTL or can we get away with this hack?
		${If} $2 = 0x666666 ;our special return, the new process was not admin after all 
			MessageBox mb_iconExclamation "You need to login with an account that is a member of the admin group to continue..."
			Abort 
		${ElseIf} $0 = 1223 ;cancel
			Abort
		${Else}
			${If} $0 <> 0
				${If} $0 = 1062
					MessageBox mb_iconstop "Unable to elevate, Secondary Logon service not running!"
				${Else}
					MessageBox mb_iconstop "Unable to elevate, error $0"
				${EndIf} 
				Abort
			${EndIf}
		${EndIf} 
		Quit ;We now have a new process, the install will continue there, we have nothing left to do here
	${EndIf}
${EndIf}
FunctionEnd

Function FinishRun
!insertmacro UAC_AsUser_ExecShell "" "calc.exe" "" "" ""
FunctionEnd

Section "Required Files"
SectionIn RO
SectionEnd

Section "Start menu shortcuts"
CreateShortcut "$smprograms\${APPNAME}.lnk" '"${UNINSTALLER_FULLPATH}"'
SectionEnd

Section Uninstaller
SetOutPath -
${If} $InstMode = 0
	!insertmacro CreateUninstaller "${UNINSTALLER_FULLPATH}" 0
${Else}
	!insertmacro CreateUninstaller "${UNINSTALLER_FULLPATH}" 1
${EndIf}
WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" DisplayName "${APPNAME}"
WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" UninstallString "${UNINSTALLER_FULLPATH}"
WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" InstallLocation $InstDir
WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" NoModify 1
WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" NoRepair 1
SectionEnd
!macroend

/***************************************************
** Uninstaller
***************************************************/
!macro BUILD_UNINSTALLER
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
;!insertmacro MUI_UNPAGE_FINISH

Function UN.onInit
!if ${BUILDUNINST} > 0
	SetShellVarContext ALL
	!insertmacro UAC_RunElevated
	${Switch} $0
	${Case} 0
		${IfThen} $1 = 1 ${|} Quit ${|} ;we are the outer process, the inner process has done its work, we are done
		${IfThen} $3 <> 0 ${|} ${Break} ${|} ;we are admin, let the show go on
		;fall-through and die
	${Case} 1223
		MessageBox mb_IconStop|mb_TopMost|mb_SetForeground "This uninstaller requires admin privileges, aborting!"
		Quit
	${Case} 1062
		MessageBox mb_IconStop|mb_TopMost|mb_SetForeground "Logon service not running, aborting!"
		Quit
	${Default}
		MessageBox mb_IconStop|mb_TopMost|mb_SetForeground "Unable to elevate , error $0"
		Quit
	${EndSwitch}
!endif
FunctionEnd

Section -un.Main
Delete "$smprograms\${APPNAME}.lnk"
Delete "${UNINSTALLER_FULLPATH}"
DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
RMDir $instdir
SectionEnd
!macroend



/***************************************************/
!macro CreateUninstaller extractTo mode
!tempfile UNINSTEXE
!system '"${NSISDIR}\MakeNSIS" /DBUILDUNINST=${mode} /DUNINSTEXE=${UNINSTEXE} "${__FILE__}"' = 0
!system '"${UNINSTEXE}"' = 0
File "/oname=${extractTo}" "${UNINSTEXE}.un"
!delfile "${UNINSTEXE}"
!delfile "${UNINSTEXE}.un"
!undef UNINSTEXE
!macroend

!ifndef BUILDUNINST
	RequestExecutionLevel user
	!insertmacro BUILD_INSTALLER
!else
	SilentInstall silent
	OutFile "${UNINSTEXE}"
	!if ${BUILDUNINST} > 0
		RequestExecutionLevel admin
	!else
		RequestExecutionLevel user
	!endif
	!insertmacro MUI_PAGE_INSTFILES
	!insertmacro BUILD_UNINSTALLER
	Section
	WriteUninstaller "${UNINSTEXE}.un"
	SectionEnd
!endif
!insertmacro BUILD_LANGUAGES



