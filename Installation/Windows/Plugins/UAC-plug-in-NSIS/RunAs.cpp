// Copyright (C) Anders Kjersem. Licensed under the zlib/libpng license, see License.txt for details.

/*
If UAC is disabled, the runas verb is broken (Vista RTM) so when running as LUA there is no way to elevate so 
we provide our own dialog.
*/

#include "uac.h"

extern GETUSERNAMEEX			_GetUserNameEx;
extern CREATEPROCESSWITHLOGONW	_CreateProcessWithLogonW;


#include <Lmcons.h>//UNLEN && GNLEN && PWLEN
#include <WindowsX.h>
#include "resource.h"
using namespace NSIS;

#define ERRAPP_TRYAGAIN (0x20000000|1)
#define MYMAX_DOMAIN (2+max(GNLEN,MAX_COMPUTERNAME_LENGTH)+1)


static LPCTSTR g_RunAsDlgTitle=_T("Run as");
static LPCTSTR g_RunAsCurrUsrFmt=_T("&Current user (%s)");//Max 50 chars!
static LPCSTR g_RunAsHelpText=("You may not have the necessary permissions to use all the features of the program you are about to run. You may run this program as a different user or continue to run the program as the current user.");
static LPCSTR g_RunAsSpecHelp=("Run the program as the &following user:");


typedef struct {
	SHELLEXECUTEINFO*pSEI;
	DWORD LastErr;
	bool AsSelf;
} RUNASDLGDATA;


void AsciiToUnicode(const char*asciiText,WCHAR*buf) 
{
	do
	{
		*buf++=*asciiText;
		if (!*asciiText++)break;
	}
	while(1);
}

#if defined(_UNICODE) && 1
void SetDlgTCharItemAsciiText(HWND hDlg,UINT id,const char*asciiText) 
{
	TCHAR buf[MAX_PATH];
	AsciiToUnicode(asciiText,buf);//this is a hack so we don't have to call MultiByteToWideChar, strings stored as ascii to save space
	MySetDlgItemText(hDlg,id,buf);
}
#else
#define SetDlgTCharItemAsciiText MySetDlgItemText
#endif


void MyRunAsFmtCurrUserRadio(HWND hDlg,LPCTSTR Fmt) {
	TCHAR bufFullName[MYMAX_DOMAIN+UNLEN+1];
	TCHAR buf[50+MYMAX_DOMAIN+UNLEN+1];
	*bufFullName=0;
	ULONG cch;
	if ((!_GetUserNameEx || !_GetUserNameEx(NameSamCompatible,bufFullName,&(cch=ARRAYSIZE(bufFullName)))) && 
		!GetUserName(bufFullName,&(cch=ARRAYSIZE(bufFullName))) ) {
		*bufFullName=0;
	}
	wsprintf(buf,Fmt,*bufFullName?bufFullName:_T("?"));
	MySetDlgItemText(hDlg,IDC_RUNASCURR,buf);

	// default the "User name:" to Administrator from shell32
	if (LoadString(LoadLibraryA(("SHELL32")),21763, bufFullName,ARRAYSIZE(bufFullName)) > 0) {
		MySetDlgItemText(hDlg,IDC_USERNAME,bufFullName);
	}
}

#ifdef FEAT_CUSTOMRUNASDLG_TRANSLATE
void MyRunAsTranslateDlgString(LPCTSTR StrID,LPTSTR Ini,HWND hDlg,INT_PTR DlgItemId,int special=0) {
	TCHAR buf[MAX_PATH*2];
	DWORD len=GetPrivateProfileString(_T("MyRunAsStrings"),StrID,0,buf,ARRAYSIZE(buf),Ini);
	if (len) {
		if (IDC_RUNASCURR==special)
			MyRunAsFmtCurrUserRadio(hDlg,buf);
		else
			(DlgItemId==-1) ? SendMessage(hDlg,WM_SETTEXT,0,(LPARAM)buf) : MySetDlgItemText(hDlg,DlgItemId,buf);
	}
}

#if 0
UINT MyGetPrivateProfileInt(LPCTSTR Section,LPCTSTR Name,UINT defval,LPCTSTR inifile) 
{
	TCHAR buf[20];
	*buf=NULL;
	GetPrivateProfileString(Section,Name,buf,buf,ARRAYSIZE(buf),inifile);
	BOOL badchar;
	UINT ret=StrToUInt(buf,false,&badchar);
	return (badchar || !*buf) ? defval : ret;
}
#else
#define MyGetPrivateProfileInt GetPrivateProfileInt
#endif

