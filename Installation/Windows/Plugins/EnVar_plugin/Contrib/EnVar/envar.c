/*
 * EnVar plugin for NSIS
 *
 * 2014-2016, 2018, 2020-2021 MouseHelmet Software.
 *
 * Created By Jason Ross aka JasonFriday13 on the forums
 *
 * Checks, adds and removes paths to environment variables.
 *
 * envar.c
 */

/* Include relevent files. */
#include <windows.h>
#include "nsis\pluginapi.h" /* This means NSIS 2.42 or higher is required. */

/* Registry defines. */
#define HKCU HKEY_CURRENT_USER
#define HKCU_STR _T("Environment")
#define HKLM HKEY_LOCAL_MACHINE
#define HKLM_STR _T("System\\CurrentControlSet\\Control\\Session Manager\\Environment")

/* I would have used ints, but removing pushint() also
   removes a dependency on wsprintf and user32.dll. */
#define ERR_SUCCESS _T("0")
#define ERR_NOMEMALLOC _T("1")
#define ERR_NOREAD _T("2")
#define ERR_NOVARIABLE _T("3")
#define ERR_NOTYPE _T("4")
#define ERR_NOVALUE _T("5")
#define ERR_NOWRITE _T("6")

/* The amount of extra room to allocate to prevent overflows. */
#define APPEND_SIZE (4 * sizeof(TCHAR))

/* Unicode and odd value finder. */
#define IS_UNICODE_AND_ODD(x) ((sizeof(TCHAR) > 1 && (x) & 0x1) ? 1 : 0)

/* Global declarations. */
BOOL bRegKeyHKLM = FALSE;
HINSTANCE hInstance;

HGLOBAL RawAlloc(SIZE_T bytes)
{
  return GlobalAlloc(GPTR, bytes);
}

/* Allocates a string. */
PTCHAR StrAlloc(SIZE_T strlen)
{
  return (PTCHAR)RawAlloc(strlen*sizeof(TCHAR));
}

/* Frees a string. */
void RawFree(HGLOBAL hVar)
{
  if (hVar) GlobalFree(hVar);
}

/* Returns the string size. */
int StrSize(PTCHAR hVar)
{
  return hVar ? (int)((GlobalSize(hVar)-IS_UNICODE_AND_ODD(GlobalSize(hVar)))/sizeof(TCHAR)) : 0;
}

/* Returns the string length. */
int StrLen(LPCTSTR hVar)
{
  return lstrlen(hVar);
}

void StrCopy(PTCHAR dest, LPCTSTR src, int len)
{
  int i;

  if (!dest || !src) return;

  for (i = 0; i < len; i++)
    dest[i] = src[i];
}

/* Appends a semi-colon to a string. */
BOOL AppendSemiColon(PTCHAR bufStr)
{
  int len;
  
  if (!bufStr) return FALSE;
  len = StrLen(bufStr);
  if (!len) return TRUE;
  if (bufStr[len-1] != ';' && bufStr[0] != ';')
  {
    /* Note: APPEND_SIZE should allow this with no buffer overrun. */
    bufStr[len] = ';';
    bufStr[len+1] = 0;
  }
  return TRUE;
}

/* Removes the trailing semi-colon if it exists. */
void RemoveSemiColon(PTCHAR bufStr)
{
  if (!bufStr) return;
  if (StrLen(bufStr) < 1) return;
  if (bufStr[StrLen(bufStr)-1] == ';') bufStr[StrLen(bufStr)-1] = 0;
}

/* Double-linked list to hold each value. */
struct VARITEM
{
  PTCHAR string;
  struct VARITEM *next;
  struct VARITEM *prev;
};

struct VARLIST
{
  struct VARITEM head;
  struct VARITEM tail;
};

//void InitVarList(struct VARLIST *p_head, struct VARLIST *p_tail)
void InitVarList(struct VARLIST *p_list)
{
  p_list->head.next = &p_list->tail;
  p_list->head.prev = NULL;
  p_list->head.string = NULL;

  p_list->tail.next = NULL;
  p_list->tail.prev = &p_list->head;
  p_list->tail.string = NULL;
}

