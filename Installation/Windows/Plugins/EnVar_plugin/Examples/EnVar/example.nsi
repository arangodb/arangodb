Name "EnVar Example"
OutFile "EnVarExample.exe"

RequestExecutionLevel User
ShowInstDetails Show

Page InstFiles

Unicode True

Section

  ; Check for write access
  EnVar::Check "NULL" "NULL"
  Pop $0
  DetailPrint "EnVar::Check write access HKCU returned=|$0|"

  ; Set to HKLM
  EnVar::SetHKLM

  ; Check for write access
  EnVar::Check "NULL" "NULL"
  Pop $0
  DetailPrint "EnVar::Check write access HKLM returned=|$0|"

  ; Set back to HKCU
  EnVar::SetHKCU
  DetailPrint "EnVar::SetHKCU"

  ; Check for a 'temp' variable
  EnVar::Check "temp" "NULL"
  Pop $0
  DetailPrint "EnVar::Check returned=|$0|"

  ; Add a value
  EnVar::AddValue "ZTestVariable" "C:\Test"
  Pop $0
  DetailPrint "EnVar::AddValue returned=|$0|"

  EnVar::AddValue "ZTestVariable" "C:\TestJas"
  Pop $0
  DetailPrint "EnVar::AddValue returned=|$0|"

  ; Add an expanded value
  EnVar::AddValue "ZTestVariable1" "C:\Test"
  Pop $0
  DetailPrint "EnVar::AddValue returned=|$0|"

  EnVar::AddValueEx "ZTestVariable1" "C:\Test"
  Pop $0
  DetailPrint "EnVar::AddValue returned=|$0|"

  EnVar::AddValueEx "ZTestVariable1" "C:\TestVariable"
  Pop $0
  DetailPrint "EnVar::AddValue returned=|$0|"

  ; Update the installer environment so that new
  ; paths are available to the installer
  EnVar::Update "HKCU" "ZTestVariable"
  Pop $0
  DetailPrint "EnVar::Update returned=|$0|"

  EnVar::Update "" "ZTestVariable1"
  Pop $0
  DetailPrint "EnVar::Update returned=|$0|"

  ; Delete a value from a variable
  EnVar::DeleteValue "ZTestVariable1" "C:\Test"
  Pop $0
  DetailPrint "EnVar::DeleteValue returned=|$0|"

  EnVar::DeleteValue "ZTestVariable1" "C:\Test"
  Pop $0
  DetailPrint "EnVar::DeleteValue returned=|$0|"

  EnVar::DeleteValue "ZTestVariable1" "C:\TestJason"
  Pop $0
  DetailPrint "EnVar::DeleteValue returned=|$0|"

  ; Delete a variable
  EnVar::Delete "ZTestVariable"
  Pop $0
  DetailPrint "EnVar::Delete returned=|$0|"

  ; Try deleting "path", this should give an error
  EnVar::Delete "path"
  Pop $0
  DetailPrint "EnVar::Delete returned=|$0|"

SectionEnd