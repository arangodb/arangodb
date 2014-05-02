// Copyright (C) Anders Kjersem. Licensed under the zlib/libpng license, see License.txt for details.

#pragma once
/** /#define BUILD_DBGRELEASE // Include simple debug output in release version */
/**/#define FEAT_CUSTOMRUNASDLG // Include custom runas dialog, used for broken Vista config */
/**/#define FEAT_CUSTOMRUNASDLG_TRANSLATE //*/
/**/#define FEAT_MSRUNASDLGMODHACK // Tweak RunAs dialog on NT5 */
/**/#define FEAT_AUTOPAGEJUMP //*/

#define UAC_HACK_FGNDWND2011 //aka tiny splash screen
//#define UAC_HACK_FGNDWND2010
//#define UAC_HACK_INNERPARENT2010
//#define UAC_HACK_ONINIT


#include "platform.h"
#include "NSIS_CUSTOMPISDK.h"
#include "util.h"

#if (defined(_MSC_VER) && !defined(_DEBUG))
	#pragma comment(linker,"/opt:nowin98")
	#pragma comment(linker,"/ignore:4078")
	#pragma comment(linker,"/merge:.rdata=.text")
#endif

#if 0
#	define BUILDRLSCANLEAK(x)
#else
#	define BUILDRLSCANLEAK(x) x
#endif

extern HINSTANCE g_hInst;

extern UINT_PTR StrToUInt(LPTSTR s,bool ForceHEX=false,BOOL*pFoundBadChar=0);
#ifdef FEAT_CUSTOMRUNASDLG
extern DWORD WINAPI MyRunAs(HINSTANCE hInstDll,SHELLEXECUTEINFO&sei); 
#endif


typedef BOOL	(WINAPI*ALLOWSETFOREGROUNDWINDOW)(DWORD dwProcessId);
typedef BOOL	(WINAPI*OPENPROCESSTOKEN)(HANDLE ProcessHandle,DWORD DesiredAccess,PHANDLE TokenHandle);
typedef BOOL	(WINAPI*OPENTHREADTOKEN)(HANDLE ThreadHandle,DWORD DesiredAccess,BOOL OpenAsSelf,PHANDLE TokenHandle);
typedef BOOL	(WINAPI*GETTOKENINFORMATION)(HANDLE hToken,TOKEN_INFORMATION_CLASS TokInfoClass,LPVOID TokInfo,DWORD TokInfoLenh,PDWORD RetLen);
typedef BOOL	(WINAPI*ALLOCATEANDINITIALIZESID)(PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,BYTE nSubAuthorityCount,DWORD sa0,DWORD sa1,DWORD sa2,DWORD sa3,DWORD sa4,DWORD sa5,DWORD sa6,DWORD sa7,PSID*pSid);
typedef PVOID	(WINAPI*FREESID)(PSID pSid);
typedef BOOL	(WINAPI*EQUALSID)(PSID pSid1,PSID pSid2);
typedef BOOL	(WINAPI*CHECKTOKENMEMBERSHIP)(HANDLE TokenHandle,PSID SidToCheck,PBOOL IsMember);
typedef BOOL	(WINAPI*CHANGEWINDOWMESSAGEFILTER)(UINT message,DWORD dwFlag);
#ifdef FEAT_CUSTOMRUNASDLG
typedef BOOL	(WINAPI*GETUSERNAME)(LPTSTR lpBuffer,LPDWORD nSize);
typedef BOOL	(WINAPI*CREATEPROCESSWITHLOGONW)(LPCWSTR lpUsername,LPCWSTR lpDomain,LPCWSTR lpPassword,DWORD dwLogonFlags,LPCWSTR lpApplicationName,LPWSTR lpCommandLine,DWORD dwCreationFlags,LPVOID pEnv,LPCWSTR WorkDir,LPSTARTUPINFOW pSI,LPPROCESS_INFORMATION pPI);
#define SECURITY_WIN32
#include <Security.h> //NameSamCompatible
typedef BOOLEAN	(WINAPI*GETUSERNAMEEX)(EXTENDED_NAME_FORMAT NameFormat,LPTSTR lpNameBuffer,PULONG nSize);
typedef DWORD	(WINAPI*SHGETVALUEA)(HKEY hKey,LPCSTR pszSubKey,LPCSTR pszValue,DWORD*pdwType,void*pvData,DWORD*pcbData);
#endif