void CleanVarList(struct VARLIST *p_list)
{
  struct VARITEM *count = p_list->head.next;

  if (count == NULL) return;

  while (count != &p_list->tail)
  {
    struct VARITEM *item = count;

    if (count->string) RawFree(count->string);

    count->prev->next = count->next;
    count->next->prev = count->prev;
    count = count->next;
    RawFree(item);
  }
}

struct VARITEM *FindStrData(LPCTSTR str, struct VARLIST *p_list)
{
  struct VARITEM *count = p_list->head.next;

  while (count != &p_list->tail && lstrcmpi(count->string, str) != 0)
  {
    count = count->next;
  }
  return count == &p_list->tail ? NULL : count;
}

int GetStrDataLen(struct VARLIST *p_list)
{
  int len = 0;
  struct VARITEM *count = p_list->head.next;

  while (count != &p_list->tail)
  {
    len += StrLen(count->string);
    count = count->next;
  }
  return len;
}

BOOL AppendStrData(LPCTSTR str, int len, struct VARLIST *p_list)
{
  struct VARITEM *pThisData;

  pThisData = RawAlloc(sizeof(struct VARITEM));
  if (!pThisData)
    return FALSE;

  pThisData->string = StrAlloc(len+APPEND_SIZE);
  if (!pThisData->string)
    return FALSE;

  StrCopy(pThisData->string, str, len);
  pThisData->string[len] = 0;
  AppendSemiColon(pThisData->string);
  pThisData->next = &p_list->tail;
  pThisData->prev = p_list->tail.prev;
  p_list->tail.prev->next = pThisData;
  p_list->tail.prev = pThisData;

  return TRUE;
}

void RemoveStrData(struct VARITEM *p_item)
{
  if (p_item)
  {
    p_item->prev->next = p_item->next;
    p_item->next->prev = p_item->prev;

    if (p_item->string) RawFree(p_item->string);
    RawFree(p_item);
  }
}

BOOL ParseAndAddStrData(PTCHAR data, struct VARLIST *p_list)
{
  int start = 0, length = 0;
  
  if (!data) return TRUE;
  if (*data != 0) AppendSemiColon(data);

  /* Remove starting semicolons and/or whitespace */
  while (data[start] == ';' || data[start] == ' ')
    start++;

  while (data[start+length] && start + length <= StrSize(data))
  {
    if (data[start+length] == ';' || data[start+length] == 0)
    {
      if (!AppendStrData(&data[start], length, p_list))
        return FALSE;

      start += length + 1; /* skip the semi-colon */
      length = 0;
    }
    else
    {
      length++;
    }
  }   
  return TRUE;
}

PTCHAR GetAllStrData(struct VARLIST *p_list)
{
  int pos = 0, length = 0, len = GetStrDataLen(p_list);
  PTCHAR out = StrAlloc(len+APPEND_SIZE);
  struct VARITEM *count = p_list->head.next;

  if (!out) return NULL;

  while (count != &p_list->tail)
  {
    length = StrLen(count->string);
    StrCopy(out+pos, count->string, length);
    pos += length;
    count = count->next;
  }
  out[pos] = 0;
  return out;
}

void Notify(void)
{
  SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, (WPARAM)NULL, (LPARAM)HKCU_STR, 0, 100, 0);
}

/*  Our callback function so that our dll stays loaded. */
UINT_PTR __cdecl NSISPluginCallback(enum NSPIM Event) 
{
  if (Event == NSPIM_UNLOAD) Notify();

  return 0;
}

/* Sets the current root key. */
void SetRegKey(BOOL bKeyHKLM)
{
  bRegKeyHKLM = bKeyHKLM;
}

/* Gets the current root key. */
BOOL GetRegKey(void)
{
  return bRegKeyHKLM;
}

/* Registry helper functions. */
ULONG CreateRegKey(void)
{
  DWORD dwRet, dwDisType = 0;
  HKEY hKey;

  if (bRegKeyHKLM)
    dwRet = RegCreateKeyEx(HKLM, HKLM_STR, 0, 0, 0, KEY_WRITE, 0, &hKey, &dwDisType);
  else
    dwRet = RegCreateKeyEx(HKCU, HKCU_STR, 0, 0, 0, KEY_WRITE, 0, &hKey, &dwDisType);

  RegCloseKey(hKey);

  return dwRet;
}

