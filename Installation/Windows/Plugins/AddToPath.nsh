!include "StrStr.nsh"
Var PathToAdd

;--------------------------------
; path functions
!include "winmessages.nsh"
!define NT_current_env 'HKCU "Environment"'
!define NT_all_env     'HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"'

; AddToPath - Adds the given dir to the search path.
;        Input - head of the stack
;        Note - Win9x systems requires reboot


Function Trim ; Added by Pelaca
  Exch $R1
  Push $R2
  Loop:
    StrCpy $R2 "$R1" 1 -1
    StrCmp "$R2" " " RTrim
    StrCmp "$R2" "$\n" RTrim
    StrCmp "$R2" "$\r" RTrim
    StrCmp "$R2" ";" RTrim
    GoTo Done
  RTrim:
    StrCpy $R1 "$R1" -1
    Goto Loop
  Done:
    Pop $R2
    Exch $R1
FunctionEnd


!macro AddToPath PathToAdd forAllUsers
  push ${PathToAdd}
  push ${forAllUsers}
  call AddToPath
!macroend

Var ADP_forAllUsers

Function AddToPath
  Exch $0  ; Path do add to path
  Exch $1
  StrCpy $ADP_forAllUsers $1
  
  Push $1
  Push $2
  Push $3

  ; don't add if the path doesn't exist
  IfFileExists "$0\*.*" "" AddToPath_done

  ReadEnvStr $1 PATH
  ; if the path is too long for a NSIS variable NSIS will return a 0
  ; length string.  If we find that, then warn and skip any path
  ; modification as it will trash the existing path.
  StrLen $2 $1
  IntCmp $2 0 CheckPathLength_ShowPathWarning CheckPathLength_Done CheckPathLength_Done
  CheckPathLength_ShowPathWarning:
    Messagebox MB_OK|MB_ICONEXCLAMATION "Warning! Your PATH environment is already too long; the installer is unable to add ArangoDB to it."
    Goto AddToPath_done
  CheckPathLength_Done:
    Push "$1;"
    Push "$0;"
    Call StrStr
    Pop $2
    StrCmp $2 "" "" AddToPath_done
    Push "$1;"
    Push "$0\;"
    Call StrStr
    Pop $2
    StrCmp $2 "" "" AddToPath_done
    GetFullPathName /SHORT $3 $0
    Push "$1;"
    Push "$3;"
    Call StrStr
    Pop $2
    StrCmp $2 "" "" AddToPath_done
    Push "$1;"
    Push "$3\;"
    Call StrStr
    Pop $2
    StrCmp $2 "" "" AddToPath_done

    StrCmp $ADP_forAllUsers "1" ReadAllKey
    ReadRegStr $1 ${NT_current_env} "PATH"
    Goto DoTrim
  ReadAllKey:
    ReadRegStr $1 ${NT_all_env} "PATH"
  DoTrim:
    StrCmp $1 "" AddToPath_NTdoIt
    Push $1
    Call Trim
    Pop $1
    StrCpy $0 "$1;$0"
  AddToPath_NTdoIt:
    StrCmp $ADP_forAllUsers "1" WriteAllKey
    WriteRegExpandStr ${NT_current_env} "PATH" $0
    Goto DoSend
  WriteAllKey:
    WriteRegExpandStr ${NT_all_env} "PATH" $0
  DoSend:
    SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  AddToPath_done:
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd


!macro un.RemoveFromPath forAllUsers
  push ${PathToAdd}
  push ${forAllUsers}
  call un.RemoveFromPath
!macroend

; RemoveFromPath - Remove a given dir from the path
;     Input: head of the stack

Function un.RemoveFromPath
  Exch $0
  Exch $1
  StrCpy $ADP_forAllUsers $1

  Push $1
  Push $2
  Push $3
  Push $4
  Push $5
  Push $6

  IntFmt $6 "%c" 26 ; DOS EOF

  StrCmp $ADP_forAllUsers "1" unReadAllKey
  ReadRegStr $1 ${NT_current_env} "PATH"
  Goto unDoTrim
  unReadAllKey:
    ReadRegStr $1 ${NT_all_env} "PATH"
  unDoTrim:
    StrCpy $5 $1 1 -1 ; copy last char
    StrCmp $5 ";" +2 ; if last char != ;
    StrCpy $1 "$1;" ; append ;
    Push $1
    Push "$0;"
    Call un.StrStr ; Find `$0;` in $1
    Pop $2 ; pos of our dir
    StrCmp $2 "" unRemoveFromPath_done
    ; else, it is in path
    ; $0 - path to add
    ; $1 - path var
    StrLen $3 "$0;"
    StrLen $4 $2
    StrCpy $5 $1 -$4 ; $5 is now the part before the path to remove 
    StrCpy $6 $2 "" $3 ; $6 is now the part after the path to remove
    StrCpy $3 $5$6

    StrCpy $5 $3 1 -1 ; copy last char
    StrCmp $5 ";" 0 +2 ; if last char == ;
    StrCpy $3 $3 -1 ; remove last char

    StrCmp $ADP_forAllUsers "1" unWriteAllKey
    WriteRegExpandStr ${NT_current_env} "PATH" $3
    Goto unDoSend
  unWriteAllKey:
    WriteRegExpandStr ${NT_all_env} "PATH" $3
  unDoSend:
    SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  unRemoveFromPath_done:
    Pop $6
    Pop $5
    Pop $4
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd

