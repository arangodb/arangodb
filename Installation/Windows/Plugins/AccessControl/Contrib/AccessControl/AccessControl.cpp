
/* AccessControl plugin for NSIS
 * Copyright (C) 2003 Mathias Hasselmann <mathias@taschenorakel.de>
 *
 * This software is provided 'as-is', without any express or implied 
 * warranty. In no event will the authors be held liable for any damages 
 * arising from the use of this software. 
 *
 * Permission is granted to anyone to use this software for any purpose, 
 * including commercial applications, and to alter it and redistribute it 
 * freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not 
 *      claim that you wrote the original software. If you use this software 
 *      in a product, an acknowledgment in the product documentation would be
 *      appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not 
 *      be misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any source 
 *      distribution.
 */

/* Modifications by Afrow UK, kichik and AndersK.
 * See Docs\AccessControl\Readme.txt for full modifications list.
 */

#define WIN32_LEAN_AND_MEAN
#ifdef _WIN64
#define WINVER 0x502
#else
#define WINVER 0x400
#endif

#include <windows.h>
#ifdef UNICODE
#include "nsis_unicode/pluginapi.h"
#else
#include "nsis_ansi/pluginapi.h"
#endif
#include <aclapi.h>
#include <sddl.h>

/*****************************************************************************
 GLOBAL VARIABLES
 *****************************************************************************/

HINSTANCE g_hInstance = NULL;
int g_string_size = 1024;
extra_parameters* g_extra = NULL;

/*****************************************************************************
 TYPE DEFINITIONS
 *****************************************************************************/

typedef struct
{
  const TCHAR* name;
  SE_OBJECT_TYPE type;
  DWORD defaultInheritance;
  const TCHAR** const permissionNames;
  const DWORD* const permissionFlags;
  const int permissionCount;
}
SchemeType;

typedef struct
{
  BOOL noInherit;
  HKEY hRootKey;
}
Options;

typedef enum
{
  ChangeMode_Owner,
  ChangeMode_Group
}
ChangeMode;

/*****************************************************************************
 UTILITIES
 *****************************************************************************/

#define SIZE_OF_ARRAY(Array) (sizeof((Array)) / sizeof(*(Array)))
#define ARRAY_CONTAINS(Array, Index) (Index >= 0 && Index < SIZE_OF_ARRAY(Array))

void* LocalAllocZero(size_t cb) { return LocalAlloc(LPTR, cb); }
inline void* LocalAlloc(size_t cb) { return LocalAllocZero(cb); }

/*****************************************************************************
 PLUG-IN HANDLING
 *****************************************************************************/

#define PUBLIC_FUNCTION(Name) \
extern "C" void __declspec(dllexport) __cdecl Name(HWND hWndParent, int string_size, TCHAR* variables, stack_t** stacktop, extra_parameters* extra) \
{ \
  EXDLL_INIT(); \
  g_string_size = string_size; \
  g_extra = extra;

#define PUBLIC_FUNCTION_END \
}

void showerror_s(const TCHAR* fmt, TCHAR* arg)
{
  TCHAR* msg = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));
  if (msg)
  {
    wsprintf(msg, fmt, arg);
    pushstring(msg);
    LocalFree(msg);
  }
}

void showerror_d(const TCHAR* fmt, DWORD arg)
{
  TCHAR* msg = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));
  if (msg)
  {
    wsprintf(msg, fmt, arg);
    pushstring(msg);
    LocalFree(msg);
  }
}

#define ERRSTR_OOM TEXT("OutOfMemory")

#define ABORT_s(x, y) \
    { showerror_s(TEXT(x), y); goto cleanup; }
#define ABORT_d(x, y) \
    { showerror_d(TEXT(x), y); goto cleanup; }
#define ABORT(x) \
    { pushstring(TEXT(x)); goto cleanup; }

/*****************************************************************************
 COMPATIBILITY UTILITIES
 *****************************************************************************/

#define BuildExplicitAccessWithNameA BuildExplicitAccessWithNameT
#define BuildExplicitAccessWithNameW BuildExplicitAccessWithNameT
#if WINVER < 0x500
#define ConvertStringSidToSidA Compat_ConvertStringSidToSidT
#define ConvertStringSidToSidW Compat_ConvertStringSidToSidT

static FARPROC AdvApi32_GetProcAddress(LPCSTR Name)
{
  return GetProcAddress(LoadLibraryA("ADVAPI32"), Name);
}

TCHAR* CharPos(TCHAR* szStr, int cchStrLen, TCHAR chFind)
{
  for (int i = 0; i < cchStrLen && NULL != *(szStr + i); i++)
    if (*(szStr + i) == chFind)
      return szStr + i;
  return NULL;
}

BYTE FromHex(TCHAR* szHex)
{
  int a = 0;
  int b = 0;

  if (szHex[0] >= TEXT('0') && szHex[0] <= TEXT('9'))
    a = szHex[0] - TEXT('0');
  else if (szHex[0] >= TEXT('A') && szHex[0] <= TEXT('F'))
    a = szHex[0] - TEXT('A') + 10;
  else if (szHex[0] >= TEXT('a') && szHex[0] <= TEXT('f'))
    a = szHex[0] - TEXT('a') + 10;

  if (szHex[1] >= TEXT('0') && szHex[1] <= TEXT('9'))
    b = szHex[1] - TEXT('0');
  else if (szHex[1] >= TEXT('A') && szHex[1] <= TEXT('F'))
    b = szHex[1] - TEXT('A') + 10;
  else if (szHex[1] >= TEXT('a') && szHex[1] <= TEXT('f'))
    b = szHex[1] - TEXT('a') + 10;

  return a * 16 + b;
}

static void SetIdentifierAuthority(SID_IDENTIFIER_AUTHORITY&sia, DWORD val32)
{
  sia.Value[5] = (BYTE)((val32 & 0x000000FF) >> 0);
  sia.Value[4] = (BYTE)((val32 & 0x0000FF00) >> 8);
  sia.Value[3] = (BYTE)((val32 & 0x00FF0000) >> 16);
  sia.Value[2] = (BYTE)((val32 & 0xFF000000) >> 24);
  sia.Value[1] = sia.Value[0] = 0;
}

// Based on GetBinarySid function from http://www.codeguru.com/cpp/w-p/system/security/article.php/c5659.
BOOL Compat_ConvertStringSidToSidT(LPTSTR szSid, PSID* ppSid)
{  
  // Try to call the real function on 2000+, 
  // this will enable support for more SID Strings (Those will not work on NT4 so script carefully)
  FARPROC fp = AdvApi32_GetProcAddress(sizeof(TCHAR) > 1 ? "ConvertStringSidToSidW" : "ConvertStringSidToSidA");
  bool ret = fp && ((BOOL(WINAPI*)(LPCTSTR,PSID*))fp)(szSid,ppSid);
  if (ret) return ret;

  *ppSid = NULL;
  BYTE nByteAuthorityCount = 0;
  PSID pSid = LocalAllocZero(8 + (sizeof(DWORD) * SID_MAX_SUB_AUTHORITIES));
  if (!pSid) return false;

  // Building a revision 1 SID in memory, the rest of the code assumes that pSid is zero filled!
  ((char*)pSid)[0] = 1;
  SID_IDENTIFIER_AUTHORITY &sidIA = *(SID_IDENTIFIER_AUTHORITY*) ((char*)pSid + 2);
  DWORD *pSidSA = (DWORD*) ((char*)pSid + 8);

  static const struct { char id0, id1; BYTE ia, sa0, sa1; } sidmap[] = {
    {'A'|32,'N'|32, (5), (7) , 0}, // NT AUTHORITY\ANONYMOUS LOGON
    {'A'|32,'U'|32, (5), (11), 0}, // NT AUTHORITY\Authenticated Users
    {'B'|32,'A'|32, (5), (32), (544)-500}, // BUILTIN\Administrators
    {'B'|32,'U'|32, (5), (32), (545)-500}, // BUILTIN\Users
    {'I'|32,'U'|32, (5), (4) , 0}, // NT AUTHORITY\INTERACTIVE
    {'S'|32,'Y'|32, (5), (18), 0}, // NT AUTHORITY\SYSTEM
    {'W'|32,'D'|32, (1), (0) , 0}, // Everyone
  };
  // Try to lookup a SID string
  for (int i = 0; i < SIZE_OF_ARRAY(sidmap); ++i)
  {
    if ((szSid[0]|32) != sidmap[i].id0 || (szSid[1]|32) != sidmap[i].id1 || szSid[2]) continue;
    SetIdentifierAuthority(sidIA, sidmap[i].ia);
    pSidSA[nByteAuthorityCount++] = sidmap[i].sa0;
    if (sidmap[i].sa1) pSidSA[nByteAuthorityCount++] = (DWORD)sidmap[i].sa1 + 500;
    goto done;
  }

  // S-SID_REVISION- + identifierauthority- + subauthorities + NULL

  // Skip S
  PTSTR ptr;
  if (!(ptr = CharPos(szSid, lstrlen(szSid), TEXT('-'))))
    return FALSE;
  ptr++;

  // Skip SID_REVISION
  if (!(ptr = CharPos(ptr, lstrlen(ptr), TEXT('-'))))
    return FALSE;
  ptr++;

  // Skip identifierauthority
  PTSTR ptr1;
  if (!(ptr1 = CharPos(ptr, lstrlen(ptr), TEXT('-'))))
    return FALSE;
  *ptr1 = 0;
  
  if ((*ptr == TEXT('0')) && (*(ptr+1) == TEXT('x')))
  {
    sidIA.Value[0] = FromHex(ptr);
    sidIA.Value[1] = FromHex(ptr + 2);
    sidIA.Value[2] = FromHex(ptr + 4);
    sidIA.Value[3] = FromHex(ptr + 8);
    sidIA.Value[4] = FromHex(ptr + 10);
    sidIA.Value[5] = FromHex(ptr + 12);
  }
  else
  {
    SetIdentifierAuthority(sidIA, myatou(ptr));
  }

  // Skip -
  *ptr1 = TEXT('-'), ptr = ptr1, ptr1++;

  for (int i = 0; i < 8; i++)
  {
    // Get subauthority count.
    if (!(ptr = CharPos(ptr, lstrlen(ptr), TEXT('-')))) break;
    *ptr = 0, ptr++, nByteAuthorityCount++;
  }

  for (int i = 0; i < nByteAuthorityCount; i++)
  {
    // Get subauthority.
    pSidSA[i] = myatou(ptr1);
    ptr1 += lstrlen(ptr1) + 1;
  }

done:
  if (nByteAuthorityCount)
  {
    *ppSid = pSid, ((char*)pSid)[1] = nByteAuthorityCount;
    return TRUE;
  }
  LocalFree(pSid);
  return FALSE;
}

// Based on GetTextualSid function from http://www.codeguru.com/cpp/w-p/system/security/article.php/c5659.
BOOL ConvertSidToStringSidNoAlloc(const PSID pSid, LPTSTR pszSid)
{
  // Validate the binary SID
  if(!IsValidSid(pSid)) return FALSE;

  // Get the identifier authority value from the SID
  PSID_IDENTIFIER_AUTHORITY psia = GetSidIdentifierAuthority(pSid);

  // Get the number of subauthorities in the SID.
  DWORD dwSubAuthorities = *GetSidSubAuthorityCount(pSid);

  // Compute the buffer length
  // S-SID_REVISION- + IdentifierAuthority- + subauthorities- + NULL
  DWORD dwSidSize = (15 + 12 + (12 * dwSubAuthorities) + 1) * sizeof(TCHAR);

  // Add 'S' prefix and revision number to the string
  dwSidSize = wsprintf(pszSid, TEXT("S-%lu-"), SID_REVISION);

  // Add SID identifier authority to the string.
  if((psia->Value[0] != 0) || (psia->Value[1] != 0))
  {
    dwSidSize += wsprintf(pszSid + lstrlen(pszSid),
        TEXT("0x%02hx%02hx%02hx%02hx%02hx%02hx"),
        (USHORT)psia->Value[0],
        (USHORT)psia->Value[1],
        (USHORT)psia->Value[2],
        (USHORT)psia->Value[3],
        (USHORT)psia->Value[4],
        (USHORT)psia->Value[5]);
  }
  else
  {
    dwSidSize += wsprintf(pszSid + lstrlen(pszSid),
        TEXT("%lu"),
        (ULONG)(psia->Value[5]) +
        (ULONG)(psia->Value[4] << 8) +
        (ULONG)(psia->Value[3] << 16) +
        (ULONG)(psia->Value[2] << 24));
  }

  // Add SID subauthorities to the string
  for(DWORD dwCounter = 0; dwCounter < dwSubAuthorities; dwCounter++)
  {
    dwSidSize += wsprintf(pszSid + dwSidSize, TEXT("-%lu"),
        *GetSidSubAuthority(pSid, dwCounter));
  }

  return TRUE;
}

