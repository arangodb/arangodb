// Copyright (C) Anders Kjersem. Licensed under the zlib/libpng license, see License.txt for details.

#include "uac.h"
#include <objbase.h> // CoInitialize
using namespace NSIS;

CHECKTOKENMEMBERSHIP	_CheckTokenMembership=0;
#ifdef FEAT_CUSTOMRUNASDLG
GETUSERNAMEEX			_GetUserNameEx=0;
CREATEPROCESSWITHLOGONW	_CreateProcessWithLogonW=0;
SHGETVALUEA _SHGetValueA=0;
#endif

UAC_GLOBALS globals;
HINSTANCE g_hInst;
extra_parameters* g_pXP;

FORCEINLINE UAC_GLOBALS& G()         {return globals;}
FORCEINLINE UAC_GLOBALS_OUTER& OG()  {return G().outer;}
FORCEINLINE bool IsInnerInstance()   {return G().RunMode==RUNMODE_INNER;}
FORCEINLINE bool IsOuterInstance()   {return G().RunMode==RUNMODE_OUTER;}
FORCEINLINE HWND xxxGetOuterIPCWND() {return G().hwndOuter;}
FORCEINLINE HWND xxxGetOuterNSISMainWnd() {return(HWND)SendMessage(xxxGetOuterIPCWND(),OWM_GETOUTERSTATE,GOSI_HWNDMAINWND,0);}

#ifdef FEAT_AUTOPAGEJUMP
#	define Outer_GetOuterNSISMainWnd() xxxGetOuterNSISMainWnd()
#	define Inner_GetOuterNSISMainWnd() xxxGetOuterNSISMainWnd()
#endif
#ifdef FEAT_AUTOPAGEJUMP
#	define Inner_GetInnerNSISMainWnd() G().hwndNSIS
#endif
#define Inner_GetOuterIPCWND xxxGetOuterIPCWND


bool SysQuery_IsServiceRunning(LPCTSTR servicename) 
{
	bool retval=false;
	SC_HANDLE scdbh=NULL,hsvc;
	scdbh=OpenSCManager(NULL,NULL,GENERIC_READ);
	if (scdbh) 
	{
		if (hsvc=OpenService(scdbh,servicename,SERVICE_QUERY_STATUS)) 
		{
			SERVICE_STATUS ss;
			if (QueryServiceStatus(hsvc,&ss))retval=(ss.dwCurrentState==SERVICE_RUNNING);
			CloseServiceHandle(hsvc);
		}
		CloseServiceHandle(scdbh);
	}
	else 
	{
		//Guest account can't open OpenSCManager, let's just lie and pretend everything is ok!
		retval = ERROR_ACCESS_DENIED==GetLastError();
	}
	return retval;
}

inline bool NT5_IsSecondaryLogonSvcRunning() 
{
	return SysQuery_IsServiceRunning(_T("seclogon"));
}


BOOL QueryIsAdminOrGetIL(UINT32*pIL)
{
	BOOL isAdmin=false;
	OSVERSIONINFO ovi={sizeof(ovi)};
	SetLastError(NO_ERROR);
	GetVersionEx(&ovi);
	const bool OSAtleastVista = ovi.dwMajorVersion >= 6;
	HANDLE hToken = NULL;
	if (VER_PLATFORM_WIN32_NT != ovi.dwPlatformId)
	{
		isAdmin = true;
	}
	else 
	{
		if (OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hToken))
		{
			BYTE sidbuf[8+(sizeof(DWORD)*2)];
			PSID psid = (PSID) sidbuf;
			((UINT32*)psid)[0] = 1 | 2<<8;
			((UINT32*)psid)[1] = 5<<24;
			((UINT32*)psid)[2] = SECURITY_BUILTIN_DOMAIN_RID;
			((UINT32*)psid)[3] = DOMAIN_ALIAS_RID_ADMINS;
			if (_CheckTokenMembership)
			{
				if (!_CheckTokenMembership(0,psid,&isAdmin)) isAdmin=false;
			}
			else 
			{
				DWORD cbTokenGrps;
				if (!GetTokenInformation(hToken,TokenGroups,0,0,&cbTokenGrps)&&GetLastError()==ERROR_INSUFFICIENT_BUFFER) 
				{
					TOKEN_GROUPS*ptg=0;
					if (ptg=(TOKEN_GROUPS*)NSIS::MemAlloc(cbTokenGrps)) 
					{
						if (GetTokenInformation(hToken,TokenGroups,ptg,cbTokenGrps,&cbTokenGrps)) 
						{
							for (UINT i=0; i<ptg->GroupCount; ++i) 
							{
								if (EqualSid(ptg->Groups[i].Sid,psid)) isAdmin=true;
							}
						}
						NSIS::MemFree(ptg);
					}
				}
			}	
		}
	}
	UINT32 il = isAdmin ? 0x3000 : 0x2000;
	if (OSAtleastVista)
	{
		BYTE bufTokMandLbl[30];
		PTOKEN_MANDATORY_LABEL pTML=(PTOKEN_MANDATORY_LABEL)bufTokMandLbl;
		DWORD RetLen;
		il = 0;
		if (GetTokenInformation(hToken,TokenIntegrityLevel,pTML,sizeof(bufTokMandLbl),&RetLen))
		{
			PSID psid = pTML->Label.Sid;
			if ((1|1<<8) == ((UINT32*)psid)[0] && (16<<24) == ((UINT32*)psid)[1])
			{
				il = ((UINT32*)psid)[2];
			}
		}
		isAdmin = il >= 0x3000;
	}
	if (pIL) *pIL = il;
	CloseHandle(hToken);
	return isAdmin;
}
inline UINT32 GetIntegrityLevel() 
{
	UINT32 il = 0;
	QueryIsAdminOrGetIL(&il);
	return il;
}
bool IsCurrentProcessAdmin() 
{
	return !!QueryIsAdminOrGetIL(NULL);
}


bool SysQuery_UAC_IsActive() 
{
	//TODO: check AppInfo service?
	FARPROC CEE = GetProcAddress(LoadLibraryA("KERNEL32"),"CheckElevationEnabled");
	BOOL bEE;
	if (CEE && 0==((DWORD(WINAPI*)(BOOL*))CEE)(&bEE)) return !!bEE;
	ASSERT(_SHGetValueA);
	DWORD type,cbio=sizeof(DWORD),data;
	return !_SHGetValueA(HKEY_LOCAL_MACHINE,"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System","EnableLUA",&type,&data,&cbio) && data!=0;
}


