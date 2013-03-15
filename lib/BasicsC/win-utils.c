////////////////////////////////////////////////////////////////////////////////
/// @brief some utilities for windows
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <io.h>
#include "win-utils.h"
#include "BasicsC/logging.h"
#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <crtdbg.h>

/*
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <share.h>
*/




// .............................................................................
// Some global variables which may be required throughtout the lifetime of the
// server
// .............................................................................

_invalid_parameter_handler oldInvalidHandleHandler;
_invalid_parameter_handler newInvalidHandleHandler;

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Windows_Utilties
/// @{
////////////////////////////////////////////////////////////////////////////////

int ftruncate(int fd, long newSize) {
  int result = _chsize(fd, newSize);
  return result;
}


int getpagesize(void) {
  static int pageSize = 0; // only define it once

  if (!pageSize) {
    // first time, so call the system info function
    SYSTEM_INFO systemInfo;
    GetSystemInfo (&systemInfo);
    pageSize = systemInfo.dwPageSize;
  }

  return pageSize;
}


////////////////////////////////////////////////////////////////////////////////
// Calls the windows Sleep function which always sleeps for milliseconds
////////////////////////////////////////////////////////////////////////////////

void TRI_sleep(unsigned long waitTime) {
  Sleep(waitTime * 1000);
}


////////////////////////////////////////////////////////////////////////////////
// Calls a timer which waits for a signal after the elapsed time.
// The timer is acurate to 100nanoseconds
////////////////////////////////////////////////////////////////////////////////

void TRI_usleep(unsigned long waitTime) {
  int result;
  HANDLE hTimer = NULL;    // stores the handle of the timer object
  LARGE_INTEGER wTime;    // essentially a 64bit number
  wTime.QuadPart = waitTime * 10; // *10 to change to microseconds
  wTime.QuadPart = -wTime.QuadPart;  // negative indicates relative time elapsed,

  // Create an unnamed waitable timer.
  hTimer = CreateWaitableTimer(NULL, 1, NULL);
  if (hTimer == NULL) {
    // no much we can do at this low level
    return;
  }

  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    abort();
  }
  // Set timer to wait for indicated micro seconds.
  if (!SetWaitableTimer(hTimer, &wTime, 0, NULL, NULL, 0)) {
    // no much we can do at this low level
    CloseHandle(hTimer);
    return;
  }

  // Wait for the timer
  result = WaitForSingleObject(hTimer, INFINITE);
  if (result != WAIT_OBJECT_0) {
    CloseHandle(hTimer);
    abort();
  }

  CloseHandle(hTimer);
  // todo: go through what the result is e.g. WAIT_OBJECT_0
  return;
}



////////////////////////////////////////////////////////////////////////////////
// Sets up a handler when invalid (win) handles are passed to a windows function.
// This is not of much use since no values can be returned. All we can do
// for now is to ignore error and hope it goes away!
////////////////////////////////////////////////////////////////////////////////

static void InvalidParameterHandler(const wchar_t* expression, // expression sent to function - NULL
                                    const wchar_t* function,   // name of function - NULL
                                    const wchar_t* file,       // file where code resides - NULL
                                    unsigned int line,         // line within file - NULL
                                    uintptr_t pReserved) {     // in case microsoft forget something
  LOG_ERROR("Invalid handle parameter passed");

  /* start oreste -debug */
  if (expression != 0) {
    wprintf(L"win-utils.c:InvalidParameterHandler:EXPRESSION = %s\n",expression);
  }
  else {
    wprintf(L"win-utils.c:InvalidParameterHandler:EXPRESSION = NULL\n");
  }
  if (function != 0) {
    wprintf(L"win-utils.c:InvalidParameterHandler:FUNCTION = %s\n",function);
  }
  else {
    wprintf(L"win-utils.c:InvalidParameterHandler:FUNCTION = NULL\n");
  }
  if (file!= 0) {
    wprintf(L"win-utils.c:InvalidParameterHandler:FILE = %s\n",file);
  }
  else {
    wprintf(L"win-utils.c:InvalidParameterHandler:FILE = NULL\n");
  }
  printf("oreste:win-utils.c:InvalidParameterHandler:LINE = %ud\n",line);
  /* end oreste -debug */
  //abort();
  // TODO: use the wcstombs_s function to convert wchar to char - since all the above
  // wchar never will contain 2 byte chars
}


////////////////////////////////////////////////////////////////////////////////
// Called from the 'main' and performs any initialisation requirements which
// are specific to windows.
//
// If this function returns 0, then no errors encountered. If < 0, then the
// calling function should terminate the application. If > 0, then the
// calling function should decide what to do.
////////////////////////////////////////////////////////////////////////////////


