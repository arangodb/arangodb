
!define S_NAME "UAC_Tests"
Name "${S_NAME}"
OutFile "$%temp%\${S_NAME}.exe"
RequestExecutionLevel user
ShowInstDetails show
completedtext "Completed, passed $_t_p of $_t_c tests"

!addplugindir ".\Ansi"
!include LogicLib.nsh
!include UAC.nsh

var _t_p
var _t_c
var _t
var test1
var test2

!macro _Test pass
IntOp $_t_c $_t_c + 1
${IfThen} "${pass}" == "PASS" ${|} IntOp $_t_p $_t_p + 1 ${|}
!macroend

!macro TestChk1 testname var test val
StrCpy $_t FAIL
${IfThen} "${var}" ${test} "${val}" ${|} StrCpy $_t PASS ${|}
DetailPrint "$_t:${testname} <<< ${var} ${test} ${val}"
!insertmacro _Test $_t
!macroend

Section testsection sec1
!insertmacro TestChk1 "TEST:StartEnv" $test1 != "outer1"

!insertmacro UAC_AsUser_GetGlobalVar $test1
!insertmacro TestChk1 "UAC_AsUser_GetGlobalVar 1.1" $test1 == "outer1"
!insertmacro UAC_AsUser_GetGlobalVar $test1
!insertmacro TestChk1 "UAC_AsUser_GetGlobalVar 1.2" $test1 == "outer1"
!insertmacro UAC_AsUser_GetGlobalVar $test2
!insertmacro TestChk1 "UAC_AsUser_GetGlobalVar 2.1" $test2 == "outer2"

!echo ----------------------------

!insertmacro UAC_AsUser_GetSection Text ${sec1} $0
!insertmacro TestChk1 "UAC_AsUser_GetSection $$0" $0 == "sec${sec1}_outerName"
!insertmacro UAC_AsUser_GetSection Text ${sec1} $1
!insertmacro TestChk1 "UAC_AsUser_GetSection $$1" $1 == "sec${sec1}_outerName"

${If} 0 = 1
	!echo ----------------------------
	!insertmacro UAC_AsUser_ExecShell "" "calc.exe" "" "" ""
	!insertmacro UAC_AsUser_ExecShell "" "calc.exe" "" "" ""
${EndIf}
SectionEnd

Function .onInit
StrCpy $test1 "outer1"
StrCpy $test2 "outer2"
SectionSetText ${sec1} sec${sec1}_outerName

uac_tryagain:
!insertmacro UAC_RunElevated
#MessageBox mb_TopMost "0=$0 1=$1 2=$2 3=$3"
${Switch} $0
${Case} 0
	${IfThen} $1 = 1 ${|} Quit ${|} ;we are the outer process, the inner process has done its work, we are done
	${IfThen} $3 <> 0 ${|} ${Break} ${|} ;we are admin, let the show go on
	${If} $1 = 3 ;RunAs completed successfully, but with a non-admin user
		MessageBox mb_IconExclamation|mb_TopMost|mb_SetForeground "This installer requires admin access, try again" /SD IDNO IDOK uac_tryagain IDNO 0
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

;we only get here in the inner instance
StrCpy $test1 "inner1"
StrCpy $test2 "inner2"
SectionSetText ${sec1} inner${sec1}
FunctionEnd

page instfiles