LPTSTR FindExePathEnd(LPTSTR p) 
{
	if ( *p=='"' && *(++p) ) 
	{
		while( *p && *p!='"' )p=StrNextChar(p);
		if (*p)
			p=StrNextChar(p);
		else 
			return 0;
	}
	else 
		if ( *p!='/' )while( *p && *p!=' ' )p=StrNextChar(p);
	return p;
}


#ifdef FEAT_MSRUNASDLGMODHACK
HHOOK g_MSRunAsHook;
FORCEINLINE void MSRunAsDlgMod_Unload() 
{
	UnhookWindowsHookEx((HHOOK)g_MSRunAsHook);
}
LRESULT CALLBACK MSRunAsDlgMod_ShellProc(int nCode,WPARAM wp,LPARAM lp) 
{
	CWPRETSTRUCT*pCWPS;
	if (nCode >= 0 && (pCWPS=(CWPRETSTRUCT*)lp) && WM_INITDIALOG==pCWPS->message)
	{
		TCHAR buf[30];
		GetClassName(pCWPS->hwnd,buf,ARRAYSIZE(buf));
		if (!lstrcmpi(buf,_T("#32770"))) 
		{
#ifdef FEAT_AUTOPAGEJUMP
			if (!MyIsWindowVisible(G().hwndNSIS))
			{
				WndModifyLong(pCWPS->hwnd,GWL_EXSTYLE,WS_EX_APPWINDOW,WS_EX_APPWINDOW);
				SetForegroundWindow(pCWPS->hwnd);
			}		
#endif
			const UINT IDC_USRSAFER=0x106,IDC_OTHERUSER=0x104,IDC_SYSCRED=0x105;
			GetClassName(GetDlgItem(pCWPS->hwnd,IDC_SYSCRED),buf,ARRAYSIZE(buf));
			if (!lstrcmpi(buf,_T("SysCredential"))) //make sure this is the run as dialog
			{
				SndDlgItemMsg(pCWPS->hwnd,IDC_USRSAFER,BM_SETCHECK,BST_UNCHECKED);
				SndDlgItemMsg(pCWPS->hwnd,IDC_OTHERUSER,BM_CLICK);
				DBGONLY(UAC_DbgHlpr_LoadPasswordInRunAs(pCWPS->hwnd));
			}
		}
	}
	return CallNextHookEx(g_MSRunAsHook,nCode,wp,lp);
}
void MSRunAsDlgMod_Init() 
{
	g_MSRunAsHook=NULL;
	if(GetOSVerMaj()==5 && GetOSVerMin()>=1) //only XP/2003
		g_MSRunAsHook=SetWindowsHookEx(WH_CALLWNDPROCRET,MSRunAsDlgMod_ShellProc,0,GetCurrentThreadId());
}
#endif


FORCEINLINE void* DelayLoadFunction(HMODULE hLib,LPCSTR Export) 
{
	return GetProcAddress(hLib,Export);
}

DWORD DelayLoadFunctions() 
{
#ifdef UNICODE
#	define __DLD_FUNCSUFFIX "W"
#else
#	define __DLD_FUNCSUFFIX "A"
#endif
	HMODULE hModAdvAPI=LoadLibraryA("ADVAPI32")
		,hModShlWAPI=LoadLibraryA("ShlWAPI") ;
	// Optional:
	_CheckTokenMembership=(CHECKTOKENMEMBERSHIP)DelayLoadFunction(hModAdvAPI,"CheckTokenMembership");
#ifdef FEAT_CUSTOMRUNASDLG
	_GetUserNameEx=(GETUSERNAMEEX)DelayLoadFunction(LoadLibraryA("SECUR32"),"GetUserNameEx" __DLD_FUNCSUFFIX);//We never free this library
	_CreateProcessWithLogonW=(CREATEPROCESSWITHLOGONW)DelayLoadFunction(hModAdvAPI,"CreateProcessWithLogonW");
	_SHGetValueA=(SHGETVALUEA)DelayLoadFunction(hModShlWAPI,"SHGetValueA");
#endif
	return NO_ERROR;
}


bool GetOuterHwndFromCommandline(HWND&hwndOut) 
{
	LPTSTR p=FindExePathEnd(GetCommandLine());
	while(p && *p==' ')p=CharNext(p);
	TRACEF("GetOuterHwndFromCommandline:%s|\n",p);
	if (p && *p++=='/'&&*p++=='U'&&*p++=='A'&&*p++=='C'&&*p++==':') 
	{
		hwndOut=(HWND)StrToUInt(p,true);
		return (/*IsWindow*/(hwndOut))!=0;
	}
	return false;
}


UINT_PTR __cdecl NSISPluginCallback(enum NSPIM Event) 
{
	TRACEF("NSISPluginCallback:%d %d:%d\n",Event,GetCurrentProcessId(),GetCurrentThreadId());
	switch(Event) 
	{
	case NSPIM_UNLOAD:
		if (IsOuterInstance()) 
		{
			SendMessage(G().hwndOuter,WM_CLOSE,0,0);
			WaitForSingleObject(OG().hOuterThread,INFINITE);
			BUILDRLSCANLEAK(CloseHandle(OG().hOuterThread));
		}
		BUILDRLSCANLEAK(CloseHandle(G().PerformOp));
		BUILDRLSCANLEAK(CloseHandle(G().CompletedOp));
		UnmapViewOfFile(G().pShared);
		BUILDRLSCANLEAK(CloseHandle(G().hSharedMap));
		TRACEF("NSISPluginCallback:NSPIM_UNLOAD completed cleanup, ready to be unloaded...\n");
		break;
	}
	return NULL;
}


