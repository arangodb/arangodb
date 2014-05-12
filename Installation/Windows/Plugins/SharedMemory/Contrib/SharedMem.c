#define WIN32_LEAN_AND_MEAN
#ifdef _WIN64
#define WINVER 0x502
#else
#define WINVER 0x400
#endif

#include <windows.h>
#include <aclapi.h>
#include <tchar.h>
#include <sddl.h>
#include <stdio.h>
 
#ifdef UNICODE
#include "nsis_unicode/pluginapi.h"
#else
#include "nsis/pluginapi.h"
#endif

///////////////////////////////////////////////////////////
///                                        Global Variables 
///////////////////////////////////////////////////////////

HINSTANCE g_hInstance = NULL;
int g_string_size = 1024;
extra_parameters* g_extra = NULL;

///////////////////////////////////////////////////////////
/// size of the memory segment
///////////////////////////////////////////////////////////
#define BUF_SIZE 256

///////////////////////////////////////////////////////////
/// handle for the memory segment
///////////////////////////////////////////////////////////

// HANDLE hMapFile = NULL;

///////////////////////////////////////////////////////////
/// content of the memory segment
///////////////////////////////////////////////////////////

// LPCTSTR pBuf = NULL;

///////////////////////////////////////////////////////////
/// content of the memory segment
///////////////////////////////////////////////////////////

TCHAR * SHARED_MEM_SEGMENT = "__TRIANGES_INSTALLER_STATE__";

///////////////////////////////////////////////////////////
///                                    PLUG-IN Handling
/// Copied from AccesPlugin
///////////////////////////////////////////////////////////


#define PUBLIC_FUNCTION(Name) \
  EXTERN_C void __declspec(dllexport) __cdecl Name(HWND hWndParent, int string_size, TCHAR* variables, stack_t** stacktop, extra_parameters* extra) \
{ \
  EXDLL_INIT(); \
  g_string_size = string_size; \
  g_extra = extra;

#define PUBLIC_FUNCTION_END \
}

///////////////////////////////////////////////////////////
///                                    Forward declarations 
///////////////////////////////////////////////////////////

BOOL CreateMyDACL(SECURITY_ATTRIBUTES * pSA);

///////////////////////////////////////////////////////////
/// puts the given (error) value into stack
///////////////////////////////////////////////////////////

void PushReturnValue(int value);

///////////////////////////////////////////////////////////
///                                        Public Functions
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
///
/// param none
///
///////////////////////////////////////////////////////////

PUBLIC_FUNCTION(CreateSharedMemory)
{
  HANDLE hMapFile = NULL;
  LPCTSTR pBuf = NULL;
  TCHAR * szName = SHARED_MEM_SEGMENT;
  SECURITY_ATTRIBUTES attr;
  attr.bInheritHandle = 1;
  attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  attr.lpSecurityDescriptor = NULL;

  CreateMyDACL(&attr);
  hMapFile = CreateFileMapping(
    INVALID_HANDLE_VALUE,    // use paging file
    &attr,                    // our security specification
    PAGE_READWRITE,          // read/write access
    0,                       // maximum object size (high-order DWORD)
    BUF_SIZE,                // maximum object size (low-order DWORD)
    szName);                 // name of mapping object

  if (hMapFile == NULL)
  {

    PushReturnValue(GetLastError()); // Could not create file mapping object
    return;
  }
  // memory was created succefull
  // generate the buffer for input/output operatíons
  pBuf = (LPTSTR)MapViewOfFile(hMapFile,   // handle to map object
    FILE_MAP_ALL_ACCESS, // read/write permission
    0,
    0,
    BUF_SIZE);

  if (pBuf == NULL)
  {
    PushReturnValue(GetLastError());  // Could not map view of file

    CloseHandle(hMapFile);

    hMapFile = NULL;
  }


  PushReturnValue(0);
}

PUBLIC_FUNCTION_END

///////////////////////////////////////////////////////////
///
/// param content is the content of the shared memory segment
///
///////////////////////////////////////////////////////////

PUBLIC_FUNCTION(WriteIntoSharedMem)
{
  HANDLE hMapFile = NULL;
  LPCTSTR pBuf = NULL;

  TCHAR * szMsg = (TCHAR*)LocalAlloc(LPTR, g_string_size * sizeof(TCHAR));
  if (!szMsg) {
    PushReturnValue(-2); // memory could not be allocated
    return;
  }
  hMapFile = OpenFileMapping(
    FILE_MAP_WRITE,        // read/write access
    FALSE,                 // do not inherit the name
    SHARED_MEM_SEGMENT);               // name of mapping object

  if (hMapFile == NULL)
  {
    PushReturnValue(GetLastError());
    // _tprintf(TEXT("Could not open file mapping object (%d).\n"),
    //  GetLastError());
    return;
  }

  pBuf = (LPTSTR)MapViewOfFile(hMapFile, // handle to map object
    FILE_MAP_WRITE,  // read/write permission
    0,
    0,
    BUF_SIZE);

  if (pBuf == NULL)
  {
    // _tprintf(TEXT("Could not map view of file (%d).\n"),
    //   GetLastError());

    PushReturnValue(GetLastError());

    CloseHandle(hMapFile);

    return;
  }

  // read name for mem segment from stack
  popstring(szMsg);
  CopyMemory((PVOID)pBuf, szMsg, (_tcslen(szMsg) * sizeof(TCHAR)));

  CloseHandle(hMapFile);

  PushReturnValue(0);
}
PUBLIC_FUNCTION_END