void MyRunAsTranslateDlg(HWND hDlg) {
	DWORD len;
	TCHAR buf[MAX_PATH*2];
	HMODULE hDll=g_hInst;ASSERT(hDll);
	if ( (len=GetModuleFileName(hDll,buf,ARRAYSIZE(buf))) <1)return;
	buf[len-3]=0;
	lstrcat(buf,_T("lng"));
	MyRunAsTranslateDlgString(_T("DlgTitle"),buf,hDlg,-1);
	MyRunAsTranslateDlgString(_T("HelpText"),buf,hDlg,IDC_HELPTEXT);
	MyRunAsTranslateDlgString(_T("OptCurrUser"),buf,hDlg,IDC_RUNASCURR,IDC_RUNASCURR);
	MyRunAsTranslateDlgString(_T("OptOtherUser"),buf,hDlg,IDC_RUNASSPEC);
	MyRunAsTranslateDlgString(_T("Username"),buf,hDlg,IDC_LBLUSER);
	MyRunAsTranslateDlgString(_T("Pwd"),buf,hDlg,IDC_LBLPWD);
	MyRunAsTranslateDlgString(_T("OK"),buf,hDlg,IDOK);
	MyRunAsTranslateDlgString(_T("Cancel"),buf,hDlg,IDCANCEL);
	HWND h=GetDlgItem(hDlg,IDC_RUNASCURR);
	if (MyGetPrivateProfileInt(_T("MyRunAsCfg"),_T("DisableCurrUserOpt"),false,buf))EnableWindow(h,false);
	if (MyGetPrivateProfileInt(_T("MyRunAsCfg"),_T("HideCurrUserOpt"),false,buf))ShowWindow(h,false);
}
#endif

bool ErrorIsLogonError(DWORD err) {
	switch (err) {
	case ERROR_LOGON_FAILURE:
	case ERROR_ACCOUNT_RESTRICTION:
	case ERROR_INVALID_LOGON_HOURS:
	case ERROR_INVALID_WORKSTATION:
	case ERROR_PASSWORD_EXPIRED:
	case ERROR_ACCOUNT_DISABLED:
	case ERROR_NONE_MAPPED:
	case ERROR_NO_SUCH_USER:
	case ERROR_INVALID_ACCOUNT_NAME:
		return true;
	}
	return false;
}



void VerifyOKBtn(HWND hDlg,RUNASDLGDATA*pRADD) {
	const bool HasText=pRADD?(pRADD->AsSelf?true:SndDlgItemMsg(hDlg,IDC_USERNAME,WM_GETTEXTLENGTH)>0):false;
	EnableWindow(GetDlgItem(hDlg,IDOK),HasText);
}

void SetDlgState(HWND hDlg,bool AsSelf,RUNASDLGDATA*pRADD) {
	if (pRADD)pRADD->AsSelf=AsSelf;
	SndDlgItemMsg(hDlg,IDC_RUNASCURR,BM_SETCHECK,AsSelf?BST_CHECKED:BST_UNCHECKED);
	SndDlgItemMsg(hDlg,IDC_RUNASSPEC,BM_SETCHECK,!AsSelf?BST_CHECKED:BST_UNCHECKED);
	int ids[]={IDC_USERNAME,IDC_PASSWORD,IDC_LBLUSER,IDC_LBLPWD};
	for (int i=0; i<ARRAYSIZE(ids);++i)EnableWindow(GetDlgItem(hDlg,ids[i]),!AsSelf);
	VerifyOKBtn(hDlg,pRADD);
}

