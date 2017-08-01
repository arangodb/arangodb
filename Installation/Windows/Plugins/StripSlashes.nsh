; https://stackoverflow.com/questions/19683976/best-way-to-remove-trailing-character-from-a-string-for-example-trailing-slash

Function StripBackslash
Exch $0
Push $1
StrCpy $1 $0 "" -1
StrCmp $1 "\" 0 +2
StrCpy $0 $0 -1
Pop $1
Exch $0
FunctionEnd

!macro StripBackslash PathToTrim
  push $${PathToTrim}
  call StripBackslash
  pop $${PathToTrim}
!macroend

Function StripSlash
Exch $0
Push $1
StrCpy $1 $0 "" -1
StrCmp $1 "/" 0 +2
StrCpy $0 $0 -1
Pop $1
Exch $0
FunctionEnd

!macro StripSlash PathToTrim
  push $${PathToTrim}
  call StripSlash
  pop $${PathToTrim}
!macroend