BOOL CALLBACK ProcessThreadMessages(HWND hwndDlg=NULL,DWORD*pExitCode=NULL) 
{
	MSG msg;
	while( PeekMessage(&msg,NULL,0,0,PM_REMOVE) ) 
	{
		if (msg.message==WM_QUIT) 
		{
			TRACEF("ProcessThreadMessages got WM_QUIT %d:%d, code=%u hDlg=%X\n",GetCurrentProcessId(),GetCurrentThreadId(),msg.wParam,hwndDlg);
			if (pExitCode)*pExitCode=msg.wParam;
			return true;
		//	ASSERT(!"TODO: should we handle WM_QUIT?");
		//	ec=ERROR_CANCELLED;
		//	break;
		}
		if (!hwndDlg || !IsDialogMessage(hwndDlg,&msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return false;
}


bool Inner_DoOpInOuter(UINT opid,UINT Direction) 
{
	OP_HDR*pOP=(OP_HDR*)G().pShared;
	pOP->Op=opid;
	pOP->Direction=Direction;
//	TRACEF("DoOp %d %d PID=%u\n",opid,Direction,GetCurrentProcessId());

	if (SetEvent(G().PerformOp)) 
	{
		for(DWORD w;;) 
		{
			w=MsgWaitForMultipleObjects(1,&G().CompletedOp,false,INFINITE,QS_ALLINPUT);
			switch(w) 
			{
			case WAIT_OBJECT_0:
				return true;
			case WAIT_OBJECT_0+1:
				if (ProcessThreadMessages())break;
				continue;
			default:
				ASSERT(!"DoOp>MsgWaitForMultipleObjects failed!");
				break;
			}
		}
	}
	ASSERT(!"DoOp failed!");
	return false;
}


void OP_SYNCREG_CopyRegisters(bool VarToShared,UINT offset,const UINT count) 
{
	const UINT cbOneVar=G().NSIScchStr*sizeof(NSISCH);
	offset*=cbOneVar;
	BYTE*pD=G().pShared+sizeof(OP_SYNCREG),*pS=((BYTE*)G().NSISVars)+offset;
	if (!VarToShared){BYTE*temp=pD;pD=pS;pS=temp;}
	MemCopy(pD,pS,count*cbOneVar);
}


bool Inner_DoSyncRegisters(UINT Dir,UINT regoffset=INST_0,UINT count=10) 
{
	OP_SYNCREG*pOP=(OP_SYNCREG*)G().pShared;
	pOP->RegOffset=regoffset;
	pOP->Count=count;
//	TRACEF("*** Inner_DoSyncRegisters BEGIN PID=%u\n",GetCurrentProcessId());
	for (int i=0;i<2;++i) 
	{
		if (Dir==OPDIR_I2O)OP_SYNCREG_CopyRegisters(true,pOP->RegOffset,pOP->Count);//CopyRegisters(pSharedBaseOffset,G().NSISVars,pOP->RegOffset,pOP->Count);
		if (!Inner_DoOpInOuter(OPID_SYNCREG,Dir))return false;
		if (Dir==OPDIR_O2I)OP_SYNCREG_CopyRegisters(false,pOP->RegOffset,pOP->Count);//CopyRegisters(G().NSISVars,pSharedBaseOffset,pOP->RegOffset,pOP->Count);
		pOP->RegOffset=INST_R0;
	}
//	TRACEF("*** Inner_DoSyncRegisters END\n");
	return true;
}


inline void Inner_SyncNSISVar(UINT Dir,bool instdir,bool outdir) 
{
	const UINT first=instdir?INST_INSTDIR:INST_OUTDIR;
	Inner_DoSyncRegisters(Dir,first,(INST_OUTDIR+1)-first);
}


LRESULT CALLBACK OuterWndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) 
{
	switch(msg) 
	{
	case WM_DESTROY:
#ifdef UAC_HACK_FGNDWND2011
		SendMessage(hwnd,OWM_HIDESPLASH,0,0);
#endif
		PostMessage(NULL,WM_QUIT,0,NULL);
		break;
	case WM_CLOSE:
		return DestroyWindow(hwnd);
	case OWM_INITINNER:
		TRACEF("OuterWndProc:OWM_INITINNER %X\n",OG().hInnerProcess);
		if (OG().hInnerProcess)
		{
			const HANDLE hThisProcess=GetCurrentProcess(),hTargetProcess=OG().hInnerProcess;
			HANDLE destMap;
			if (DuplicateHandle(hThisProcess,G().hSharedMap,hTargetProcess,&destMap,FILE_MAP_WRITE|FILE_MAP_READ,false,0))
			{
				BYTE*p=(BYTE*)G().pShared;
				HANDLE dest,src[]={G().PerformOp,G().CompletedOp};
				for (int i=0;i<ARRAYSIZE(src);++i) 
				{
					if (!DuplicateHandle(hThisProcess,src[i],hTargetProcess,&dest,SYNCHRONIZE|EVENT_MODIFY_STATE,false,/*DUPLICATE_SAME_ACCESS*/0)) 
					{
						TRACEF("DuplicateHandle on src[%d] failed with %d\n",i,GetLastError());
						ASSERT(dest);
						return NULL;
					}
					*((HANDLE*)p)=dest;
					p+=sizeof(HANDLE);
				}
				TRACEF("OuterWndProc:OWM_INITINNER duped handles, returning shared handle %X\n",destMap);
				return (LRESULT)destMap;
			}
			else 
			{
				TRACEF("DuplicateHandle on G().hSharedMap failed with %d\n",GetLastError());
			}
		}
		return NULL;
	case OWM_ISREADYFORINIT:
		TRACEF("OuterWndProc:OWM_ISREADYFORINIT %X\n",OG().hInnerProcess);
		return OG().hInnerProcess?0x666:false;
	case OWM_PERFORMWINDOWSWITCH:
		TRACEF("OuterWndProc:OWM_PERFORMWINDOWSWITCH %X:%X\n",wp,lp);
#if 1
		if (GetWindowStyle((HWND)lp)&WS_VISIBLE) 
		{
			HWND hOuterNSIS=(HWND)wp;
			HWND hInnerNSIS=(HWND)lp;
			SetForegroundWindow(hInnerNSIS);
			if (1)
			{
				//Just hiding the outer window works, but can cause focus problems with MessageBox in outer instance
				ShowWindow(/*hInnerNSIS??*/hOuterNSIS,SW_HIDE);
			}
			else
			{
				SetWindowLongPtr(hOuterNSIS,GWL_STYLE,WS_CHILD|WS_VISIBLE);
				if (!SendMessage(hInnerNSIS,IWM_HELPWITHWINDOWSWITCH,0,0)) ShowWindow(hOuterNSIS,SW_HIDE);
				SetWindowPos(hOuterNSIS,HWND_BOTTOM,0,0,0,0,SWP_NOACTIVATE|SWP_ASYNCWINDOWPOS|SWP_FRAMECHANGED|SWP_NOOWNERZORDER);//SWP_NOSENDCHANGING
			}
		}
		else
			/*if (lp)*/PostMessage(hwnd,msg,wp,lp);//this is ugly, but we need to wait for the inner window
		return 0;
#endif

#ifdef FEAT_AUTOPAGEJUMP
	case OWM_GETOUTERSTATE:
		switch(wp) 
		{
		case GOSI_HWNDMAINWND:return (LRESULT)G().hwndNSIS;
		case GOSI_PAGEJUMP:return G().JumpPageOffset;
#ifdef GOSI_PID
		case GOSI_PID:return GetCurrentProcessId();
#endif
		}
		return NULL;
#endif
	case OWM_SETOUTERSTATE:
		TRACEF("OWM_SETOUTERSTATE %X %X\n",wp,lp);
		switch(wp)
		{
		case SOSI_PROCESSDUPHANDLE:
			if (!OG().hInnerProcess && lp)
			{
				//TODO: Verfiy that this handle has the correct PID?
				TRACEF("OWM_SETOUTERSTATE:SOSI_PROCESSDUPHANDLE got %X\n",lp);
				OG().hInnerProcess=(HANDLE)lp;
				return 0x666;
			}
			break;
#ifdef FEAT_AUTOPAGEJUMP
		case SOSI_ELEVATIONPAGE_GUIINITERRORCODE:
			OG().ecInner_InitSharedData=lp;
			break;
#endif
		}
		return NULL;
#ifdef UAC_HACK_INNERPARENT2010
	case OWM_PARENTIFY:
		{
			TRACEF("OWM_PARENTIFY %X %X\n",wp,lp);
			HWND hOuterNSIS=(HWND)wp;
			HWND hInnerNSIS=(HWND)lp;
			SetWindowLongPtr(hOuterNSIS,GWL_STYLE,WS_CHILD|WS_VISIBLE);
			SetParent(hOuterNSIS,hInnerNSIS);
			SetWindowPos(hOuterNSIS,HWND_BOTTOM,0,0,0,0,SWP_NOACTIVATE|SWP_ASYNCWINDOWPOS|SWP_FRAMECHANGED|SWP_NOOWNERZORDER);//SWP_NOSENDCHANGING
			TRACEF("\tOWM_PARENTIFY completed...\n");
		}
		return false;
#endif
#ifdef UAC_HACK_FGNDWND2011
	case OWM_HIDESPLASH:
		TRACEF("OWM_HIDESPLASH %X\n",OG().hwndSplash);
		ShowWindow(OG().hwndSplash,SW_HIDE);
		DestroyWindow(OG().hwndSplash);
		break;
#endif
	}
	return DefWindowProc(hwnd,msg,wp,lp);
}


DWORD CALLBACK OuterWndThread(LPVOID param) 
{
	DWORD ec=NO_ERROR;
	CoInitialize(NULL);
	if ( G().hwndOuter=CreateWindowEx(WS_EX_TOOLWINDOW,_T("Static"),NULL,/*WS_VISIBLE*/0,0,0,0,0,NULL,0,g_hInst,NULL) )
	{
		TRACEF("OuterWndThread: Created outer IPC window %X\n",G().hwndOuter);
		SetWindowLongPtr(G().hwndOuter,GWLP_WNDPROC,(LONG_PTR)OuterWndProc);
		SetEvent(G().CompletedOp);//Tell the other thread we have a window and will now process messages
		do
		{	//TODO: Since DoOp now calls MsgWaitForMultipleObjects, we can probably move this back to the RunElevated msgloop
			const HANDLE hWaitObjs[]={G().PerformOp};
			const UINT waitCount=ARRAYSIZE(hWaitObjs);
			DWORD w=MsgWaitForMultipleObjects(waitCount,hWaitObjs,false,INFINITE,QS_ALLINPUT);
			switch(w) 
			{
			case WAIT_OBJECT_0:
				{
					OP_HDR*pHdr=(OP_HDR*)G().pShared;
					TRACEF("UAC perform outer op: %d %d PID=%u:%u \n",pHdr->Op,pHdr->Direction,GetCurrentProcessId(),GetCurrentThreadId());
					switch(pHdr->Op) 
					{
					case OPID_SYNCREG:
						{
							OP_SYNCREG*pSR=(OP_SYNCREG*)G().pShared;
							OP_SYNCREG_CopyRegisters(pSR->OpHdr.Direction==OPDIR_O2I,pSR->RegOffset,pSR->Count);
						}
						break;
					case OPID_CALLCODESEGMENT:
						TRACEF("OPID_CALLCODESEGMENT: %u\n",((OP_CALLCODESEGMENT*)pHdr)->Pos);
						if (((OP_CALLCODESEGMENT*)pHdr)->Flags&OPCCSF_SETOUTPATH)SetCurrentDirectory(NSIS_UNSAFE_GETVAR(INST_OUTDIR,G().NSISVars,G().NSIScchStr));
						g_pXP->ExecuteCodeSegment(((OP_CALLCODESEGMENT*)pHdr)->Pos,NULL);
						break;
/*					case OPID_GETOUTERSTATE:
						MemCopy(G().pShared,&G(),sizeof(UAC_GLOBALS));
						break;*/
					default:
						TRACEF("OPID_???\n");
					}
				}
				SetEvent(G().CompletedOp);
				break;
			case WAIT_OBJECT_0+1:
				if (ProcessThreadMessages(NULL))ec=ERROR_CANCELLED;
				break;
			default:
				ec=GetLastError();
				ASSERT(ec);
				TRACEF("OuterWndThread MsgWait failed with %u\n",ec);
			}
		}
		while( G().hwndOuter && NO_ERROR==ec );
	}
	else 
		ec=GetLastError();
//	CoUninitialize();
	TRACEF("OuterWndThread %d:%d ending with %d\n",GetCurrentProcessId(),GetCurrentThreadId(),ec);
	return ec;
}


FORCEINLINE DWORD Outer_InitShared(UINT NSIScchStr) 
{
	UINT ec,cbMap=sizeof(OP_SYNCREG) + ((sizeof(NSISCH)*NSIScchStr)*10);
	if (true
		&& (G().PerformOp=CreateEvent(/*&sa*/NULL,false,false,NULL))
		&& (G().CompletedOp=CreateEvent(/*&sa*/NULL,false,false,NULL))
		&& (G().hSharedMap=CreateFileMapping(INVALID_HANDLE_VALUE,/*&sa*/NULL,PAGE_READWRITE|SEC_COMMIT,0,cbMap,NULL))
		&& (G().pShared=(BYTE*)MapViewOfFile(G().hSharedMap,FILE_MAP_ALL_ACCESS,0,0,0))
		) 
		ec=NO_ERROR;
	else
		ec=GetLastError();
	return ec;
}

FORCEINLINE DWORD Outer_CreateOuterWindow() 
{
	DWORD ec=NO_ERROR;
	if ( !(OG().hOuterThread=CreateThread(0,1,OuterWndThread,NULL,0,NULL)) )return GetLastError();
	if ( WAIT_OBJECT_0!=WaitForSingleObject(G().CompletedOp,INFINITE) )GetExitCodeThread(OG().hOuterThread,&ec);
	return ec;
}


DWORD Inner_InitSharedData() 
{
	ASSERT(G().hwndOuter);
	TRACEF("Inner_InitSharedData() has=%X\n",G().hSharedMap);
	if (!G().hSharedMap) 
	{
		HANDLE hOuterProcess,hThisProcess=GetCurrentProcess();
		DWORD pid;
		ASSERT(G().hwndOuter);
		GetWindowThreadProcessId(G().hwndOuter,&pid);
		ASSERT(pid);
		BOOL HadDbgPriv;
		EnablePrivilege(SE_DEBUG_NAME,true,&HadDbgPriv);
		TRACEF("try to open process outer pid=%u\n",pid);
		if (!(hOuterProcess=OpenProcess(PROCESS_DUP_HANDLE,false,pid)))goto dieGLE;
		TRACEF("hOuterProcess with dup right =%X\n",hOuterProcess);
		DuplicateHandle(hThisProcess,hThisProcess,hOuterProcess,&hOuterProcess,PROCESS_DUP_HANDLE,false,DUPLICATE_CLOSE_SOURCE);
		EnablePrivilege(SE_DEBUG_NAME,HadDbgPriv,NULL);

		
		//The handle we get from ShExecEx can only be syncronized with (NT6+), so we give the outer process a handle with DUP_HANDLE rights so it can give us access to its shared memory
		if (0x666!=SendMessage(G().hwndOuter,OWM_SETOUTERSTATE,SOSI_PROCESSDUPHANDLE,(LPARAM)hOuterProcess))return ERROR_INVALID_WINDOW_HANDLE;
		//Wait for a very unlikely racecond.
		while(SendMessage(G().hwndOuter,OWM_ISREADYFORINIT,0,0)!=0x666)Sleep(50);
		
		TRACEF("Inner_InitSharedData: Sending OWM_INITINNER to get hSharedMap...\n");
		G().hSharedMap=(HANDLE)SendMessage(G().hwndOuter,OWM_INITINNER,0,0);
		ASSERT(G().hSharedMap);
		
		if (G().pShared=(BYTE*)MapViewOfFile(G().hSharedMap,FILE_MAP_ALL_ACCESS,0,0,0)) 
		{
			HANDLE*p=(HANDLE*)G().pShared;
			G().PerformOp=*p;
			G().CompletedOp=*(++p);
		}
		if (!G().pShared) 
		{
			TRACEF("Inner_InitSharedData failed!\n");
			ASSERT(G().pShared);
			return ERROR_INVALID_PARAMETER;
		}
	}
	return NO_ERROR;
dieGLE:
	return GetLastError();
}


#ifdef FEAT_AUTOPAGEJUMP
LRESULT CALLBACK OuterMainWndSubProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) 
{
	switch(msg)
	{
	case WM_NOTIFY_OUTER_NEXT:
		G().JumpPageOffset+=wp;
		TRACEF("WM_NOTIFY_OUTER_NEXT: now=%d change=%d\n",G().JumpPageOffset,wp);
		break;
	}
	return CallWindowProc(G().OrgMainWndProc,hwnd,msg,wp,lp);
}
#endif


#ifdef FEAT_AUTOPAGEJUMP
LRESULT CALLBACK InnerMainWndSubProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) 
{
	switch(msg) 
	{
	case WM_WINDOWPOSCHANGED:
		{
			WINDOWPOS&wp=*(WINDOWPOS*)lp;
			if (SWP_SHOWWINDOW&wp.flags && G().JumpPageOffset!=-1) 
			{
				TRACEF("InnerMainWndSubProc:WM_WINDOWPOSCHANGED: Performing delayed onGuiInit pagejump and OWM_PERFORMWINDOWSWITCH");
				HWND hOuterMainWnd=Inner_GetOuterNSISMainWnd();
				G().JumpPageOffset=(WORD)SendMessage(G().hwndOuter,OWM_GETOUTERSTATE,GOSI_PAGEJUMP,0);
				
				TRACEF("\tposting WM_NOTIFY_OUTER_NEXT %d to autojump\n",G().JumpPageOffset);
				PostMessage(hwnd,WM_NOTIFY_OUTER_NEXT,G().JumpPageOffset,0);

				RECT rect;
				GetWindowRect(hOuterMainWnd,&rect);
				SetWindowPos(hwnd,hOuterMainWnd,rect.left,rect.top,0,0,SWP_NOSIZE);
				
/*#ifdef UAC_DELAYLOAD_CHANGEWINDOWMESSAGEFILTER
				if (_ChangeWindowMessageFilter) 
				{
					VERIFY(_ChangeWindowMessageFilter(IWM_HELPWITHWINDOWSWITCH,MSGFLT_ADD));
				}
#endif
*/				
				TRACEF("\tPosting OWM_PERFORMWINDOWSWITCH %X,%X\n",hOuterMainWnd,hwnd);
				PostMessage(G().hwndOuter,OWM_PERFORMWINDOWSWITCH,(WPARAM)hOuterMainWnd,(LPARAM)hwnd);

				G().JumpPageOffset=-1;
			}
		}
		break;
/*	case IWM_HELPWITHWINDOWSWITCH:
		TRACEF("InnerMainWndSubProc:IWM_HELPWITHWINDOWSWITCH\n");
		SetParent(Inner_GetOuterNSISMainWnd(),hwnd);
		return TRUE;
*/	}
	return CallWindowProc(G().OrgMainWndProc,hwnd,msg,wp,lp);
}
#endif