int finaliseWindows(const TRI_win_finalise_e finaliseWhat, const char* data) {
  int result = 0;

  // ............................................................................
  // The data is used to transport information from the calling function to here
  // it may be NULL (and will be in most cases)
  // ............................................................................

  switch (finaliseWhat) {

    case TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL: {
      result = WSACleanup();     // could this cause error on server termination?
      if (result != 0) {
        // can not use LOG_ etc here since the logging may have terminated
        printf("ERROR: Could not perform a valid Winsock2 cleanup. WSACleanup returned error %d.",result);
        return -1;
      }
      return 0;
    }

    default: {
      // can not use LOG_ etc here since the logging may have terminated
      printf("ERROR: Invalid windows finalisation called");
      return -1;
    }
  }

  return -1;
}



int initialiseWindows(const TRI_win_initialise_e initialiseWhat, const char* data) {


  // ............................................................................
  // The data is used to transport information from the calling function to here
  // it may be NULL (and will be in most cases)
  // ............................................................................

  switch (initialiseWhat) {

    case TRI_WIN_INITIAL_SET_DEBUG_FLAG: {
      _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF)|_CRTDBG_CHECK_ALWAYS_DF);
      return 0;
    }

    // ...........................................................................
    // Assign a handler for invalid handles
    // ...........................................................................

    case TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER: {
      newInvalidHandleHandler = InvalidParameterHandler;
      oldInvalidHandleHandler = _set_invalid_parameter_handler(newInvalidHandleHandler);
      return 0;
    }

    case TRI_WIN_INITIAL_SET_MAX_STD_IO: {
      int* newMax = (int*)(data);
      int result = _setmaxstdio(*newMax);
      if (result != *newMax) {
        return -1;
      }
      return 0;
    }

    case TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL: {
      int errorCode;
      WSADATA wsaData;
      WORD wVersionRequested = MAKEWORD(2,2);
      errorCode = WSAStartup(wVersionRequested, &wsaData);

      if (errorCode != 0) {
        LOG_ERROR("Could not find a usuable Winsock DLL. WSAStartup returned an error.");
        return -1;
      }
      if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        LOG_ERROR("Could not find a usuable Winsock DLL. WSAStartup did not return version 2.2.");
        WSACleanup();
        return -1;
      }
      return 0;
    }

    default: {
      LOG_ERROR("Invalid windows initialisation called");
      return -1;
    }
  }

  return -1;

}


int TRI_createFile (const char* filename, int openFlags, int modeFlags) {
  HANDLE fileHandle;
  int    fileDescriptor;

  fileHandle = CreateFileA(filename,
                          GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          CREATE_NEW,
                          0,
                          NULL);


  if (fileHandle == INVALID_HANDLE_VALUE) {
    return -1;
  }

  fileDescriptor = _open_osfhandle( (intptr_t)(fileHandle), O_RDWR| _O_BINARY);
  return fileDescriptor;

/*
#define O_RDONLY        _O_RDONLY
#define O_WRONLY        _O_WRONLY
#define O_RDWR          _O_RDWR
#define O_APPEND        _O_APPEND
#define O_CREAT         _O_CREAT
#define O_TRUNC         _O_TRUNC
#define O_EXCL          _O_EXCL
#define O_TEXT          _O_TEXT
#define O_BINARY        _O_BINARY
#define O_RAW           _O_BINARY
#define O_TEMPORARY     _O_TEMPORARY
#define O_NOINHERIT     _O_NOINHERIT
#define O_SEQUENTIAL    _O_SEQUENTIAL
#define O_RANDOM        _O_RANDOM
//filename, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR
*/
}

////////////////////////////////////////////////////////////////////////////////
// Creates or opens a file using the windows CreateFile method. Notice below we
// have used the method CreateFileA to avoid unicode characters - for now anyway
// TODO oreste: map the flags e.g. O_RDWR to the equivalents for CreateFileA
////////////////////////////////////////////////////////////////////////////////

int TRI_openFile (const char* filename, int openFlags) {
  HANDLE fileHandle;
  int    fileDescriptor;

  fileHandle = CreateFileA(filename,
                          GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          OPEN_EXISTING,
                          0,
                          NULL);
  if (fileHandle == INVALID_HANDLE_VALUE) {
    return -1;
  }

  fileDescriptor = _open_osfhandle( (intptr_t)(fileHandle), O_RDWR| _O_BINARY);
  return fileDescriptor;

/*
#define O_RDONLY        _O_RDONLY
#define O_WRONLY        _O_WRONLY
#define O_RDWR          _O_RDWR
#define O_APPEND        _O_APPEND
#define O_CREAT         _O_CREAT
#define O_TRUNC         _O_TRUNC
#define O_EXCL          _O_EXCL
#define O_TEXT          _O_TEXT
#define O_BINARY        _O_BINARY
#define O_RAW           _O_BINARY
#define O_TEMPORARY     _O_TEMPORARY
#define O_NOINHERIT     _O_NOINHERIT
#define O_SEQUENTIAL    _O_SEQUENTIAL
#define O_RANDOM        _O_RANDOM
//filename, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR
*/
}



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