#ifndef UNICODE
#define GetNamedSecurityInfoA Delayload_GetNamedSecurityInfoA
#define SetEntriesInAclA Delayload_SetEntriesInAclA
#define SetNamedSecurityInfoA Delayload_SetNamedSecurityInfoA

DWORD Delayload_GetNamedSecurityInfoA(LPSTR ON, SE_OBJECT_TYPE OT, SECURITY_INFORMATION SI, PSID*ppSO, PSID*ppSG, PACL*ppD, PACL*ppS, PSECURITY_DESCRIPTOR*ppSD)
{
  FARPROC fp = AdvApi32_GetProcAddress("GetNamedSecurityInfoA");
  if (!fp) return ERROR_NOT_SUPPORTED;
  return ((DWORD(WINAPI*)(LPSTR,SE_OBJECT_TYPE,SECURITY_INFORMATION,PSID*,PSID*,PACL*,PACL*,PSECURITY_DESCRIPTOR*))fp)(ON,OT,SI,ppSO,ppSG,ppD,ppS,ppSD);
}

DWORD Delayload_SetEntriesInAclA(ULONG c, PEXPLICIT_ACCESS_A pEA, PACL pOA, PACL*ppNA)
{
  FARPROC fp = AdvApi32_GetProcAddress("SetEntriesInAclA");
  if (!fp) return ERROR_NOT_SUPPORTED;
  return ((DWORD(WINAPI*)(ULONG,PEXPLICIT_ACCESS_A,PACL,PACL*))fp)(c,pEA,pOA,ppNA);
}

DWORD Delayload_SetNamedSecurityInfoA(LPSTR ON, SE_OBJECT_TYPE OT, SECURITY_INFORMATION SI, PSID pSO, PSID pSG, PACL pD, PACL pS)
{
  FARPROC fp = AdvApi32_GetProcAddress("SetNamedSecurityInfoA");
  if (!fp) return ERROR_NOT_SUPPORTED;
  return ((DWORD(WINAPI*)(LPSTR,SE_OBJECT_TYPE,SECURITY_INFORMATION,PSID,PSID,PACL,PACL))fp)(ON,OT,SI,pSO,pSG,pD,pS);
}
#endif // ~!UNICODE
#else
BOOL ConvertSidToStringSidNoAlloc(const PSID pSid, LPTSTR pszSid)
{
  LPTSTR tmp;
  if (!ConvertSidToStringSid(pSid, &tmp)) return false;
  lstrcpy(pszSid, tmp), LocalFree(tmp);
  return true;
}
#endif // ~WINVER < 0x500

static VOID BuildExplicitAccessWithNameT(PEXPLICIT_ACCESS pEA, LPTSTR TN, DWORD AP, ACCESS_MODE AM, DWORD Inheritance)
{
  TRUSTEE &t = pEA->Trustee;
  t.pMultipleTrustee = NULL, t.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
  t.TrusteeForm = TRUSTEE_IS_NAME, t.TrusteeType = TRUSTEE_IS_UNKNOWN;
  t.ptstrName = TN, pEA->grfInheritance = Inheritance;
  pEA->grfAccessPermissions = AP, pEA->grfAccessMode = AM;
}

/*****************************************************************************
 STRING PARSERS
 *****************************************************************************/

/** Converts a string into an enumeration index. If the enumeration
 ** contains the string the index of the string within the enumeration
 ** is return. On error you'll receive -1.
 **/
static int ParseEnum(const TCHAR* keywords[], const TCHAR* str)
{
  const TCHAR** key;

  for(key = keywords; *key; ++key)
    if (!lstrcmpi(str, *key)) return (int)(key - keywords);

  return -1;
}

/** Parses a trustee string. If enclosed in brackets the string contains
 ** a string SID. Otherwise it's assumed that the string contains a
 ** trustee name.
 **/
static TCHAR* ParseTrustee(TCHAR* trustee, DWORD* trusteeForm)
{
  TCHAR* strend = trustee + lstrlen(trustee) - 1;

  if (TEXT('(') == *trustee && TEXT(')') == *strend)
  {
    PSID pSid = NULL;

    *strend = TEXT('\0'); 
    trustee++;

    if (!ConvertStringSidToSid(trustee, &pSid))
      pSid = NULL;

    *trusteeForm = TRUSTEE_IS_SID;
    return (TCHAR*)pSid;
  }

  *trusteeForm = TRUSTEE_IS_NAME;
  TCHAR* ret = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));
  if (ret) lstrcpy(ret, trustee);
  return ret;
}

static PSID ParseSid(TCHAR* trustee)
{
  PSID pSid = NULL;
  TCHAR* strend = trustee + lstrlen(trustee) - 1;

  if (TEXT('(') == *trustee && TEXT(')') == *strend)
  {
    *strend = TEXT('\0'); 
    ++trustee;

    if (!ConvertStringSidToSid(trustee, &pSid)) pSid = NULL;
  }
  else
  {
    DWORD sidLen = 0;
    DWORD domLen = 0;
    TCHAR* domain = NULL;
    SID_NAME_USE use;

    if ((LookupAccountName(NULL, trustee, 
       NULL, &sidLen, NULL, &domLen, &use) ||
       ERROR_INSUFFICIENT_BUFFER == GetLastError()) &&
        NULL != (domain = (TCHAR*)LocalAlloc(domLen*sizeof(TCHAR))) &&
      NULL != (pSid = (PSID)LocalAlloc(sidLen)))
    {
      if (!LookupAccountName(NULL, trustee, 
        pSid, &sidLen, domain, &domLen, &use))
      {
        LocalFree(pSid);
        pSid = NULL;
      }
    }

    LocalFree(domain);
  }

  return pSid;
}

