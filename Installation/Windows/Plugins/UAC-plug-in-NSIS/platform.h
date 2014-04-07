#pragma once
/** /#define UNICODE //*/
#ifdef UNICODE
#	define _UNICODE
#endif

#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ShellAPI.h>
#include <TChar.h>
#include <WindowsX.h>

#define FORCEINLINE __forceinline

#ifndef ARRAYSIZE
#	define ARRAYSIZE(___c) ( sizeof(___c)/sizeof(___c[0]) )
#endif


#ifndef SEE_MASK_NOZONECHECKS
#define SEE_MASK_NOZONECHECKS 0x00800000
#endif

#ifndef MSGFLT_ADD
#define MSGFLT_ADD 1
#endif


#if (!defined(NTDDI_VISTA) && !defined(NTDDI_LONGHORN)) || defined(BUILD_OLDSDK)
#define TokenElevationType  ((TOKEN_INFORMATION_CLASS)(TokenOrigin+1))
#define TokenElevation      ((TOKEN_INFORMATION_CLASS)(TokenOrigin+3))
#define TokenIntegrityLevel ((TOKEN_INFORMATION_CLASS)(TokenOrigin+8))
enum TOKEN_ELEVATION_TYPE { 
	TokenElevationTypeDefault = 1, 
	TokenElevationTypeFull, 
	TokenElevationTypeLimited 
};
typedef struct _TOKEN_ELEVATION {
  DWORD TokenIsElevated;
} TOKEN_ELEVATION, *PTOKEN_ELEVATION;
typedef struct _TOKEN_MANDATORY_LABEL {
  SID_AND_ATTRIBUTES Label;
} TOKEN_MANDATORY_LABEL, *PTOKEN_MANDATORY_LABEL;
#endif




