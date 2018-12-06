!macro xCopyDir sourceDir DestDir
  push ${sourceDir}
  push ${DestDir}
call xCopyDir
!macroend
Function xCopyDir
  pop $4
  pop $3
  ReadEnvStr $5 COMSPEC

  FindFirst $0 $1 "$3\*"
    loop:
      StrCmp $1 "" done
      StrCmp $1 "." skip
      StrCmp $1 ".." skip

      IfFileExists "$3\$1\*.*" IsDir 0
      DetailPrint "File Source: $3\$1"
      DetailPrint "Dest: $4"
      ExecWait '$5 /C xcopy /E /C /H /K /O /Y "$3\$1" "$4\" '
      goto skip
  IsDir:
      DetailPrint "Dir Source: $3\$1"
      DetailPrint "Dest: $4"
      ExecWait '$5 /C xcopy /E /C /H /K /O /Y "$3\$1" "$4\$1\" '
  skip:
    FindNext $0 $1
      Goto loop
    done:
  FindClose $0

FunctionEnd