/* i know: this function is far to generious in accepting strings. 
 * but hey: this all is about code size, isn't it? 
 * so i can live with that pretty well.
 */
static DWORD ParsePermissions(const SchemeType* scheme, TCHAR* str)
{
  DWORD perms = 0;
  TCHAR* first, * last;

  for(first = str; *first; first = last)
  {
    int token;

    while(*first && *first <= TEXT(' ')) ++first;
    for(last = first; *last && *last > TEXT(' ') && *last != TEXT('|') && 
      *last != TEXT('+'); ++last);
    if (*last) *last++ = TEXT('\0');

    token = ParseEnum(scheme->permissionNames, first);
    if (token >= 0 && token < scheme->permissionCount)
      perms|= scheme->permissionFlags[token];
  }

  return perms;
}

/*****************************************************************************
 SYMBOL TABLES
 *****************************************************************************/

static const TCHAR* g_filePermissionNames[] =
{
  TEXT("ReadData"), TEXT("WriteData"), TEXT("AppendData"), 
  TEXT("ReadEA"), TEXT("WriteEA"), TEXT("Execute"), TEXT("ReadAttributes"), TEXT("WriteAttributes"), 
  TEXT("Delete"), TEXT("ReadControl"), TEXT("WriteDAC"), TEXT("WriteOwner"), TEXT("Synchronize"),
  TEXT("FullAccess"), TEXT("GenericRead"), TEXT("GenericWrite"), TEXT("GenericExecute"), NULL
};

static const DWORD g_filePermissionFlags[] =
{
  FILE_READ_DATA, FILE_WRITE_DATA, FILE_APPEND_DATA,
  FILE_READ_EA, FILE_WRITE_EA, FILE_EXECUTE, FILE_READ_ATTRIBUTES, FILE_WRITE_ATTRIBUTES,
  DELETE, READ_CONTROL, WRITE_DAC, WRITE_OWNER, SYNCHRONIZE,
  FILE_ALL_ACCESS, FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE
};

static const TCHAR* g_directoryPermissionNames[] =
{
  TEXT("ListDirectory"), TEXT("AddFile"), TEXT("AddSubdirectory"), 
  TEXT("ReadEA"), TEXT("WriteEA"), TEXT("Traverse"), TEXT("DeleteChild"), 
  TEXT("ReadAttributes"), TEXT("WriteAttributes"), 
  TEXT("Delete"), TEXT("ReadControl"), TEXT("WriteDAC"), TEXT("WriteOwner"), TEXT("Synchronize"),
  TEXT("FullAccess"), TEXT("GenericRead"), TEXT("GenericWrite"), TEXT("GenericExecute"), NULL
};

static const DWORD g_directoryPermissionFlags[] =
{
  FILE_LIST_DIRECTORY, FILE_ADD_FILE, FILE_ADD_SUBDIRECTORY,
  FILE_READ_EA, FILE_WRITE_EA, FILE_TRAVERSE, FILE_DELETE_CHILD,
  FILE_READ_ATTRIBUTES, FILE_WRITE_ATTRIBUTES,
  DELETE, READ_CONTROL, WRITE_DAC, WRITE_OWNER, SYNCHRONIZE,
  FILE_ALL_ACCESS, FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE
};

static const TCHAR* g_registryPermissionNames[] =
{
  TEXT("QueryValue"), TEXT("SetValue"), TEXT("CreateSubKey"), 
  TEXT("EnumerateSubKeys"), TEXT("Notify"), TEXT("CreateLink"), 
  TEXT("Delete"), TEXT("ReadControl"), TEXT("WriteDAC"), TEXT("WriteOwner"), TEXT("Synchronize"),
  TEXT("GenericRead"), TEXT("GenericWrite"), TEXT("GenericExecute"), TEXT("FullAccess"), NULL
};

static const DWORD g_registryPermissionFlags[] =
{
  KEY_QUERY_VALUE, KEY_SET_VALUE, KEY_CREATE_SUB_KEY, 
  KEY_ENUMERATE_SUB_KEYS, KEY_NOTIFY, KEY_CREATE_LINK, 
  DELETE, READ_CONTROL, WRITE_DAC, WRITE_OWNER, SYNCHRONIZE,
  KEY_READ, KEY_WRITE, KEY_EXECUTE, KEY_ALL_ACCESS
};

static const SchemeType g_fileScheme[] =
{
  TEXT("file"), SE_FILE_OBJECT,
  OBJECT_INHERIT_ACE,
  g_filePermissionNames, 
  g_filePermissionFlags,
  SIZE_OF_ARRAY(g_filePermissionFlags)
};

static const SchemeType g_directoryScheme[] =
{
  TEXT("directory"), SE_FILE_OBJECT,
  OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE,
  g_directoryPermissionNames, 
  g_directoryPermissionFlags,
  SIZE_OF_ARRAY(g_directoryPermissionFlags)
};

static const SchemeType g_registryScheme[] =
{ 
  TEXT("registry"), SE_REGISTRY_KEY,
  CONTAINER_INHERIT_ACE,
  g_registryPermissionNames, 
  g_registryPermissionFlags,
  SIZE_OF_ARRAY(g_registryPermissionFlags)
};

static const TCHAR* g_rootKeyNames[] =
{
  TEXT("HKCR"), TEXT("HKCU"), TEXT("HKLM"), TEXT("HKU"), NULL
};

static const HKEY g_rootKeyValues[] =
{
  HKEY_CLASSES_ROOT, HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, HKEY_USERS, NULL
};

static const TCHAR* g_rootKeyPrefixes[] =
{
  TEXT("CLASSES_ROOT\\"), TEXT("CURRENT_USER\\"), TEXT("MACHINE\\"), TEXT("USERS\\")
};

/*****************************************************************************
 GENERIC ACL HANDLING
 *****************************************************************************/

