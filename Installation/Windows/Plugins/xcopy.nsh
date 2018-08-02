

!macro xCopyDir sourceDir DestDir
  push ${sourceDir}
  push ${DestDir}
call xCopyDir
!macroend
Function xCopyDir
  pop $4
  pop $3

  FindFirst $0 $1 "$3\*"
    loop:
      StrCmp $1 "" done
      StrCmp $1 "." skip
      StrCmp $1 ".." skip
      IfFileExists "$3$1\*.*" IsDir 0
      Exec 'cmd /C xcopy /E /C /H /K /O /Y "$3$1" "$4\" '
      goto skip
  IsDir:
      Exec 'cmd /C xcopy /E /C /H /K /O /Y "$3$1" "$4\$1\" '
  skip:
    FindNext $0 $1
      Goto loop
    done:
  FindClose $0

FunctionEnd