#define GetVarStr(varid) ((LPTSTR)(Vars+((varid)*NSIScchStr)))
#define SetVarUINT(varid,data) wsprintf((Vars+((varid)*NSIScchStr)),_T("%u"),(data))
#define NSISHlpr_SetErrorFlag() pXP->exec_flags->exec_error=1
#define NSISHlpr_ClearErrorFlag() pXP->exec_flags->exec_error=0


INT_PTR CALLBACK SplashDialogProc(HWND hwnd,UINT uMsg,WPARAM wp,LPARAM lp) 
{
	return false;
}


extern "C" void __declspec(dllexport) __cdecl _(HWND hwndNSIS,UINT NSIScchStr,NSISCH*Vars,NSIS::stack_t**StackTop,NSIS::extra_parameters* pXP) 
{
	stack_t*sp1=(*StackTop);
	bool freeSP1=true;
	NSISCH*p=sp1->text;
	DWORD ec=NO_ERROR;
	if (G().RunMode==RUNMODE_INIT) 
	{
		G().NSIScchStr=NSIScchStr;
		G().NSISVars=Vars;
		g_pXP=pXP;
		pXP->RegisterPluginCallback(g_hInst,NSISPluginCallback);
		if ( GetOSVerMaj() < 5 )
		{
			G().RunMode=RUNMODE_LEGACY;
		}
		else
		{
			G().RunMode = GetOuterHwndFromCommandline(G().hwndOuter) ? RUNMODE_INNER : RUNMODE_OUTER ;
			if ( !(ec=DelayLoadFunctions()) ) 
			{
				if ( G().RunMode==RUNMODE_OUTER && !(ec=Outer_InitShared(NSIScchStr)) ) ec=Outer_CreateOuterWindow();
			}
		}
	}
	TRACEF("EXPORT cmd=%c outer=%d %d:%d ec=%d\n",*(p),IsOuterInstance(),GetCurrentProcessId(),GetCurrentThreadId(),ec);
	
	NSISHlpr_ClearErrorFlag();
	if (!ec) switch(*(p))
	{
	case '0':/*** RunElevated ***/
		{
			bool IsAdmin=IsCurrentProcessAdmin();
			BYTE UACMode=0;
			const UINT OSVMaj=GetOSVerMaj();
			DWORD ForkExitCode=ERROR_CANCELLED;//is this the best value to use? is it required?
			TRACEF("UAC:RunElevated: admin=%d os=%d.%d runmode=%u\n",IsAdmin,OSVMaj,GetOSVerMin(),G().RunMode);
			
#ifdef FEAT_AUTOPAGEJUMP
			G().hwndNSIS=hwndNSIS;
#endif

			if (IsAdmin) 
			{
				UACMode=2;
				if (IsInnerInstance()) 
				{
					ec=Inner_InitSharedData();
					ASSERT(!ec);
#ifdef UAC_HACK_FGNDWND2011
					PostMessage(Inner_GetOuterIPCWND(),OWM_HIDESPLASH,0,0);
#endif
				}
			}
			else 
			{
				if (IsInnerInstance()) //non-admin used in runas dialog
				{
					if (!(ec=Inner_InitSharedData())) 
					{
						UACMode=1;//fake value so the script calls quit without further processing
						pXP->exec_flags->errlvl=0;
						((OP_HDR*)G().pShared)->Op=OPID_OUTERMUSTRETRY; //DoOp(OPID_OUTERMUSTRETRY,0);//tell the outer process about this problem
					}
					else 
					{
						ASSERT(!ec);
					}
				}
				else 
				{
					SHELLEXECUTEINFO sei;
					MemZero(&sei,sizeof(sei));
					LPTSTR pszExePathBuf=0;
					LPTSTR pszParamBuf=0;
					LPTSTR p,pCL=GetCommandLine();
					UINT len;
					LPCTSTR verb_runas = _T("runas");

					sei.cbSize=sizeof(sei);
					sei.hwnd=hwndNSIS?hwndNSIS:G().hwndOuter;
					sei.nShow=SW_SHOWNORMAL;
					sei.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOZONECHECKS|SEE_MASK_FLAG_DDEWAIT;
					//TODO: |= SEE_MASK_UNICODE ??
					sei.lpVerb=verb_runas;

#ifdef UAC_HACK_FGNDWND2011
					if (!MyIsWindowVisible(hwndNSIS)) 
					{
						HMODULE hExeMod=GetModuleHandle(0);
						if (!OG().hwndSplash) OG().hwndSplash=CreateDialog(hExeMod,MAKEINTRESOURCE(111),0,SplashDialogProc);
						if (OG().hwndSplash) 
						{
							sei.hwnd=OG().hwndSplash;
							//MySetDlgItemText(sei.hwnd,1030,NULL);
							LONG xs = WS_EX_TOOLWINDOW;
							// Hacky invisible splash screen, 
							// you can hex edit "runas" > "Runas" to remove this hack and display it.
							if (*verb_runas=='r') xs |= WS_EX_TRANSPARENT|WS_EX_LAYERED;
							WndModifyLong(sei.hwnd,GWL_EXSTYLE,xs,xs);
							SetWindowPos(sei.hwnd,0,0,0,0,0,SWP_FRAMECHANGED|SWP_NOSIZE|SWP_NOMOVE|SWP_NOZORDER);
							HICON hIcon=LoadIcon(hExeMod,MAKEINTRESOURCE(103));
							HWND hCtl = NULL;
							for(;hCtl = FindWindowEx(sei.hwnd,hCtl,NULL,NULL);) ShowWindow(hCtl,SW_HIDE);
							HWND hIcoCtl=GetDlgItem(sei.hwnd,1030);
							ShowWindow(hIcoCtl,SW_SHOW);
							RECT r;
							GetClientRect(sei.hwnd,&r);
							SetWindowPos(hIcoCtl,0,0,0,r.right,r.bottom,SWP_NOZORDER|SWP_NOACTIVATE);
							WndModifyLong(hIcoCtl,GWL_STYLE,0,SS_ICON|SS_CENTERIMAGE);
							SNDMSG(hIcoCtl,STM_SETICON,(WPARAM)hIcon,0);
							SNDMSG(sei.hwnd,WM_SETICON,ICON_BIG,(LPARAM)hIcon);
							ShowWindow(sei.hwnd,SW_SHOW);
						}
					}
#endif
					
					p=FindExePathEnd(pCL);
					len=(p-pCL)/sizeof(TCHAR);
					ec=ERROR_FILE_NOT_FOUND;

					if (p && len)
					{
						for (;ec!=ERROR_OUTOFMEMORY;) 
						{
							NSIS::MemFree(pszExePathBuf);
							if (!(pszExePathBuf=(LPTSTR)NSIS::MemAlloc((++len)*sizeof(TCHAR))))
								ec=ERROR_OUTOFMEMORY;
							else
							{
								if ( GetModuleFileName(NULL,pszExePathBuf,len) < len ) //FUCKO: what if GetModuleFileName returns 0?
								{
									TRACEF("module=%s|\n",pszExePathBuf);
									ec=0;
									break; 
								}
							}
							len+=MAX_PATH;
						}
						if (!ec)
						{
							sei.lpFile=pszExePathBuf;
							len=lstrlen(p);
							len+=20;//space for "/UAC:xxxxxx /NCRC\0"
							if (!(pszParamBuf=(LPTSTR)NSIS::MemAlloc(len*sizeof(TCHAR))))
								ec=ERROR_OUTOFMEMORY;
							else 
							{
								wsprintf(pszParamBuf,_T("/UAC:%X /NCRC%s"),G().hwndOuter,p);//NOTE: The argument parser depends on our special flag appearing first
								sei.lpParameters=pszParamBuf;
								
								if (OSVMaj==5) 
								{
									bool hasseclogon=NT5_IsSecondaryLogonSvcRunning();
									TRACEF("SysNT5IsSecondaryLogonSvcRunning=%d\n",hasseclogon);
									if (!hasseclogon)ec=ERROR_SERVICE_NOT_ACTIVE;
								}
								if (!ec) 
								{
									SetForegroundWindow(sei.hwnd);
									TRACEF("UAC:Elevate: Execute: %s|%s|\n",sei.lpFile,sei.lpParameters);

									#ifdef FEAT_CUSTOMRUNASDLG
									if (OSVMaj>=6 && !SysQuery_UAC_IsActive()) //NOTE: v6 not hardcoded, will MS ever fix this?
									{
										ec=MyRunAs(g_hInst,sei);
										TRACEF("MyRunAs ret, ec=%u\n",ec);
									}
									else
									#endif
									{
										#ifdef FEAT_MSRUNASDLGMODHACK
										MSRunAsDlgMod_Init();
										#endif
										
										ec=(ShellExecuteEx(&sei)?NO_ERROR:GetLastError());
										TRACEF("ShellExecuteEx ret, gle=%u\n",ec);

										#ifdef FEAT_MSRUNASDLGMODHACK
										MSRunAsDlgMod_Unload();
										#endif
									}

									if (!ec) 
									{
										bool endWait=false;
										TRACEF("RunElevated: %d:%d waiting for fork hP=%X\n",GetCurrentProcessId(),GetCurrentThreadId(),sei.hProcess);
										
										do 
										{
											DWORD w=MsgWaitForMultipleObjects(1,&sei.hProcess,false,INFINITE,QS_ALLINPUT);
											switch(w)
											{
											case WAIT_OBJECT_0:
												VERIFY(GetExitCodeProcess(sei.hProcess,&ForkExitCode));
												endWait=true;
												UACMode= ((OP_HDR*)G().pShared)->Op==OPID_OUTERMUSTRETRY ? 3 : 1;
												pXP->exec_flags->errlvl=ForkExitCode;
												TRACEF("UAC:RunElevated: Inner process exit with %d, UACMode=%d\n",ForkExitCode,UACMode);
												break;
											case WAIT_OBJECT_0+1:
												ProcessThreadMessages(hwndNSIS);
												break;
											default:
												endWait=true;
												ec=GetLastError();
												TRACEF("UAC:RunElevated:MsgWaitForMultipleObjects failed, ec=%u w=%u\n",ec,w);
											}
										}
										while(!endWait && NO_ERROR==ec);
										CloseHandle(sei.hProcess);
										if (OG().ecInner_InitSharedData) 
										{
											TRACEF("inner process completed, but never got access to shared mem? ec=%u forkexit=%u ecInner_InitSharedData=%u\n",ec,ForkExitCode,OG().ecInner_InitSharedData);
											ec=OG().ecInner_InitSharedData;
										}
									}
								}
							}
						}
					}
				}
			}
			if (IsOuterInstance()) 
			{
				if(G().JumpPageOffset)--G().JumpPageOffset;
				CloseHandle(OG().hInnerProcess);
				OG().hInnerProcess=NULL;
			}

			pXP->exec_flags->errlvl=ec?ec:ForkExitCode;//We now set the errlvl no matter what, is this a good idea?
			SetVarUINT(INST_0,ec);
			SetVarUINT(INST_1,UACMode);
			SetVarUINT(INST_2,ForkExitCode);
			SetVarUINT(INST_3,IsAdmin);
			TRACEF("UAC:RunElevated end: ec=%u mode=%d forkexit=%u admin=%d isouter=%d\n",ec,UACMode,ForkExitCode,IsAdmin,IsOuterInstance());
		}
		break;
	case '1':/*** ExecCodeSegment(flags) ***/
		{
			int pos=StrToUInt(&p[1]);
			if (pos--)
			{
				UINT ecsflags=0;
				TCHAR*pFlags=&p[1];
				while(*pFlags && *pFlags!=':')++pFlags;
				if (*pFlags) 
				{
					++pFlags;
					TRACEF("ExecCodeSegment>Flags=%s|\n",pFlags);
					ecsflags=StrToUInt(pFlags);
				}

				if (IsInnerInstance())
				{
					if (!(ec=Inner_InitSharedData())) 
					{
						const bool SyncRegisters=(ecsflags&UAC_SYNCREGISTERS)!=0;
						const bool SyncInstDir=(ecsflags&UAC_SYNCINSTDIR)!=0;
						const bool SyncOutDir=(ecsflags&UAC_SYNCOUTDIR)!=0;
						if (SyncRegisters)Inner_DoSyncRegisters(OPDIR_I2O);
						Inner_SyncNSISVar(OPDIR_I2O,SyncInstDir,SyncOutDir);
						
						OP_CALLCODESEGMENT*pOP=(OP_CALLCODESEGMENT*)G().pShared;
						pOP->Pos=pos;
						pOP->Flags=SyncOutDir?OPCCSF_SETOUTPATH:NULL;
						Inner_DoOpInOuter(OPID_CALLCODESEGMENT,OPDIR_I2O);
						
						if (SyncRegisters)Inner_DoSyncRegisters(OPDIR_O2I);
						Inner_SyncNSISVar(OPDIR_O2I,SyncInstDir,SyncOutDir);
						if (SyncOutDir)SetCurrentDirectory(NSIS_UNSAFE_GETVAR(INST_OUTDIR,G().NSISVars,G().NSIScchStr));
					}
					else 
					{
						ASSERT(!ec);
					}
				}
				else
				{
					pXP->ExecuteCodeSegment(pos,NULL);
				}
			}
		}
		break;
	case '2':/*** IsAdmin ***/
		TRACEF("UAC_IsAdmin: %d:%d outer=%d admin=%d\n",GetCurrentProcessId(),GetCurrentThreadId(),IsOuterInstance(),IsCurrentProcessAdmin());
		if (p[1]=='s')freeSP1=false;
		wsprintf(freeSP1 ? (Vars+(INST_0*NSIScchStr)) : p ,_T("%u"),IsCurrentProcessAdmin());
		ec=GetLastError();
		TRACEF("UAC:IsAdmin PID=%X ret=%d useReg=%d ec=%X\n",GetCurrentProcessId(),IsCurrentProcessAdmin(),freeSP1,ec);
		break;
	case '3':/*** IsInnerInstance ***/
		TRACEF("UAC_IsInnerInstance: %d:%d outer=%d admin=%d\n",GetCurrentProcessId(),GetCurrentThreadId(),IsOuterInstance(),IsCurrentProcessAdmin());
		if (p[1]=='s')freeSP1=false;
		wsprintf(freeSP1 ? (Vars+(INST_0*NSIScchStr)) : p ,_T("%u"),IsInnerInstance());
		TRACEF("UAC:IsInnerInstance PID=%d ret=%d useReg=%d\n",GetCurrentProcessId(),IsInnerInstance(),freeSP1);
		break;
#ifdef FEAT_AUTOPAGEJUMP
	case '4':/*** Notify_OnGuiInit / PageElevation_OnGuiInit ***/
		ASSERT(hwndNSIS);
		TRACEF("UAC_Notify_OnGuiInit: %d:%d outer=%d admin=%d\n",GetCurrentProcessId(),GetCurrentThreadId(),IsOuterInstance(),IsCurrentProcessAdmin());

		if (!(G().OrgMainWndProc=(WNDPROC)SetWindowLongPtr(hwndNSIS,GWLP_WNDPROC,(LONG_PTR)(IsInnerInstance()?InnerMainWndSubProc:OuterMainWndSubProc))))
		{
			ec=ERROR_INVALID_WINDOW_HANDLE;
		}
		if (IsInnerInstance()) 
		{
			if (!ec) ec=Inner_InitSharedData();
			ASSERT(!ec);
			TRACEF("Inner_InitSharedData ret %u\n",ec);
		}
		else 
		{
			DBGONLY(SetForegroundWindow(FindWindowA("dbgviewClass",0)));
		}
		//SetVarUINT(INST_0,ec);
		break;
	case '5':/*** PageElevation_OnInit ***/
		ASSERT(!ec);
		if (IsInnerInstance()) 
		{
			ec=Inner_InitSharedData();
			SendMessage(Inner_GetOuterIPCWND(),OWM_SETOUTERSTATE,SOSI_ELEVATIONPAGE_GUIINITERRORCODE,ec);
		}
		break;
#endif
	case '6':/*** GetTokenIntegrityLevel ***/
		{
			const UINT32 il = GetIntegrityLevel();
			wsprintf(p,_T("%u"),il);
			freeSP1 = false;
		}
		break;
/*	case '?':/*** FindCmdLineParams *** /
		{
		freeSP1=false;
		NSISCH*pStart=&p[1];
		NSISCH*pParams=FindExePathEnd( *pStart == ':' ? ++pStart : pStart=GetCommandLine() );
		pParams=StrSkipWhitespace(pParams);
		MemCopy(p,pParams,(lstrlen(pParams)+1)*sizeof(NSISCH));
		}
		break;*/
	}

	if (freeSP1) 
	{
		*StackTop=(*StackTop)->next;
		NSIS::MemFree(sp1);
	}
	if (ec) NSISHlpr_SetErrorFlag();
}



#ifdef _DEBUG
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInst,DWORD Event,LPVOID lpReserved) 
#else
EXTERN_C BOOL WINAPI _DllMainCRTStartup(HINSTANCE hInst,ULONG Event,LPVOID lpReserved) 
#endif
{
	if(Event==DLL_PROCESS_ATTACH) 
	{
		g_hInst=hInst;
		MemZero(&globals,sizeof(UAC_GLOBALS));
		ASSERT(G().RunMode==RUNMODE_INIT);
		DBGONLY(HWND dummy;if(!GetOuterHwndFromCommandline(dummy))DBG_RESETDBGVIEW(););
		TRACEF("*** UAC DllMain PID=%u TID=%d\n",GetCurrentProcessId(),GetCurrentThreadId());
	}
	TRACEF("*** UAC DllMain Event=%d\n",Event);
	return true;
}
