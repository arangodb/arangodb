
!define StrTrimNewLines "!insertmacro StrTrimNewLines"
 
!macro StrTrimNewLines ResultVar String
  Push "${String}"
  Call StrTrimNewLines
  Pop "${ResultVar}"
!macroend
 
Function StrTrimNewLines
/*After this point:
  ------------------------------------------
  $R0 = String (input)
  $R1 = TrimCounter (temp)
  $R2 = Temp (temp)*/
 
  ;Get input from user
  Exch $R0
  Push $R1
  Push $R2
 
  ;Initialize trim counter
  StrCpy $R1 0
 
  loop:
  ;Subtract to get "String"'s last characters
  IntOp $R1 $R1 - 1
 
  ;Verify if they are either $\r or $\n
  StrCpy $R2 $R0 1 $R1
  ${If} $R2 == `$\r`
  ${OrIf} $R2 == `$\n`
    Goto loop
  ${EndIf}
 
  ;Trim characters (if needed)
  IntOp $R1 $R1 + 1
  ${If} $R1 < 0
    StrCpy $R0 $R0 $R1
  ${EndIf}
 
/*After this point:
  ------------------------------------------
  $R0 = ResultVar (output)*/
 
  ;Return output to user
  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd

Function SplitFirstStrPart
  Exch $R0
  Exch
  Exch $R1
  Push $R2
  Push $R3
  StrCpy $R3 $R1
  StrLen $R1 $R0
  IntOp $R1 $R1 + 1
  loop:
    IntOp $R1 $R1 - 1
    StrCpy $R2 $R0 1 -$R1
    StrCmp $R1 0 exit0
    StrCmp $R2 $R3 exit1 loop
  exit0:
  StrCpy $R1 ""
  Goto exit2
  exit1:
    IntOp $R1 $R1 - 1
    StrCmp $R1 0 0 +3
     StrCpy $R2 ""
     Goto +2
    StrCpy $R2 $R0 "" -$R1
    IntOp $R1 $R1 + 1
    StrCpy $R0 $R0 -$R1
    StrCpy $R1 $R2
  exit2:
  Pop $R3
  Pop $R2
  Exch $R1 ;rest
  Exch
  Exch $R0 ;first
FunctionEnd

Function ReadINIFileKeys
  Exch $R0 ;INI file to write
  Exch
  Exch $R1 ;INI file to read
  Push $R2
  Push $R3
  Push $R4 ;uni var
  Push $R5 ;uni var
  Push $R6 ;last INI section

  ClearErrors

  FileOpen $R2 $R1 r

  Loop:
    FileRead $R2 $R3   ;get next line into R3
    IfErrors Exit

    Push $R3
    Call StrTrimNewLines
    Pop $R3

    StrCmp $R3 "" Loop   ;if blank line, skip

    StrCpy $R4 $R3 1   ;get first char into R4
    StrCmp $R4 ";" Loop   ;check it for semicolon and skip line if so(ini comment)

    StrCpy $R4 $R3 "" -1   ;get last char of line into R4
    StrCmp $R4 "]" 0 +6     ;if last char is ], parse section name, else jump to parse key/value
    StrCpy $R6 $R3 -1   ;get all except last char
    StrLen $R4 $R6     ;get str length
    IntOp $R4 $R4 - 1    ;subtract one from length
    StrCpy $R6 $R6 "" -$R4   ;copy all but first char to trim leading [, placing the section name in R6
    Goto Loop

    Push "="  ;push delimiting char
    Push $R3
    Call SplitFirstStrPart
    Pop $R4
    Pop $R5

    WriteINIStr $R0 $R6 $R4 $R5
    IfErrors 0 Loop
    MessageBox MB_OK "Ini file write errored while writing $R0 $R6 $R4 $R5"

    Goto Loop
  Exit:

    FileClose $R2

    Pop $R6
    Pop $R5
    Pop $R4
    Pop $R3
    Pop $R2
    Pop $R1
    Pop $R0
FunctionEnd