/* Read from the registry and return a buffer with the data. */
PTCHAR ReadRegVar(LPCTSTR ptName, PDWORD pdwType, PDWORD pdwStrLen, PDWORD pdwRet)
{
  DWORD dwRet, dwSize = 0, dwType = 0;
  HKEY hKey;
  PTCHAR ptDest = NULL;

  if (bRegKeyHKLM)
    dwRet = RegOpenKeyEx(HKLM, HKLM_STR, 0, KEY_READ, &hKey);
  else
    dwRet = RegOpenKeyEx(HKCU, HKCU_STR, 0, KEY_READ, &hKey);

  if (dwRet == ERROR_SUCCESS)
  {
    dwRet = RegQueryValueEx(hKey, ptName, 0, &dwType, NULL, NULL);
    if (dwRet != ERROR_FILE_NOT_FOUND)
    {
      dwRet = RegQueryValueEx(hKey, ptName, 0, NULL, NULL, &dwSize);
      if (dwRet == ERROR_SUCCESS)
      {
        DWORD dwSizeTemp = dwSize + APPEND_SIZE + IS_UNICODE_AND_ODD(dwSize);

        ptDest = StrAlloc(dwSizeTemp);
        if (ptDest)
        {
          dwRet = RegQueryValueEx(hKey, ptName, 0, NULL, (LPBYTE)ptDest, &dwSizeTemp);
          ptDest[(dwRet == ERROR_SUCCESS) ? ((dwSizeTemp+IS_UNICODE_AND_ODD(dwSizeTemp))/sizeof(TCHAR)) : 0] = 0;
          dwSize = dwSizeTemp;
        }
        else
        {
          *pdwStrLen = 0;
          dwRet = GetLastError();
        }
      }
    }
  }
  RegCloseKey(hKey);
  if (pdwType) *pdwType = dwType;
  if (pdwStrLen) *pdwStrLen = ptDest != NULL ? (dwSize+IS_UNICODE_AND_ODD(dwSize))/sizeof(TCHAR) : 0;
  if (pdwRet) *pdwRet = dwRet;
  return ptDest;
}

/* Custom WriteRegVar function, writes a value to the environment. */
DWORD WriteRegVar(LPCTSTR ptName, DWORD dwKeyType, PTCHAR ptData, DWORD dwStrLen)
{
  DWORD dwRet;
  HKEY hKey;

  if (bRegKeyHKLM)
    dwRet = RegOpenKeyEx(HKLM, HKLM_STR, 0, KEY_WRITE, &hKey);
  else
    dwRet = RegOpenKeyEx(HKCU, HKCU_STR, 0, KEY_WRITE, &hKey);

  if (dwRet != ERROR_SUCCESS) return dwRet;
  dwRet = RegSetValueEx(hKey, ptName, 0, dwKeyType, (LPBYTE)ptData, dwStrLen*sizeof(TCHAR));
  RegCloseKey(hKey);

  return dwRet;
}

