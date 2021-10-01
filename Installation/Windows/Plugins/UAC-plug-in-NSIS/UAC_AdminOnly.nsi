
!define S_NAME "UAC_AdminOnly example"
Name "${S_NAME}"
OutFile "${S_NAME}.exe"
RequestExecutionLevel user
ShowInstDetails show

!addplugindir ".\release"
!include LogicLib.nsh
!include UAC.nsh


Function .onInit
uac_tryagain:
!insertmacro UAC_RunElevated
${Switch} $0
${Case} 0
	${IfThen} $1 = 1 ${|} Quit ${|} ;we are the outer process, the inner process has done its work, we are done
	${IfThen} $3 <> 0 ${|} ${Break} ${|} ;we are admin, let the show go on
	${If} $1 = 3 ;RunAs completed successfully, but with a non-admin user
		MessageBox mb_YesNo|mb_IconExclamation|mb_TopMost|mb_SetForeground "This installer requires admin privileges, try again" /SD IDNO IDYES uac_tryagain IDNO 0
	${EndIf}
	;fall-through and die
${Case} 1223
	MessageBox mb_IconStop|mb_TopMost|mb_SetForeground "This installer requires admin privileges, aborting!"
	Quit
${Case} 1062
	MessageBox mb_IconStop|mb_TopMost|mb_SetForeground "Logon service not running, aborting!"
	Quit
${Default}
	MessageBox mb_IconStop|mb_TopMost|mb_SetForeground "Unable to elevate , error $0"
	Quit
${EndSwitch}
FunctionEnd

page instfiles

Function getdumpinfo
System::Call "advapi32::GetUserName(t.r9,*i${NSIS_MAX_STRLEN})"
System::Call "kernel32::GetComputerName(t.r8,*i${NSIS_MAX_STRLEN})"
!insertmacro UAC_IsAdmin
StrCpy $7 $0
!insertmacro UAC_IsInnerInstance
StrCpy $6 $0
StrCpy $5 $pluginsdir
!insertmacro UAC_GetIntegrityLevel $0
IntFmt $4 "%#x" $0
#!insertmacro UAC_GetLaunchParams
FunctionEnd

!macro dumpinfo desc
DetailPrint ${desc}
DetailPrint =================
DetailPrint "$8\$9"
DetailPrint IsAdmin=$7
DetailPrint IsInnerInstance=$6
DetailPrint UAC_GetIntegrityLevel=$4
DetailPrint $$pluginsdir=$5
DetailPrint " "
!macroend

Section testsection
Call getdumpinfo
!insertmacro dumpinfo "Inner:"

!insertmacro UAC_AsUser_Call Function getdumpinfo ${UAC_SYNCREGISTERS}
!insertmacro dumpinfo "Outer:"
SectionEnd