INT_PTR CALLBACK MyRunAsDlgProc(HWND hwnd,UINT uMsg,WPARAM wp,LPARAM lp) {
	RUNASDLGDATA*pRADD=(RUNASDLGDATA*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
	switch(uMsg) {
	//case WM_DESTROY:
	//	break;
	case WM_CLOSE:
		return DestroyWindow(hwnd);
	case WM_INITDIALOG:
		{	
			pRADD=(RUNASDLGDATA*)lp;ASSERT(pRADD);
			SetWindowLongPtr(hwnd,GWLP_USERDATA,lp);
			Edit_LimitText(GetDlgItem(hwnd,IDC_USERNAME),UNLEN+1+MYMAX_DOMAIN); //room for "foo@BAR" or "BAR\foo"
			Edit_LimitText(GetDlgItem(hwnd,IDC_PASSWORD),PWLEN);
			const HINSTANCE hSh32=LoadLibraryA(("SHELL32"));
			const HICON hIco=(HICON)LoadImage(hSh32,MAKEINTRESOURCE(194),IMAGE_ICON,32,32,LR_SHARED);
			SndDlgItemMsg(hwnd,IDC_SHICON,STM_SETICON,(WPARAM)hIco);
			SendMessage(hwnd,WM_SETTEXT,0,(LPARAM)g_RunAsDlgTitle);
			SetDlgTCharItemAsciiText(hwnd,IDC_HELPTEXT,g_RunAsHelpText);
			MyRunAsFmtCurrUserRadio(hwnd,g_RunAsCurrUsrFmt);
			SetDlgTCharItemAsciiText(hwnd,IDC_RUNASSPEC,g_RunAsSpecHelp);
#ifdef FEAT_CUSTOMRUNASDLG_TRANSLATE
			MyRunAsTranslateDlg(hwnd);
#endif
			SetDlgState(hwnd,false,pRADD);

#if defined(BUILD_DBG) && 0 //auto login used during testing ;)
			SetDlgItemText(hwnd,IDC_USERNAME,_T("root"));
			SetDlgItemText(hwnd,IDC_PASSWORD,_T("???"));
			Sleep(1);PostMessage(hwnd,WM_COMMAND,IDOK,0);
#endif
		}
		return true;
	case WM_COMMAND:
		{
			switch(HIWORD(wp)) {
			case EN_CHANGE:
				VerifyOKBtn(hwnd,pRADD);
				break;
			case EN_SETFOCUS:
			case BN_CLICKED:
				if (LOWORD(wp)<=IDCANCEL)break;
				SetDlgState(hwnd,LOWORD(wp)==IDC_RUNASCURR,pRADD);
				return FALSE;
			}

			INT_PTR exitcode=!pRADD?-1:IDCANCEL;
			switch(LOWORD(wp)) {
			case IDOK:
				if (pRADD) {
					SHELLEXECUTEINFO&sei=*pRADD->pSEI;
					PROCESS_INFORMATION pi={0};
					DWORD ec=NO_ERROR;
					WCHAR*wszExec;//Also used as TCHAR buffer in AsSelf mode
					bool PerformTCharFmt=pRADD->AsSelf;
					//const DWORD CommonStartupInfoFlags=STARTF_FORCEONFEEDBACK;
#ifdef UNICODE
					PerformTCharFmt=true;
#endif
					wszExec=(WCHAR*)NSIS::MemAlloc( (pRADD->AsSelf?sizeof(TCHAR):sizeof(WCHAR)) *(lstrlen(sei.lpFile)+1+lstrlen(sei.lpParameters)+1));
					if (!wszExec)ec=ERROR_OUTOFMEMORY;
					if (PerformTCharFmt)wsprintf((TCHAR*)wszExec,_T("%s%s%s"),sei.lpFile,((sei.lpParameters&&*sei.lpParameters)?_T(" "):_T("")),sei.lpParameters);
					if (!ec) {
						if (pRADD->AsSelf) {
							STARTUPINFO si={sizeof(si)};
							TRACEF("MyRunAs:CreateProcess:%s|\n",wszExec);
							ec=(CreateProcess(0,(TCHAR*)wszExec,0,0,false,0,0,0,&si,&pi)?NO_ERROR:GetLastError());
						}
						else {
							//All Wide strings!
							WCHAR wszPwd[PWLEN+1];
							WCHAR wszUName[UNLEN+1+MYMAX_DOMAIN+1];
							STARTUPINFOW siw={sizeof(siw)};
							WCHAR*p;
#ifndef UNICODE
							//Build unicode string, we already know the buffer is big enough so no error handling
							p=wszExec;
							MultiByteToWideChar(CP_THREAD_ACP,0,sei.lpFile,-1,p,0xFFFFFF);
							if (sei.lpParameters && *sei.lpParameters) {
								p+=lstrlen(sei.lpFile);*p++=L' ';*p=0;
								MultiByteToWideChar(CP_THREAD_ACP,0,sei.lpParameters,-1,p,0xFFFFFF);
							}
#endif
							SendMessageW(GetDlgItem(hwnd,IDC_USERNAME),WM_GETTEXT,ARRAYSIZE(wszUName),(LPARAM)wszUName);
							SendMessageW(GetDlgItem(hwnd,IDC_PASSWORD),WM_GETTEXT,ARRAYSIZE(wszPwd),(LPARAM)wszPwd);
							
							//Try to find [\\]domain\user and split into username and domain strings
							WCHAR*pUName=wszUName,*pDomain=0;
							p=wszUName;
							//if (*p==p[1]=='\\')pUName=(p+=2);else \  //Should we still split things up if the string starts with \\ ? Is it possible to use \\machine\user at all?
							++p;//Don't parse "\something", require at least one char before backslash "?[*\]something"
							while(*p && *p!='\\')++p;
							if (*p=='\\') { 
								pDomain=pUName;
								pUName=p+1;*p=0;
							}

							TRACEF("MyRunAs:CreateProcessWithLogonW:%ws|%ws|%ws|%ws|\n",pUName,pDomain?pDomain:L"NO?DOMAIN",wszPwd,wszExec);
							ec=(_CreateProcessWithLogonW(pUName,pDomain?pDomain:0,wszPwd,LOGON_WITH_PROFILE,0,wszExec,0,0,0,&siw,&pi)?NO_ERROR:GetLastError());
							TRACEF("MyRunAs:CreateProcessWithLogonW: ret=%u\n",ec);
							MySecureZeroMemory(wszPwd,sizeof(wszPwd));//if (wszPwd) {volatile WCHAR*_p=wszPwd;for(;_p&&*_p;++_p)*_p=1;if (_p)*wszPwd=0;}//Burn password (And attempt to prevent compiler from removing it)	
							if (ec && ErrorIsLogonError(ec)) {
								LPTSTR szMsg;
								DWORD ret=FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,0,ec,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&szMsg,0,0);
								if (ret) {
									ec=ERRAPP_TRYAGAIN;
									MessageBox(hwnd,szMsg,0,MB_ICONWARNING);
									LocalFree(szMsg);
								}
								else ec=GetLastError();
							}
						}
					}
					NSIS::MemFree(wszExec);
					CloseHandle(pi.hThread);
					if (ERRAPP_TRYAGAIN==ec)break;
					exitcode=IDOK;
					pRADD->LastErr=ec;
					pRADD->pSEI->hProcess=pi.hProcess;
				}
			case IDCANCEL:
				EndDialog(hwnd,exitcode);
			}
		}
		break;
	}
	return FALSE;
}