static DWORD MySetNamedSecurityInfo(const SchemeType* scheme, TCHAR* path, SECURITY_INFORMATION si, PSID psidOwner, PSID psidGroup, PACL pDacl, Options* options)
{
  DWORD ret;

  if (scheme->type == SE_REGISTRY_KEY && (g_extra->exec_flags->alter_reg_view & KEY_WOW64_64KEY))
  {
    HKEY hKey = NULL;
    if ((ret = RegOpenKeyEx(options->hRootKey, path, 0, (si & OWNER_SECURITY_INFORMATION || si & GROUP_SECURITY_INFORMATION ? WRITE_OWNER : 0)|(si & DACL_SECURITY_INFORMATION ? WRITE_DAC : 0)|KEY_WOW64_64KEY, &hKey)) == ERROR_SUCCESS)
    {
      SECURITY_DESCRIPTOR sd;
      InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
      if (si & OWNER_SECURITY_INFORMATION)
        SetSecurityDescriptorOwner(&sd, psidOwner, FALSE);
      if (si & GROUP_SECURITY_INFORMATION)
        SetSecurityDescriptorGroup(&sd, psidGroup, FALSE);
      if (si & DACL_SECURITY_INFORMATION)
        SetSecurityDescriptorDacl(&sd, TRUE, pDacl, FALSE);
      ret = RegSetKeySecurity(hKey, si, &sd);
      RegCloseKey(hKey);
    }
  }
  else
  {
    ret = SetNamedSecurityInfo(path, scheme->type, si, psidOwner, psidGroup, pDacl, NULL);
  }

  return ret;
}

static DWORD MyGetNamedSecurityInfo(const SchemeType* scheme, TCHAR* path, SECURITY_INFORMATION si, PSECURITY_DESCRIPTOR* ppsd, Options* options)
{
  DWORD ret = 1;
  
  if (scheme->type == SE_REGISTRY_KEY && (g_extra->exec_flags->alter_reg_view & KEY_WOW64_64KEY))
  {
    HKEY hKey = NULL;
    if ((ret = RegOpenKeyEx(options->hRootKey, path, 0, READ_CONTROL|KEY_WOW64_64KEY, &hKey)) == ERROR_SUCCESS)
    {
      DWORD dwSize = 0;
      if ((ret = RegGetKeySecurity(hKey, si, NULL, &dwSize)) == ERROR_INSUFFICIENT_BUFFER)
        if (NULL != (*ppsd = (PSECURITY_DESCRIPTOR)LocalAlloc(dwSize)))
          ret = RegGetKeySecurity(hKey, si, *ppsd, &dwSize);
      RegCloseKey(hKey);
    }
  }
  else
  {
    ret = GetNamedSecurityInfo(path, scheme->type, si, NULL, NULL, NULL, NULL, ppsd);
  }

  return ret;
}

static DWORD ChangeDACL(const SchemeType* scheme, TCHAR* path, DWORD mode, Options* options, TCHAR* param)
{
  TCHAR* trusteeName = NULL;
  PSECURITY_DESCRIPTOR psd = NULL;
  PSID pSid = NULL;
  DWORD trusteeForm = TRUSTEE_IS_NAME;
  DWORD permissions = 0;
  PACL pOldAcl = NULL;
  PACL pNewAcl = NULL;
  EXPLICIT_ACCESS access;
  BOOL present = TRUE;
  BOOL defaulted = FALSE;

  DWORD ret = 0;

  if (popstring(param))
    ABORT("Trustee is missing");

  if (NULL == (trusteeName = ParseTrustee(param, &trusteeForm)))
    ABORT_s("Bad trustee (%s)", param);

  if (popstring(param))
    ABORT("Permission flags are missing");

  if (0 == (permissions = ParsePermissions(scheme, param)))
    ABORT_s("Bad permission flags (%s)", param);
  
  ret = MyGetNamedSecurityInfo(scheme, path, DACL_SECURITY_INFORMATION, &psd, options);
  if (ret != ERROR_SUCCESS)
    ABORT_d("Cannot read access control list. Error code: %d", ret);

  GetSecurityDescriptorDacl(psd, &present, &pOldAcl, &defaulted);

  BuildExplicitAccessWithName(&access, TEXT(""), permissions, (ACCESS_MODE)mode, scheme->defaultInheritance);

  access.Trustee.TrusteeForm = (TRUSTEE_FORM)trusteeForm;
  access.Trustee.ptstrName = trusteeName;
  if (options->noInherit)
    access.grfInheritance = NO_INHERITANCE;

  ret = SetEntriesInAcl(1, &access, pOldAcl, &pNewAcl);
  if (ret != ERROR_SUCCESS)
    ABORT_d("Cannot build new access control list. Error code: %d", ret);

  ret = MySetNamedSecurityInfo(scheme, path, DACL_SECURITY_INFORMATION, NULL, NULL, pNewAcl, options);
  if (ret != ERROR_SUCCESS)
    ABORT_d("Cannot apply new access control list. Error code: %d", ret);

cleanup:
  LocalFree(pNewAcl);
  LocalFree(psd);
  LocalFree(trusteeName);
  return ret;
}

static DWORD ChangeInheritance(const SchemeType* scheme, TCHAR* path, Options* options)
{
  PSECURITY_DESCRIPTOR psd = NULL;
  PACL pOldAcl = NULL;
  BOOL present = TRUE;
  BOOL defaulted = FALSE;

  DWORD ret = 0;
  
  ret = MyGetNamedSecurityInfo(scheme, path, DACL_SECURITY_INFORMATION, &psd, options);
  if (ret != ERROR_SUCCESS)
    ABORT_d("Cannot read access control list. Error code: %d", ret);

  GetSecurityDescriptorDacl(psd, &present, &pOldAcl, &defaulted);
  
  ret = MySetNamedSecurityInfo(scheme, path, DACL_SECURITY_INFORMATION | (!options->noInherit ? UNPROTECTED_DACL_SECURITY_INFORMATION : PROTECTED_DACL_SECURITY_INFORMATION), NULL, NULL, pOldAcl, options);
  if (ret != ERROR_SUCCESS)
    ABORT_d("Cannot change access control list inheritance. Error code: %d", ret);

cleanup:
  LocalFree(psd);
  return ret;
}

BOOL SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege) 
{
  TOKEN_PRIVILEGES tp;
  LUID luid;

  if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid))
    return FALSE;

  tp.PrivilegeCount = 1;
  tp.Privileges[0].Luid = luid;
  tp.Privileges[0].Attributes = (bEnablePrivilege ? SE_PRIVILEGE_ENABLED : 0);

  if (!AdjustTokenPrivileges(
         hToken,
         FALSE,
         &tp,
         sizeof(TOKEN_PRIVILEGES),
         (PTOKEN_PRIVILEGES)NULL,
         (PDWORD)NULL))
    return FALSE;

  if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    return FALSE;

  return TRUE;
}