///////////////////////////////////////////////////////////
///
/// param no
///
///////////////////////////////////////////////////////////

PUBLIC_FUNCTION(ReadIntoSharedMem)
{
  HANDLE hMapFile = NULL;
  LPCTSTR pBuf = NULL;

  hMapFile = OpenFileMapping(
    FILE_MAP_WRITE,        // read/write access
    FALSE,                 // do not inherit the name
    SHARED_MEM_SEGMENT);               // name of mapping object

  if (hMapFile == NULL)
  {

    pushstring(TEXT("Could not open file mapping object"));

    PushReturnValue(GetLastError());
    // _tprintf(TEXT("Could not open file mapping object (%d).\n"),
    //  GetLastError());
    return;
  }

  pBuf = (LPTSTR)MapViewOfFile(hMapFile, // handle to map object
    FILE_MAP_ALL_ACCESS,  // read/write permission
    0,
    0,
    BUF_SIZE);

  if (pBuf == NULL)
  {
    // _tprintf(TEXT("Could not map view of file (%d).\n"),
    //   GetLastError());
    pushstring(TEXT("Could not map view of file"));

    PushReturnValue(GetLastError());

    CloseHandle(hMapFile);

    return;
  }


  pushstring(TEXT(pBuf));

  PushReturnValue(0);

  UnmapViewOfFile(pBuf);

  CloseHandle(hMapFile);

}
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(ExistsSharedMem)
{ 
  HANDLE hMapFile = NULL;

  hMapFile = OpenFileMapping(
    FILE_MAP_WRITE,        // read/write access
    FALSE,                 // do not inherit the name
    SHARED_MEM_SEGMENT);               // name of mapping object

  if (hMapFile == NULL)
  {
    PushReturnValue(GetLastError());
    // _tprintf(TEXT("Could not open file mapping object (%d).\n"),
    //  GetLastError());
    return;
  }

  CloseHandle(hMapFile);

  PushReturnValue(1);
}

PUBLIC_FUNCTION_END

// CreateMyDACL.
//    Create a security descriptor that contains the DACL 
//    you want.
//    This function uses SDDL to make Deny and Allow ACEs.
//
// Parameter:
//    SECURITY_ATTRIBUTES * pSA
//    Pointer to a SECURITY_ATTRIBUTES structure. It is your
//    responsibility to properly initialize the 
//    structure and to free the structure's 
//    lpSecurityDescriptor member when you have
//    finished using it. To free the structure's 
//    lpSecurityDescriptor member, call the 
//    LocalFree function.
// 
// Return value:
//    FALSE if the address to the structure is NULL. 
//    Otherwise, this function returns the value from the
//    ConvertStringSecurityDescriptorToSecurityDescriptor 
//    function.
BOOL CreateMyDACL(SECURITY_ATTRIBUTES * pSA)
{
  // Define the SDDL for the DACL. This example sets 
  // the following access:
  //     Built-in guests are denied all access.
  //     Anonymous logon is denied all access.
  //     Authenticated users are allowed 
  //     read/write/execute access.
  //     Administrators are allowed full control.
  // Modify these values as needed to generate the proper
  // DACL for your application. 
  TCHAR * szSD = TEXT("D:")       // Discretionary ACL
    TEXT("(A;OICI;GA;;;BG)")     // Allow built-in guests
    TEXT("(A;OICI;GA;;;AN)")     // Allow anonymous logon
    TEXT("(A;OICI;GA;;;AU)")
    //TEXT("(A;OICI;GRGWGX;;;AU)") // Allow 
    // read/write/execute 
    // to authenticated 
    // users
    TEXT("(A;OICI;GA;;;BA)");    // Allow full control 
  // to administrators

  if (NULL == pSA)
    return FALSE;

  return ConvertStringSecurityDescriptorToSecurityDescriptor(
    szSD,
    SDDL_REVISION_1,
    &(pSA->lpSecurityDescriptor),
    NULL);
}


void PushReturnValue(int value) {
  char str[256];
  memset(str, '\0', 256);
  sprintf_s(str, 256, "%d", value);
  pushstring(str);
}

#ifdef _VC_NODEFAULTLIB
#define DllMain _DllMainCRTStartup
#endif
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
  g_hInstance = (HINSTANCE)hInst;
  return TRUE;
}