/* Checks for write access and various conditions about a variable and it's type. */
LPCTSTR CheckVar(LPCTSTR ptcVarName, PTCHAR ptPathString, struct VARLIST *p_list)
{
  DWORD dwStrSize, dwKeyType, dwRet;
  HKEY hKeyHandle;
  PTCHAR ptBuffer;
  struct VARITEM *item;

  if (!StrLen(ptcVarName)) return ERR_NOVARIABLE;
  if (lstrcmpi(ptcVarName, _T("NULL")) == 0)
  {
    DWORD dwRet;

    if (bRegKeyHKLM)
      dwRet = RegOpenKeyEx(HKLM, HKLM_STR, 0, KEY_WRITE, &hKeyHandle);
    else
      dwRet = RegOpenKeyEx(HKCU, HKCU_STR, 0, KEY_WRITE, &hKeyHandle);

    if (dwRet != ERROR_SUCCESS) return ERR_NOWRITE;
    RegCloseKey(hKeyHandle);
    return ERR_SUCCESS;
  }
  ptBuffer = ReadRegVar(ptcVarName, &dwKeyType, &dwStrSize, &dwRet);
  if (dwRet != ERROR_SUCCESS)
  {
    RawFree(ptBuffer), ptBuffer = NULL;
    return ERR_NOVARIABLE;
  }
  if (!StrLen(ptPathString))
  {
    RawFree(ptBuffer), ptBuffer = NULL;
    return ERR_NOVARIABLE;
  }
  if (lstrcmpi(ptPathString, _T("NULL")) == 0)
  {
    RawFree(ptBuffer), ptBuffer = NULL;
    if (dwKeyType != REG_SZ && dwKeyType != REG_EXPAND_SZ)
      return ERR_NOTYPE;
    else
      return ERR_SUCCESS;
  }
  else
  {
    BOOL dwRet = ParseAndAddStrData(ptBuffer, p_list);

    RawFree(ptBuffer), ptBuffer = NULL;
    if (!dwRet) return ERR_NOMEMALLOC;
    if (!AppendSemiColon(ptPathString)) return ERR_NOWRITE;
    item = FindStrData(ptPathString, p_list);
    if (item == NULL)
      return ERR_NOVALUE;
    else
      return ERR_SUCCESS;
  }
}

/* Adds a value to a variable if it's the right type. */
LPCTSTR AddVarValue(LPCTSTR ptcVarName, PTCHAR ptPathString, DWORD dwKey, struct VARLIST *p_list)
{
  DWORD dwStrSize, dwKeyType, dwRet;
  PTCHAR ptBuffer = NULL;

  if (!StrLen(ptPathString)) return ERR_NOVALUE;

  if (CreateRegKey() != ERROR_SUCCESS) return ERR_NOWRITE;

  ptBuffer = ReadRegVar(ptcVarName, &dwKeyType, &dwStrSize, &dwRet);
  if (dwRet == ERROR_SUCCESS && dwKeyType != REG_SZ && dwKeyType != REG_EXPAND_SZ)
  {
    RawFree(ptBuffer), ptBuffer = NULL;
    return ERR_NOTYPE;
  }
  else if (dwRet == ERROR_FILE_NOT_FOUND)
  {
    TCHAR temp1 = 0;
    dwRet = WriteRegVar(ptcVarName, dwKey, &temp1, 1);
    dwKeyType = dwKey;
    RawFree(ptBuffer), ptBuffer = NULL;
  }
  if (dwKeyType == REG_EXPAND_SZ) dwKey = dwKeyType;
  if (!AppendSemiColon(ptPathString))
  {
    RawFree(ptBuffer), ptBuffer = NULL;
    return ERR_NOWRITE;
  }
  dwRet = ParseAndAddStrData(ptBuffer, p_list);
  RawFree(ptBuffer), ptBuffer = NULL;
  if (!dwRet) return ERR_NOMEMALLOC;
  if (FindStrData(ptPathString, p_list) == NULL)
  {
    DWORD dwRet = AppendStrData(ptPathString, StrLen(ptPathString), p_list);
    if (!dwRet) return ERR_NOMEMALLOC;
    ptBuffer = GetAllStrData(p_list);
    if (ptBuffer == NULL) return ERR_NOMEMALLOC;
    RemoveSemiColon(ptBuffer);
    dwRet = WriteRegVar(ptcVarName, dwKey, ptBuffer, StrLen(ptBuffer)+1);
    RawFree(ptBuffer), ptBuffer = NULL;
    if (dwRet != ERROR_SUCCESS) return ERR_NOWRITE;
  }
  return ERR_SUCCESS;
}

