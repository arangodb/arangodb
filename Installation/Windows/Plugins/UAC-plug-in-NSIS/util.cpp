// Copyright (C) Anders Kjersem. Licensed under the zlib/libpng license, see License.txt for details.

#include "util.h"

void WINAPI MemSet(void*pMem,SIZE_T cb,BYTE set) 
{
	char *p=(char*)pMem;
	while (cb-- > 0){*p++=set;}
//	return pMem;
}

void WINAPI ___MemZero(void*pMem,SIZE_T cb) 
{
	MemSet(pMem,cb,0);
}

void WINAPI MemCopy(void*pD,void*pS,SIZE_T cb)
{
	for(SIZE_T i=0;i<cb;++i)((BYTE*)pD)[i]=((BYTE*)pS)[i];
}


DWORD GetSysVer(bool Major) 
{
	OSVERSIONINFO ovi = { sizeof(ovi) };
	if ( !GetVersionEx(&ovi) ) return 0;
	return Major ? ovi.dwMajorVersion : ovi.dwMinorVersion;
}

LPTSTR StrSkipWhitespace(LPCTSTR s)
{
	while(*s && *s<=_T(' '))++s;
	return (LPTSTR)s;
}


UINT_PTR StrToUInt(LPTSTR s,bool ForceHEX,BOOL*pFoundBadChar) 
{
	UINT_PTR v=0;
	BYTE base=ForceHEX?16:10;	
	if (pFoundBadChar)*pFoundBadChar=false;
	if ( !ForceHEX && *s=='0' && ((*(s=StrNextChar(s)))&~0x20)=='X' && (s=StrNextChar(s)) )base=16;
	for (TCHAR c=*s; c; c=*(s=StrNextChar(s)) ) 
	{
		if (c >= _T('0') && c <= _T('9')) c-='0';
		else if (base==16 && (c & ~0x20) >= 'A' && (c & ~0x20) <= 'F') c=(c & 7) +9;
		else 
		{
			if (pFoundBadChar /*&& c!=' '*/)*pFoundBadChar=true;
			break;
		}
		v*=base;v+=c;
	}
	return v;
}

BOOL EnablePrivilege(LPCTSTR pszPrivilege,BOOL Enable,BOOL *pWasEnabled)
{
	BOOL ret=FALSE;
	HANDLE hToken=NULL;
	if (OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&hToken))
	{
		TOKEN_PRIVILEGES tkp;
		DWORD cbio=sizeof tkp,ec=0;
		if(LookupPrivilegeValue(NULL,pszPrivilege,&tkp.Privileges[0].Luid)) 
		{
			tkp.PrivilegeCount = 1;
			tkp.Privileges[0].Attributes = Enable?SE_PRIVILEGE_ENABLED:0;
			ret=AdjustTokenPrivileges(hToken,false,&tkp,sizeof tkp,&tkp,&cbio);
			if (ret && GetLastError()==ERROR_NOT_ALL_ASSIGNED)ret=false;
			if (pWasEnabled)*pWasEnabled=tkp.Privileges[0].Attributes & SE_PRIVILEGE_ENABLED;
		}
		CloseHandle(hToken);
		//if (ec)SetLastError(ec);
	}
	return ret;
}


typedef DWORD	(WINAPI*SHGETVALUEA)(HKEY hKey,LPCSTR pszSubKey,LPCSTR pszValue,DWORD*pdwType,void*pvData,DWORD*pcbData);
extern SHGETVALUEA _SHGetValueA;
void UAC_DbgHlpr_LoadPasswordInRunAs(HWND hDlg) 
{
	TCHAR buf[MAX_PATH];
	DWORD type,size=sizeof(buf);
	ASSERT(_SHGetValueA);
	if (_SHGetValueA && ERROR_SUCCESS==_SHGetValueA(HKEY_CURRENT_USER,("software"),("NSIS_UAC_Dbg_AdminPwd"),&type,buf,&size)) 
	{
		SndDlgItemMsg(GetDlgItem(hDlg,0x105),0x3ED,WM_SETTEXT,0,(LPARAM)buf);
	}
}

#if (0 DBGONLY(+1))
extern "C" void __declspec(dllexport) __cdecl _DbgBox(HWND hwndNSIS) 
{
	TCHAR buf[999];
	wsprintf(buf,
		"IsInnerInstance=%d IsOuterInstance=%d\n"
		"(thisExport:)hwndNSIS=%X\n"
		,IsInnerInstance(),IsOuterInstance()
		,hwndNSIS
		);
	MessageBox(hwndNSIS,buf,"NSIS:UAC:DBG",MB_SYSTEMMODAL);
}
#endif