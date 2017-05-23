
Function ConditionalAddToRegistry
  Pop $0
  Pop $1
  StrCmp "$0" "" ConditionalAddToRegistry_EmptyString
    WriteRegStr SHCTX "${TRI_UNINSTALL_REG_PATH}" "$1" "$0"
    ;MessageBox MB_OK "Set Registry: '$1' to '$0'"
    DetailPrint "Set install registry entry: '$1' to '$0'"
  ConditionalAddToRegistry_EmptyString:
FunctionEnd

!Macro AddToRegistry Name Value
  push "${Name}"
  push "${Value}"
  call ConditionalAddToRegistry
!macroend
