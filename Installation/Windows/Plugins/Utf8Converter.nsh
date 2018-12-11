

/**
 *  Two macroses to convert ANSI text to UTF-8 and conversely.
 *  This file is FREE FOR USE.
 *
 *  Written by Shurmialiou Vadzim at 04/2010
 *  inshae@gmail.com
 */
 
!ifndef ___ANSITOUTF8_NSH___
!define ___ANSITOUTF8_NSH___
 
!include "LogicLib.nsh"
 
/**
 * Convert ANSI text to UTF-8 text in the installer or uninstaller.
 *
 * Usage:
 * StrCpy $0 "Belarussian text: ÐŸÑ€Ñ‹Ð²iÑ‚Ð°Ð½Ð½Ðµ, Ð¡Ð²ÐµÑ‚!"
 * ${AnsiToUtf8} $0 $1
 * DetailPrint "'$1' == 'Belarussian text: Ð ÑŸÐ¡Ð‚Ð¡â€¹Ð Ð†iÐ¡â€šÐ Â°Ð Ð…Ð Ð…Ð Âµ, Ð ÐŽÐ Ð†Ð ÂµÐ¡â€š!'"   ;UTF-8 text
 */
!define AnsiToUtf8 '!insertmacro AnsiToUtf8Macro'
!macro AnsiToUtf8Macro SOURCE_STRING OUTPUT_STRING
	Push "${SOURCE_STRING}"
	Push 0		;ANSI  codepage
	Push 65001	;UTF-8 codepage
	!insertmacro ConvertOverUnicode
	Pop "${OUTPUT_STRING}"
!macroend
 
/**
 * Convert UTF-8 text to ANSI text in the installer or uninstaller.
 *
 * Usage:
 * StrCpy $0 "Belarussian text: Ð ÑŸÐ¡Ð‚Ð¡â€¹Ð Ð†iÐ¡â€šÐ Â°Ð Ð…Ð Ð…Ð Âµ, Ð ÐŽÐ Ð†Ð ÂµÐ¡â€š!"
 * ${Utf8ToAnsi} $0 $1
 * DetailPrint "'$1' == 'Belarussian text: ÐŸÑ€Ñ‹Ð²iÑ‚Ð°Ð½Ð½Ðµ, Ð¡Ð²ÐµÑ‚!'"   ;UTF-8 text
 */
!define Utf8ToAnsi '!insertmacro Utf8ToAnsiMacro'
!macro Utf8ToAnsiMacro SOURCE_STRING OUTPUT_STRING
	Push "${SOURCE_STRING}"
	Push 65001	;UTF-8 codepage
	Push 0		;ANSI  codepage
	!insertmacro ConvertOverUnicode
	Pop "${OUTPUT_STRING}"
!macroend
 
!macro ConvertOverUnicode
 
	Exch $0	;Result codepage
	Exch
	Exch $1	;Source codepage
	Exch
	Exch 2
	Exch $2	;Source text
 
	Push $3	;Result text
	Push $4
	Push $5	;unicode text
	Push $6
	Push $7
	Push $8
 
	StrCpy $3 ""
 
	;From ANSI to Unicode and then from unicode to UTF-8
 
	${If} $2 != ""
 
		;long bufSize = ::MultiByteToWideChar(CP_ACP, 0, cp1251str, -1, 0, 0);
		System::Call /NOUNLOAD "kernel32::MultiByteToWideChar(i r1, i 0, t r2, i -1, i 0, i 0) i .r4"
 
		${If} $4 > 0
 
			IntOp $4 $4 * 2	;2 bytes by one unicode-symbol
			System::Alloc /NOUNLOAD $4
			Pop $5
 
			; ::MultiByteToWideChar(CP_ACP, 0, cp1251str, -1, unicodeStr, bufSize)
			System::Call /NOUNLOAD "kernel32::MultiByteToWideChar(i r1, i 0, t r2, i -1, i r5, i r4) i .r6"
			${If} $6 > 0
 
				;bufSize = ::WideCharToMultiByte(CP_UTF8, 0, unicodeStr, -1, 0, 0, 0, 0);
				System::Call /NOUNLOAD "kernel32::WideCharToMultiByte(i r0, i 0, i r5, i -1, i 0, i 0, i 0, i 0) i .r6"
 
				; ::WideCharToMultiByte(CP_UTF8, 0, unicodeStr, -1, utf8Str, bufSize, 0, 0)
				System::Call /NOUNLOAD "kernel32::WideCharToMultiByte(i r0, i 0, i r5, i -1, t .r7, i r6, i 0, i 0) i .r8"
 
				${If} $8 > 0
					;Save result to $3
					StrCpy $3 $7
				${EndIf}
 
			${EndIf}
 
			;Free buffer from unicode string
			System::Free $5
 
		${EndIf}
 
	${EndIf}
 
	Pop  $8
	Pop  $7
	Pop  $6
	Pop  $5
	Pop  $4
	Exch
	Pop  $2
	Exch
	Pop  $1
	Exch
	Pop  $0
	Exch  $3
 
!macroend
 
!endif	;___ANSITOUTF8_NSH___