static DWORD ChangeOwner(const SchemeType* scheme, TCHAR* path, ChangeMode mode, Options* options, TCHAR* param)
{
  DWORD ret = 0;
  PSID pSid = NULL, pSidOwner = NULL, pSidGroup = NULL;
  SECURITY_INFORMATION what;
  HANDLE hToken;

  if (popstring(param))
    ABORT("Trustee is missing");

  if (NULL == (pSid = ParseSid(param)))
    ABORT_s("Bad trustee (%s)", param);

  switch(mode)
  {
  case ChangeMode_Owner:
    what = OWNER_SECURITY_INFORMATION;
    pSidOwner = pSid;
    break;

  case ChangeMode_Group:
    what = GROUP_SECURITY_INFORMATION;
    pSidGroup = pSid;
    break;

  default:
    ABORT_d("Bug: Unsupported change mode: %d", mode);
  }

  if (!OpenProcessToken(GetCurrentProcess(),
    TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    ABORT_d("Cannot open process token. Error code: %d", GetLastError());

  if (!SetPrivilege(hToken, SE_RESTORE_NAME, TRUE))
    ABORT("Unable to give SE_RESTORE_NAME privilege.");

  if (!SetPrivilege(hToken, SE_TAKE_OWNERSHIP_NAME, TRUE))
    ABORT("Unable to give SE_TAKE_OWNERSHIP_NAME privilege.");
  
  ret = MySetNamedSecurityInfo(scheme, path, what, pSidOwner, pSidGroup, NULL, options);
  if (ret != ERROR_SUCCESS)
    ABORT_d("Cannot apply new ownership. Error code: %d", ret);

cleanup:
  LocalFree(pSid);
  SetPrivilege(hToken, SE_RESTORE_NAME, FALSE);
  SetPrivilege(hToken, SE_TAKE_OWNERSHIP_NAME, FALSE);
  CloseHandle(hToken);
  return ret;
}

static DWORD GetOwner(const SchemeType* scheme, TCHAR* path, ChangeMode mode, TCHAR* owner, TCHAR* domain, Options* options)
{
  SECURITY_INFORMATION what;
  PSECURITY_DESCRIPTOR psd = NULL;
  PSID pSid = NULL, pSidGroup = NULL, pSidOwner = NULL;
  SID_NAME_USE eUse = SidTypeUnknown;
  DWORD dwOwner = g_string_size, dwDomain = g_string_size;
  BOOL defaulted = FALSE;
  DWORD ret = 0;

  switch(mode)
  {
  case ChangeMode_Owner:
    what = OWNER_SECURITY_INFORMATION;
    break;

  case ChangeMode_Group:
    what = GROUP_SECURITY_INFORMATION;
    break;

  default:
    ABORT_d("Bug: Unsupported change mode: %d", mode);
  }
  
  ret = MyGetNamedSecurityInfo(scheme, path, what, &psd, options);
  if (ret != ERROR_SUCCESS)
    ABORT_d("Cannot get current ownership. Error code: %d", ret);

  if (mode == OWNER_SECURITY_INFORMATION)
    GetSecurityDescriptorOwner(psd, &pSidOwner, &defaulted);
  else
    GetSecurityDescriptorGroup(psd, &pSidGroup, &defaulted);

  if (!LookupAccountSid(NULL, (pSidOwner ? pSidOwner : pSidGroup),
          owner, &dwOwner, domain, &dwDomain, &eUse))
    ABORT_d("Cannot look up owner. Error code: %d", GetLastError());

cleanup:
  LocalFree(psd);
  return ret;
}

static DWORD ClearACL(const SchemeType* scheme, TCHAR* path, Options* options, TCHAR* param)
{
  TCHAR* trusteeName = NULL;
  PSID pSid = NULL;
  DWORD trusteeForm = TRUSTEE_IS_NAME;
  DWORD permissions = 0;
  PACL pNewAcl = NULL;
  EXPLICIT_ACCESS access;

  DWORD ret = 0;

  if (popstring(param))
    ABORT("Trustee is missing");

  if (NULL == (trusteeName = ParseTrustee(param, &trusteeForm)))
    ABORT_s("Bad trustee (%s)", param);

  if (popstring(param))
    ABORT("Permission flags are missing");

  if (0 == (permissions = ParsePermissions(scheme, param)))
    ABORT_s("Bad permission flags (%s)", param);

  BuildExplicitAccessWithName(&access, TEXT(""), permissions, SET_ACCESS, scheme->defaultInheritance);

  access.Trustee.TrusteeForm = (TRUSTEE_FORM)trusteeForm;
  access.Trustee.ptstrName = trusteeName;
  if (options->noInherit)
    access.grfInheritance = NO_INHERITANCE;

  ret = SetEntriesInAcl(1, &access, NULL, &pNewAcl);
  if (ret != ERROR_SUCCESS)
    ABORT_d("Cannot build new access control list. Error code: %d", ret);
  
  ret = MySetNamedSecurityInfo(scheme, path, DACL_SECURITY_INFORMATION, NULL, NULL, pNewAcl, options);
  if (ret != ERROR_SUCCESS)
    ABORT_d("Cannot change access control list inheritance. Error code: %d", ret);

cleanup:
  LocalFree(pNewAcl);
  LocalFree(trusteeName);
  return ret;
}

/*****************************************************************************
 FILESYSTEM BASED ACL HANDLING
 *****************************************************************************/

static const SchemeType* PopFileArgs(TCHAR* path, Options* options)
{
  if (popstring(path) == 0)
  {
    DWORD attr;
    options->noInherit = FALSE;
    if (lstrcmpi(path, TEXT("/noinherit")) == 0)
    {
      options->noInherit = TRUE;
      popstring(path);
    }
    attr = GetFileAttributes(path);

    if (INVALID_FILE_ATTRIBUTES != attr)
      return FILE_ATTRIBUTE_DIRECTORY & attr
        ? g_directoryScheme
        : g_fileScheme;
    else
      ABORT("Invalid filesystem path missing");
  }
  else
    ABORT("Filesystem path missing");

cleanup:
  return NULL;
}

static void ChangeFileInheritance(BOOL inherit)
{
  DWORD ret = 1;
  TCHAR* path = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

  if (path)
  {
    const SchemeType* scheme;
    Options options;

    if (NULL != (scheme = PopFileArgs(path, &options)))
    {
        options.noInherit = !inherit;
        if (0 == (ret = ChangeInheritance(scheme, path, &options)))
          pushstring(TEXT("ok"));
    }

    LocalFree(path);
  }

  if (ret) pushstring(TEXT("error"));
}

static void ChangeFileDACL(DWORD mode)
{
  DWORD ret = 1;
  TCHAR* path = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

  if (path)
  {
    TCHAR* param = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

    if (param)
    {
      const SchemeType* scheme;
      Options options;

      if (NULL != (scheme = PopFileArgs(path, &options)))
        if (0 == (ret = ChangeDACL(scheme, path, mode, &options, param)))
          pushstring(TEXT("ok"));

      LocalFree(param);
    }

    LocalFree(path);
  }

  if (ret) pushstring(TEXT("error"));
}

static void ChangeFileOwner(ChangeMode mode)
{
  DWORD ret = 1;
  TCHAR* path = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

  if (path)
  {
    TCHAR* param = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

    if (param)
    {
      const SchemeType* scheme;
      Options options;

      if (NULL != (scheme = PopFileArgs(path, &options)))
        if (0 == (ret = ChangeOwner(scheme, path, mode, &options, param)))
          pushstring(TEXT("ok"));
    
      LocalFree(param);
    }

    LocalFree(path);
  }

  if (ret) pushstring(TEXT("error"));
}

static void PushFileOwner(ChangeMode mode)
{
  DWORD ret = 1;
  TCHAR* path = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

  if (path)
  {
    TCHAR* owner = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

    if (owner)
    {
      TCHAR* domain = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

      if (domain)
      {
        const SchemeType* scheme;
        Options options;

        if (NULL != (scheme = PopFileArgs(path, &options)))
        {
          if (0 == (ret = GetOwner(scheme, path, mode, owner, domain, &options)))
            pushstring(owner);
        }
      
        LocalFree(domain);
      }
      
      LocalFree(owner);
    }

    LocalFree(path);
  }

  if (ret) pushstring(TEXT("error"));
}

/*****************************************************************************
 REGISTRY BASED ACL HANDLING
 *****************************************************************************/

static BOOL PopRegKeyArgs(TCHAR* path, Options* options, TCHAR* param)
{
  int iRootKey;
  BOOL success = FALSE;

  if (popstring(param) == 0)
  {
    options->noInherit = FALSE;
    if (lstrcmpi(param, TEXT("/noinherit")) == 0)
    {
      options->noInherit = TRUE;
      popstring(param);
    }
  }
  else
    ABORT("Root key name missing");

  iRootKey = ParseEnum(g_rootKeyNames, param);
  if (!ARRAY_CONTAINS(g_rootKeyPrefixes, iRootKey))
    ABORT_s("Bad root key name (%s)", param);

  if (popstring(param) != 0)
    ABORT("Registry key name missing");

  if (g_extra->exec_flags->alter_reg_view & KEY_WOW64_64KEY)
  {
    options->hRootKey = g_rootKeyValues[iRootKey];
    lstrcpy(path, param);
  }
  else
  {
    lstrcpy(path, g_rootKeyPrefixes[iRootKey]);
    lstrcat(path, param);
  }

  success = TRUE;

cleanup:
  return success;
}

static void ChangeRegKeyInheritance(BOOL inherit)
{
  DWORD ret = 1;
  TCHAR* path = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

  if (path)
  {
    TCHAR* param = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

    if (param)
    {
      Options options;

      if (PopRegKeyArgs(path, &options, param))
      {
        options.noInherit = !inherit;
        if (0 == (ret = ChangeInheritance(g_registryScheme, path, &options)))
          pushstring(TEXT("ok"));
      }

      LocalFree(param);
    }

    LocalFree(path);
  }

  if (ret) pushstring(TEXT("error"));
}

static void ChangeRegKeyDACL(DWORD mode)
{
  DWORD ret = 1;
  TCHAR* path = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

  if (path)
  {
    TCHAR* param = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

    if (param)
    {
      Options options;

      if (PopRegKeyArgs(path, &options, param))
        if (0 == (ret = ChangeDACL(g_registryScheme, path, mode, &options, param)))
          pushstring(TEXT("ok"));

      LocalFree(param);
    }

    LocalFree(path);
  }

  if (ret) pushstring(TEXT("error"));
}

static void ChangeRegKeyOwner(ChangeMode mode)
{
  DWORD ret = 1;
  TCHAR* path = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

  if (path)
  {
    TCHAR* param = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

    if (param)
    {
      Options options;

      if (PopRegKeyArgs(path, &options, param))
        if (0 == (ret = ChangeOwner(g_registryScheme, path, mode, &options, param)))
          pushstring(TEXT("ok"));

      LocalFree(param);
    }

    LocalFree(path);
  }

  if (ret) pushstring(TEXT("error"));
}

static void PushRegKeyOwner(ChangeMode mode)
{
  DWORD ret = 1;
  TCHAR* path = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

  if (path)
  {
    TCHAR* owner = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

    if (owner)
    {
      TCHAR* domain = (TCHAR*)LocalAlloc(g_string_size*sizeof(TCHAR));

      if (domain)
      {
        Options options;

        if (PopRegKeyArgs(path, &options, owner))
          if (0 == (ret = GetOwner(g_registryScheme, path, mode, owner, domain, &options)))
            pushstring(owner);

        LocalFree(domain);
      }

      LocalFree(owner);
    }

    LocalFree(path);
  }

  if (ret) pushstring(TEXT("error"));
}

/*****************************************************************************
 PUBLIC FILE RELATED FUNCTIONS
 *****************************************************************************/

PUBLIC_FUNCTION(EnableFileInheritance)
  ChangeFileInheritance(TRUE);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(DisableFileInheritance)
  ChangeFileInheritance(FALSE);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(GrantOnFile)
  ChangeFileDACL(GRANT_ACCESS);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(SetOnFile)
  ChangeFileDACL(SET_ACCESS);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(DenyOnFile)
  ChangeFileDACL(DENY_ACCESS);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(RevokeOnFile)
  ChangeFileDACL(REVOKE_ACCESS);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(SetFileOwner)
  ChangeFileOwner(ChangeMode_Owner);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(SetFileGroup)
  ChangeFileOwner(ChangeMode_Group);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(GetFileOwner)
  PushFileOwner(ChangeMode_Owner);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(GetFileGroup)
  PushFileOwner(ChangeMode_Group);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(ClearOnFile)
{
  DWORD ret = 1;
  TCHAR* path = (TCHAR*)LocalAlloc(string_size*sizeof(TCHAR));

  if (path)
  {
    TCHAR* param = (TCHAR*)LocalAlloc(string_size*sizeof(TCHAR));
    
    if (param)
    {
      const SchemeType* scheme;
      Options options;

      if (NULL != (scheme = PopFileArgs(path, &options)))
        if (0 == (ret = ClearACL(scheme, path, &options, param)))
          pushstring(TEXT("ok"));

      LocalFree(param);
    }

    LocalFree(path);
  }

  if (ret) pushstring(TEXT("error"));
}
PUBLIC_FUNCTION_END

/*****************************************************************************
 PUBLIC REGISTRY RELATED FUNCTIONS
 *****************************************************************************/

PUBLIC_FUNCTION(EnableRegKeyInheritance)
  ChangeRegKeyInheritance(TRUE);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(DisableRegKeyInheritance)
  ChangeRegKeyInheritance(FALSE);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(GrantOnRegKey)
  ChangeRegKeyDACL(GRANT_ACCESS);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(SetOnRegKey)
  ChangeRegKeyDACL(SET_ACCESS);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(DenyOnRegKey)
  ChangeRegKeyDACL(DENY_ACCESS);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(RevokeOnRegKey)
  ChangeRegKeyDACL(REVOKE_ACCESS);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(SetRegKeyOwner)
  ChangeRegKeyOwner(ChangeMode_Owner);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(SetRegKeyGroup)
  ChangeRegKeyOwner(ChangeMode_Group);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(GetRegKeyOwner)
  PushRegKeyOwner(ChangeMode_Owner);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(GetRegKeyGroup)
  PushRegKeyOwner(ChangeMode_Group);
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(ClearOnRegKey)
{
  DWORD ret = 1;
  TCHAR* path = (TCHAR*)LocalAlloc(string_size*sizeof(TCHAR));

  if (path)
  {
    TCHAR* param = (TCHAR*)LocalAlloc(string_size*sizeof(TCHAR));

    if (param)
    {
      Options options;

      if (PopRegKeyArgs(path, &options, param))
        if (0 == (ret = ClearACL(g_registryScheme, path, &options, param)))
          pushstring(TEXT("ok"));

      LocalFree(param);
    }

    LocalFree(path);
  }

  if (ret) pushstring(TEXT("error"));
}
PUBLIC_FUNCTION_END

/*****************************************************************************
 OTHER ACCOUNT RELATED FUNCTIONS
 *****************************************************************************/

PUBLIC_FUNCTION(NameToSid)
{
  DWORD ret = 1;
  TCHAR *param = (TCHAR*)LocalAlloc(string_size*sizeof(TCHAR)), *retstr = ERRSTR_OOM;

  if (param)
  {
    if (popstring(param) == 0)
    {
      DWORD dwSid = 0;
      DWORD dwDomain = 0;
      SID_NAME_USE eUse;
      LookupAccountName(NULL, param, NULL, &dwSid, NULL, &dwDomain, &eUse);

      if (dwSid > 0)
      {
        PSID pSid = (PSID)LocalAlloc(dwSid);
        if (pSid)
        {
          TCHAR* domain = (TCHAR*)LocalAlloc(dwDomain*sizeof(TCHAR));
          if (domain)
          {
            retstr = param;
            if (!LookupAccountName(NULL, param, pSid, &dwSid, domain, &dwDomain, &eUse) || !ConvertSidToStringSidNoAlloc(pSid, param))
              wsprintf(param, TEXT("Cannot look up name. Error code: %d"), GetLastError());
            else
              ret = 0;
            LocalFree(domain);
          }
          LocalFree(pSid);
        }
      }
    }
  }
  pushstring(retstr);
  LocalFree(param);

  if (ret) pushstring(TEXT("error"));
}
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(SidToName)
{
  DWORD ret = 1;
  TCHAR *param = (TCHAR*)LocalAlloc(string_size*sizeof(TCHAR));

  if (param)
  {
    DWORD dwName = string_size;
    TCHAR* name = (TCHAR*)LocalAlloc(dwName*sizeof(TCHAR));

    if (name)
    {
      DWORD dwDomain = string_size;
      TCHAR* domain = (TCHAR*)LocalAlloc(dwDomain*sizeof(TCHAR));

      if (domain)
      {
        if (popstring(param) == 0)
        {
          PSID pSid = NULL;
          SID_NAME_USE eUse;
          if (!ConvertStringSidToSid(param, &pSid) || !LookupAccountSid(NULL, pSid, name, &dwName, domain, &dwDomain, &eUse))
          {
            wsprintf(param, TEXT("Cannot look up SID. Error code: %d"), GetLastError());
            pushstring(param); // BUGBUG: What if LocalAlloc fails and we never get here?
          }
          else
          {
            pushstring(name);
            pushstring(domain);
            ret = 0;
          }
          LocalFree(pSid);
        }
        LocalFree(domain);
      }
      LocalFree(name);
    }
    LocalFree(param);
  }

  if (ret) pushstring(TEXT("error"));
}
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(GetCurrentUserName)
{
  TCHAR *name = (TCHAR*)LocalAlloc(string_size*sizeof(TCHAR)), *retstr = TEXT("error");
  DWORD dwName = string_size;
  if (name && GetUserName(name, &dwName)) retstr = name;
  pushstring(retstr);
  LocalFree(name);
}
PUBLIC_FUNCTION_END

#ifdef _VC_NODEFAULTLIB
#define DllMain _DllMainCRTStartup
#endif
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
  g_hInstance = (HINSTANCE)hInst;
  return TRUE;
}