DWORD WINAPI MyRunAs(HINSTANCE hInstDll,SHELLEXECUTEINFO&sei) 
{
	INT_PTR ec;
	ASSERT(sei.cbSize>=sizeof(SHELLEXECUTEINFO) && hInstDll);
	ASSERT(_CreateProcessWithLogonW);
	RUNASDLGDATA radd;
	MemZero(&radd,sizeof(radd));
	radd.pSEI=&sei;
	ec=DialogBoxParam(hInstDll,MAKEINTRESOURCE(IDD_MYRUNAS),sei.hwnd,MyRunAsDlgProc,(LPARAM)&radd);
	TRACEF("MyRunAs returned %d (%s|%s)\n",ec,sei.lpFile,sei.lpParameters);
	switch(ec) {
	case 0:
		return ERROR_INVALID_HANDLE;//DialogBoxParam returns 0 on bad hwnd
	case IDOK:
		return radd.LastErr;
	case IDCANCEL:
		return ERROR_CANCELLED;
	}
	return GetLastError();
}

#ifdef ___BUILD_DBG
extern DWORD DelayLoadFunctions();
// RunDll exports are __stdcall, we dont care about that for this debug export, rundll32.exe is able to handle this mistake
extern "C" void __declspec(dllexport) __cdecl DBGRDMyRunAs(HWND hwnd,HINSTANCE hinst,LPTSTR lpCmdLine,int nCmdShow) {
	SHELLEXECUTEINFO sei={sizeof(sei)};
	sei.lpFile=_T("cmd.exe");
	sei.lpParameters=_T("/K");
	VERIFY(!DelayLoadFunctions());
	TRACEF("ec=%d\n",MyRunAs(GetModuleHandle(_T("UAC.dLl")),sei));
}
#endif