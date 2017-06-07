!include "LogicLib.nsh"
!define TRI_SVC_NAME 'ArangoDB'

;--------------------------------

Function WaitForServiceUp
  DetailPrint "starting ArangoDB Service..."
  Push 0
  Pop $R0
  try_again:
    SimpleSC::ServiceIsRunning '${TRI_SVC_NAME}'
    Pop $0 ; returns an errorcode (<>0) otherwise success (0)
    Pop $1 ; returns 1 (service is running) - returns 0 (service is not running)
    ${If} $1 == 1
      ;MessageBox MB_OK "Service running : $1 "
      ; ok, running now.
      push 0
      Return
    ${EndIf}
    Sleep 1000
    ${If} $R0 == 40
      SetErrorLevel 3
      push 1
      IfSilent 0 openStartPopup
      Quit
     openStartPopup:
      MessageBox MB_OK "Waited 40 seconds for ArangoDB to come up; Please look at the Windows Eventlog for eventual errors!"
      Return
    ${EndIf}
    IntOp $R0 $R0 + 1
    Goto try_again
FunctionEnd

;--------------------------------
!macro WaitForServiceDown un
Function ${un}WaitForServiceDown
  DetailPrint "stopping ArangoDB Service..."
  Push 0
  Pop $R0
  try_again:
    SimpleSC::ServiceIsRunning '${TRI_SVC_NAME}'
    Pop $0 ; returns an errorcode (<>0) otherwise success (0)
    Pop $1 ; returns 1 (service is running) - returns 0 (service is not running)
    ${If} $1 == 0
      ;MessageBox MB_OK "Service running : $1 "
      ; ok, stopped by now.
      push 0
      Return
    ${EndIf}
    Sleep 1000
    ${If} $R0 == 40
      SetErrorLevel 3
      Push 1
      IfSilent 0 openStopPopup
      Quit
     openStopPopup:
      MessageBox MB_OK "Waited 40 seconds for the ArangoDB Service to shutdown; you may need to remove files by hand"
      Return
    ${EndIf}
    IntOp $R0 $R0 + 1
    Goto try_again
  Sleep 1000
FunctionEnd
!macroend

!insertmacro WaitForServiceDown ""
!insertmacro WaitForServiceDown "un."


;--------------------------------
; by Anders http://forums.winamp.com/member.php?u=70852
!macro QueryServiceStatus un
Function ${un}QueryServiceStatus
  StrCpy $0 0
  push $0
  push $0
  push $0
  !define /ifndef SERVICE_QUERY_STATUS 4
  System::Call 'ADVAPI32::OpenSCManager(p0, p0, i1)p.r1'
  ${If} $1 P<> 0
    System::Call 'ADVAPI32::OpenService(pr1, t"${TRI_SVC_NAME}", i${SERVICE_QUERY_STATUS})p.r2'
    System::Call 'ADVAPI32::CloseServiceHandle(pr1)'
    ${If} $2 P<> 0
      System::Call 'ADVAPI32::QueryServiceStatus(pr2, @r3)i.r0' ; Note: NSIS 3+ syntax to "allocate" a SERVICE_STATUS
      
      ${If} $0 <> 0
        System::Call '*$3(i,i.r4,i,i.r5,i.r6)'
        pop $0
        pop $0
        pop $0
        push $4
        push $5
        push $6
        DetailPrint "CurrentState=$4 Win32ExitCode=$5 ServiceSpecificExitCode=$6"
      ${EndIf}
      System::Call 'ADVAPI32::CloseServiceHandle(pr2)'
    ${EndIf}
  ${EndIf}
FunctionEnd
!macroend

!insertmacro QueryServiceStatus ""
!insertmacro QueryServiceStatus "un."