/* Deletes a value from a variable if it's the right type. */
LPCTSTR DeleteVarValue(LPCTSTR ptcVarName, PTCHAR ptPathString, struct VARLIST *p_list)
{
  DWORD dwStrSize, dwKeyType, dwRet;
  PTCHAR ptBuffer = NULL;
  struct VARITEM *item;

  if (!AppendSemiColon(ptPathString)) return ERR_NOWRITE;
  ptBuffer = ReadRegVar(ptcVarName, &dwKeyType, &dwStrSize, &dwRet);
  if (dwRet != ERROR_SUCCESS)
  {
    RawFree(ptBuffer), ptBuffer = NULL;
    return ERR_NOVARIABLE;
  }
  if (dwRet == ERROR_SUCCESS && dwKeyType != REG_SZ && dwKeyType != REG_EXPAND_SZ)
  {
    RawFree(ptBuffer), ptBuffer = NULL;
    return ERR_NOTYPE;
  }  dwRet = ParseAndAddStrData(ptBuffer, p_list);
  RawFree(ptBuffer), ptBuffer = NULL;
  if (!dwRet) return ERR_NOMEMALLOC;

  if ((item = FindStrData(ptPathString, p_list)) == NULL)
    return ERR_NOVALUE;

  do
  {
    RemoveStrData(item);
  }
  while ((item = FindStrData(ptPathString, p_list)) != NULL);
  ptBuffer = GetAllStrData(p_list);
  if (ptBuffer == NULL)
  {
    ptBuffer = StrAlloc(1);
    if (ptBuffer == NULL) return ERR_NOMEMALLOC;
    *ptBuffer = 0;
  }
  RemoveSemiColon(ptBuffer);
  dwRet = WriteRegVar(ptcVarName, dwKeyType, ptBuffer, StrLen(ptBuffer)+1);
  RawFree(ptBuffer), ptBuffer = NULL;
  if (dwRet != ERROR_SUCCESS) return ERR_NOWRITE;

  return ERR_SUCCESS;
}

/* Deletes a variable from the environment. */
LPCTSTR DeleteVar(LPCTSTR ptcVarName)
{
  DWORD dwRet, dwStrSize, dwKeyType = 0;
  HKEY hKey;

  if (!lstrcmpi(ptcVarName, _T("path"))) return ERR_NOWRITE;

  RawFree(ReadRegVar(ptcVarName, &dwKeyType, &dwStrSize, &dwRet));
  if (dwKeyType != REG_SZ && dwKeyType != REG_EXPAND_SZ)
    return ERR_NOTYPE;

  if (bRegKeyHKLM)
    dwRet = RegOpenKeyEx(HKLM, HKLM_STR, 0, KEY_WRITE, &hKey);
  else
    dwRet = RegOpenKeyEx(HKCU, HKCU_STR, 0, KEY_WRITE, &hKey);

  if (dwRet != ERROR_SUCCESS) return ERR_NOWRITE;

  dwRet = RegDeleteValue(hKey, ptcVarName);
  RegCloseKey(hKey);
  if (dwRet == ERROR_SUCCESS)
    return ERR_SUCCESS;
  else
    return ERR_NOWRITE;
}