#define UAC_SYNCREGISTERS 0x1
#define UAC_SYNCSTACK     0x2
#define UAC_SYNCOUTDIR    0x4
#define UAC_SYNCINSTDIR   0x8
#define UAC_CLEARERRFLAG  0x10


enum OUTERWNDMSG {
	OWM_INITINNER=WM_APP,
	OWM_ISREADYFORINIT,
	OWM_PERFORMWINDOWSWITCH,
	OWM_GETOUTERSTATE,
	OWM_SETOUTERSTATE,
#ifdef UAC_HACK_INNERPARENT2010
	OWM_PARENTIFY,
#endif
#ifdef UAC_HACK_FGNDWND2011
	OWM_HIDESPLASH,
#endif
};

enum INNERWNDMSG {
	IWM_HELPWITHWINDOWSWITCH=WM_APP,
};

enum GETOUTERSTATEITEM {
	GOSI_HWNDMAINWND,
	GOSI_PAGEJUMP,
#ifdef UAC_HACK_ASFW
	GOSI_PID,
#endif
};

enum SETOUTERSTATEITEM {
	SOSI_PROCESSDUPHANDLE=1337,
	SOSI_ELEVATIONPAGE_GUIINITERRORCODE,
};

enum OPID {
//	_OPID_INVALID_,
	OPID_SYNCREG,
	OPID_CALLCODESEGMENT,
//	OPID_SYNCNSISVAR,
//	OPID_GETOUTERSTATE,
	OPID_OUTERMUSTRETRY,
	OPID_NOP,
};

#define OPDIR_I2O 0
#define OPDIR_O2I 1

typedef struct {
	BYTE Op;
	BYTE Direction;
} OP_HDR;

typedef struct {
	OP_HDR OpHdr;
	BYTE RegOffset;
	BYTE Count;
} OP_SYNCREG;

#define OPSNV_INVALIDVARID (INT_PTR)-1
typedef struct {
	OP_HDR OpHdr;
	INT_PTR var1;
	INT_PTR var2;
} OP_SYNCNSISVAR;

#define OPCCSF_SETOUTPATH 0x1
typedef struct {
	OP_HDR OpHdr;
	int Pos;
	BYTE Flags;
} OP_CALLCODESEGMENT;


#define RUNMODE_INIT   0
#define RUNMODE_LEGACY 1
#define RUNMODE_OUTER  2
#define RUNMODE_INNER  3

typedef struct {
	HANDLE hInnerProcess;
	HANDLE hOuterThread;
#ifdef UAC_HACK_FGNDWND2011
	HWND hwndSplash;
#endif
#ifdef FEAT_AUTOPAGEJUMP
	UINT ecInner_InitSharedData;
#endif
} UAC_GLOBALS_OUTER;

typedef struct {
} UAC_GLOBALS_INNER;

typedef struct {
	BYTE* pShared;
	HANDLE PerformOp;
	HANDLE CompletedOp;
	HANDLE hSharedMap;
	HWND hwndOuter;
	NSISCH*NSISVars;
	UINT NSIScchStr;
	union {
		UAC_GLOBALS_OUTER outer;
		UAC_GLOBALS_INNER inner;
	};
	BYTE RunMode;
#ifdef FEAT_AUTOPAGEJUMP
	WORD JumpPageOffset;
	WNDPROC OrgMainWndProc;
	HWND hwndNSIS;
#endif
} UAC_GLOBALS;