/* Updates the installer environment from the registry. */
LPCTSTR UpdateVar(LPCTSTR ptcRegRoot, LPCTSTR ptcVarName, struct VARLIST *p_list)
{
  PTCHAR ptBuffer;
  DWORD dwRet, dwStrSize, dwKeyType;
  BOOL bOldKey = GetRegKey();

  if (lstrcmpi(ptcRegRoot, _T("HKCU")) == 0 || lstrcmpi(ptcRegRoot, _T("HKLM")) == 0)
  {
    if (lstrcmpi(ptcRegRoot, _T("HKLM")) == 0)
      SetRegKey(TRUE);
    else
      SetRegKey(FALSE);

    ptBuffer = ReadRegVar(ptcVarName, &dwKeyType, &dwStrSize, &dwRet);
    SetRegKey(bOldKey);
    if (dwRet != ERROR_SUCCESS) RawFree(ptBuffer), ptBuffer = NULL;
    if (dwRet == ERROR_NOT_ENOUGH_MEMORY) return ERR_NOMEMALLOC;
    if (dwRet == ERROR_FILE_NOT_FOUND) return ERR_NOVARIABLE;
    if (dwRet == ERROR_SUCCESS && dwKeyType != REG_SZ && dwKeyType != REG_EXPAND_SZ)
      return ERR_NOTYPE;
  }
  else if (lstrcmpi(ptcRegRoot, _T("NULL")) == 0)
  {
    ptBuffer = NULL;
  }
  else
  {
    int Alloc = 2, Found = 2, Type = 2;
    SetRegKey(FALSE);
    ptBuffer = ReadRegVar(ptcVarName, &dwKeyType, &dwStrSize, &dwRet);
    if (dwRet == ERROR_NOT_ENOUGH_MEMORY) RawFree(ptBuffer), ptBuffer = NULL, Alloc--;
    if (dwRet == ERROR_FILE_NOT_FOUND) Found--;
    if (dwRet == ERROR_SUCCESS)
    {
      if (dwKeyType != REG_SZ && dwKeyType != REG_EXPAND_SZ)
        Type--;
      else
        ParseAndAddStrData(ptBuffer, p_list);
    }
    RawFree(ptBuffer);
    SetRegKey(TRUE);
    ptBuffer = ReadRegVar(ptcVarName, &dwKeyType, &dwStrSize, &dwRet);
    SetRegKey(bOldKey);
    if (dwRet == ERROR_NOT_ENOUGH_MEMORY) RawFree(ptBuffer), ptBuffer = NULL, Alloc--;
    if (dwRet == ERROR_FILE_NOT_FOUND) Found--;
    if (dwRet == ERROR_SUCCESS)
    {
      if (dwKeyType != REG_SZ && dwKeyType != REG_EXPAND_SZ)
        Type--;
      else
        ParseAndAddStrData(ptBuffer, p_list);
    }
    RawFree(ptBuffer);
    if (!Alloc) return ERR_NOMEMALLOC;
    if (!Found) return ERR_NOVARIABLE;
    if (!Type) return ERR_NOTYPE;
    ptBuffer = GetAllStrData(p_list);
    if (ptBuffer == NULL)
    {
      ptBuffer = StrAlloc(1);
      if (ptBuffer == NULL) return ERR_NOMEMALLOC;
      *ptBuffer = 0;
    }
  }
  RemoveSemiColon(ptBuffer);
  dwRet = SetEnvironmentVariable(ptcVarName, ptBuffer);
  RawFree(ptBuffer);
  if (!dwRet) return ERR_NOWRITE;

  return ERR_SUCCESS;
}

/* This routine sets the environment root, HKCU. */
__declspec(dllexport) void SetHKCU(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, extra_parameters* xp)
{
  /* Initialize the stack so we can access it from our DLL using 
  popstring and pushstring. */
  EXDLL_INIT();
  xp->RegisterPluginCallback(hInstance, NSISPluginCallback);

  SetRegKey(FALSE);
}

/* This routine sets the environment root, HKLM. */
__declspec(dllexport) void SetHKLM(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, extra_parameters* xp)
{
  /* Initialize the stack so we can access it from our DLL using 
  popstring and pushstring. */
  EXDLL_INIT();
  xp->RegisterPluginCallback(hInstance, NSISPluginCallback);

  SetRegKey(TRUE);
}

/* This routine checks for a path in an environment variable. */
__declspec(dllexport) void Check(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, extra_parameters* xp)
{
  struct VARLIST list;
  PTCHAR ptVarName = StrAlloc(g_stringsize+APPEND_SIZE);
  PTCHAR ptPathString = StrAlloc(g_stringsize+APPEND_SIZE);

  /* Initialize the stack so we can access it from our DLL using 
  popstring and pushstring. */
  EXDLL_INIT();
  xp->RegisterPluginCallback(hInstance, NSISPluginCallback);

  InitVarList(&list);
  popstring(ptVarName);
  popstring(ptPathString);

  pushstring(CheckVar(ptVarName, ptPathString, &list));

  RawFree(ptVarName);
  RawFree(ptPathString);
  CleanVarList(&list);
}

/* This routine adds a REG_SZ value in a environment variable (checks for existing paths first). */
__declspec(dllexport) void AddValue(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, extra_parameters* xp)
{
  struct VARLIST list;
  PTCHAR ptVarName = StrAlloc(g_stringsize+APPEND_SIZE);
  PTCHAR ptPathString = StrAlloc(g_stringsize+APPEND_SIZE);

  /* Initialize the stack so we can access it from our DLL using 
  popstring and pushstring. */
  EXDLL_INIT();
  xp->RegisterPluginCallback(hInstance, NSISPluginCallback);

  InitVarList(&list);
  popstring(ptVarName);
  popstring(ptPathString);

  pushstring(AddVarValue(ptVarName, ptPathString, REG_SZ, &list));

  RawFree(ptVarName);
  RawFree(ptPathString);
  CleanVarList(&list);
}

/* This routine adds a REG_EXPAND_SZ value in a environment variable (checks for existing paths first). */
__declspec(dllexport) void AddValueEx(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, extra_parameters* xp)
{
  struct VARLIST list;
  PTCHAR ptVarName = StrAlloc(g_stringsize+APPEND_SIZE);
  PTCHAR ptPathString = StrAlloc(g_stringsize+APPEND_SIZE);

  /* Initialize the stack so we can access it from our DLL using 
  popstring and pushstring. */
  EXDLL_INIT();
  xp->RegisterPluginCallback(hInstance, NSISPluginCallback);

  InitVarList(&list);
  popstring(ptVarName);
  popstring(ptPathString);

  pushstring(AddVarValue(ptVarName, ptPathString, REG_EXPAND_SZ, &list));

  RawFree(ptVarName);
  RawFree(ptPathString);
  CleanVarList(&list);
}

/* This routine deletes a value in an environment variable if it exists. */
__declspec(dllexport) void DeleteValue(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, extra_parameters* xp)
{
  struct VARLIST list;
  PTCHAR ptVarName = StrAlloc(g_stringsize+APPEND_SIZE);
  PTCHAR ptPathString = StrAlloc(g_stringsize+APPEND_SIZE);

  /* Initialize the stack so we can access it from our DLL using 
  popstring and pushstring. */
  EXDLL_INIT();
  xp->RegisterPluginCallback(hInstance, NSISPluginCallback);

  InitVarList(&list);
  popstring(ptVarName);
  popstring(ptPathString);

  pushstring(DeleteVarValue(ptVarName, ptPathString, &list));

  RawFree(ptVarName);
  RawFree(ptPathString);
  CleanVarList(&list);
}

/* This routine deletes an environment variable if it exists. */
__declspec(dllexport) void Delete(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, extra_parameters* xp)
{
  struct VARLIST list;
  PTCHAR ptVarName = StrAlloc(g_stringsize+APPEND_SIZE);

  /* Initialize the stack so we can access it from our DLL using 
  popstring and pushstring. */
  EXDLL_INIT();
  xp->RegisterPluginCallback(hInstance, NSISPluginCallback);

  InitVarList(&list);
  popstring(ptVarName);

  pushstring(DeleteVar(ptVarName));

  RawFree(ptVarName);
  CleanVarList(&list);
}

/* This routine reads the registry and updates the process environment. */
__declspec(dllexport) void Update(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, extra_parameters* xp)
{
  struct VARLIST list;
  PTCHAR ptRegRoot = StrAlloc(g_stringsize+APPEND_SIZE);
  PTCHAR ptVarName = StrAlloc(g_stringsize+APPEND_SIZE);

  /* Initialize the stack so we can access it from our DLL using 
  popstring and pushstring. */
  EXDLL_INIT();
  xp->RegisterPluginCallback(hInstance, NSISPluginCallback);

  InitVarList(&list);
  popstring(ptRegRoot);
  popstring(ptVarName);

  pushstring(UpdateVar(ptRegRoot, ptVarName, &list));

  RawFree(ptRegRoot);
  RawFree(ptVarName);
  CleanVarList(&list);
}

/* Our DLL entry point, this is called when we first load up our DLL. */
BOOL WINAPI _DllMainCRTStartup(HINSTANCE hInst, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  hInstance = hInst;

  if (ul_reason_for_call == DLL_PROCESS_DETACH)
    Notify();

  return TRUE;
}
