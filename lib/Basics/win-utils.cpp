////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Oreste Costa-Panaia
////////////////////////////////////////////////////////////////////////////////

#include <errno.h>

#include <io.h>

#include "win-utils.h"

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <crtdbg.h>

#include "Logger/Logger.h"
#include "Basics/files.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/directories.h"

using namespace arangodb::basics;

// .............................................................................
// Some global variables which may be required later
// .............................................................................

_invalid_parameter_handler oldInvalidHandleHandler;
_invalid_parameter_handler newInvalidHandleHandler;

// Windows variant for unistd.h's ftruncate()
int ftruncate(int fd, long newSize) {
  int result = _chsize(fd, newSize);
  return result;
}

// Windows variant for getpagesize()
int getpagesize(void) {
  static int pageSize = 0;  // only define it once

  if (!pageSize) {
    // first time, so call the system info function
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    pageSize = systemInfo.dwPageSize;
  }

  return pageSize;
}

////////////////////////////////////////////////////////////////////////////////
// Calls the windows Sleep function which always sleeps for milliseconds
////////////////////////////////////////////////////////////////////////////////

void TRI_sleep(unsigned long waitTime) { Sleep(waitTime * 1000); }

////////////////////////////////////////////////////////////////////////////////
// Calls a timer which waits for a signal after the elapsed time.
// The timer is accurate to 100nanoseconds
////////////////////////////////////////////////////////////////////////////////

void TRI_usleep(unsigned long waitTime) {
  int result;
  HANDLE hTimer = NULL;            // stores the handle of the timer object
  LARGE_INTEGER wTime;             // essentially a 64bit number
  wTime.QuadPart = waitTime * 10;  // *10 to change to microseconds
  wTime.QuadPart =
      -wTime.QuadPart;  // negative indicates relative time elapsed,

  // Create an unnamed waitable timer.
  hTimer = CreateWaitableTimer(NULL, 1, NULL);

  if (hTimer == NULL) {
    // not much we can do at this low level
    return;
  }

  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    LOG(FATAL) << "internal error in TRI_usleep()";
    FATAL_ERROR_EXIT();
  }

  // Set timer to wait for indicated micro seconds.
  if (!SetWaitableTimer(hTimer, &wTime, 0, NULL, NULL, 0)) {
    // not much we can do at this low level
    CloseHandle(hTimer);
    return;
  }

  // Wait for the timer
  result = WaitForSingleObject(hTimer, INFINITE);

  if (result != WAIT_OBJECT_0) {
    CloseHandle(hTimer);
    LOG(FATAL) << "couldn't wait for timer in TRI_usleep()";
    FATAL_ERROR_EXIT();
  }

  CloseHandle(hTimer);
  // todo: go through what the result is e.g. WAIT_OBJECT_0
  return;
}

////////////////////////////////////////////////////////////////////////////////
// Sets up a handler when invalid (win) handles are passed to a windows
// function.
// This is not of much use since no values can be returned. All we can do
// for now is to ignore error and hope it goes away!
////////////////////////////////////////////////////////////////////////////////

static void InvalidParameterHandler(
    const wchar_t* expression,  // expression sent to function - NULL
    const wchar_t* function,    // name of function - NULL
    const wchar_t* file,        // file where code resides - NULL
    unsigned int line,          // line within file - NULL
    uintptr_t pReserved) {      // in case microsoft forget something
  LOG(ERR) << "Invalid handle parameter passed";
}

////////////////////////////////////////////////////////////////////////////////
// Called from the 'main' and performs any initialization requirements which
// are specific to windows.
//
// If this function returns 0, then no errors encountered. If < 0, then the
// calling function should terminate the application. If > 0, then the
// calling function should decide what to do.
////////////////////////////////////////////////////////////////////////////////

int finalizeWindows(const TRI_win_finalize_e finalizeWhat, char const* data) {
  // ............................................................................
  // The data is used to transport information from the calling function to here
  // it may be NULL (and will be in most cases)
  // ............................................................................

  switch (finalizeWhat) {
    case TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL: {
      /*
        TODO: we can't always determine when to call this properly.
        if we have closed libev, its ok, if we have active socket operations
        these will fail with errors.
      int result =
        WSACleanup();  // could this cause error on server termination?
      if (result != 0) {
        // can not use LOG_ etc here since the logging may have terminated
        printf(
            "ERROR: Could not perform a valid Winsock2 cleanup. WSACleanup "
            "returned error %d.",
            result);
        return -1;
      }
      */
      return 0;
    }

    default: {
      // can not use LOG_ etc here since the logging may have terminated
      printf("ERROR: Invalid windows finalization called");
      return -1;
    }
  }

  return -1;
}

int initializeWindows(const TRI_win_initialize_e initializeWhat,
                      char const* data) {
  // ............................................................................
  // The data is used to transport information from the calling function to here
  // it may be NULL (and will be in most cases)
  // ............................................................................

  switch (initializeWhat) {
    case TRI_WIN_INITIAL_SET_DEBUG_FLAG: {
      _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF) |
                     _CRTDBG_CHECK_ALWAYS_DF);
      return 0;
    }

    // ...........................................................................
    // Assign a handler for invalid handles
    // ...........................................................................

    case TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER: {
      newInvalidHandleHandler = InvalidParameterHandler;
      oldInvalidHandleHandler =
          _set_invalid_parameter_handler(newInvalidHandleHandler);
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
      WORD wVersionRequested = MAKEWORD(2, 2);
      errorCode = WSAStartup(wVersionRequested, &wsaData);

      if (errorCode != 0) {
        LOG(ERR) << "Could not find a usable Winsock DLL. WSAStartup returned "
                    "an error.";
        return -1;
      }

      if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        LOG(ERR) << "Could not find a usable Winsock DLL. WSAStartup did not "
                    "return version 2.2.";
        WSACleanup();
        return -1;
      }
      return 0;
    }

    default: {
      LOG(ERR) << "Invalid windows initialization called";
      return -1;
    }
  }

  return -1;
}

int TRI_createFile(char const* filename, int openFlags, int modeFlags) {
  HANDLE fileHandle;
  int fileDescriptor;

  fileHandle =
      CreateFileA(filename, GENERIC_READ | GENERIC_WRITE,
                  FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                  (openFlags & O_APPEND) ? OPEN_ALWAYS : CREATE_NEW, 0, NULL);

  if (fileHandle == INVALID_HANDLE_VALUE) {
    return -1;
  }

  if (openFlags & O_APPEND) {
    SetFilePointer(fileHandle, 0, NULL, FILE_END);
  }

  fileDescriptor = _open_osfhandle((intptr_t)fileHandle, O_RDWR | _O_BINARY);

  return fileDescriptor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a file for windows
///
/// Creates or opens a file using the windows CreateFile method. Notice below we
/// have used the method CreateFileA to avoid unicode characters - for now
/// anyway.
////////////////////////////////////////////////////////////////////////////////

int TRI_OPEN_WIN32(char const* filename, int openFlags) {
  static int const O_ACCMODE = 3;
  HANDLE fileHandle;
  int fileDescriptor;
  DWORD mode;

  switch (openFlags & O_ACCMODE) {
    case O_RDONLY:
      mode = GENERIC_READ;
      break;

    case O_WRONLY:
      mode = GENERIC_WRITE;
      break;

    case O_RDWR:
      mode = GENERIC_READ | GENERIC_WRITE;
      break;
  }

  fileHandle = CreateFileA(
      filename, mode, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL, OPEN_EXISTING, 0, NULL);

  if (fileHandle == INVALID_HANDLE_VALUE) {
    return -1;
  }

  fileDescriptor = _open_osfhandle((intptr_t)(fileHandle),
                                   (openFlags & O_ACCMODE) | _O_BINARY);
  return fileDescriptor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fixes the ICU_DATA environment path
////////////////////////////////////////////////////////////////////////////////

void TRI_FixIcuDataEnv() {
  if (getenv("ICU_DATA") != nullptr) {
    return;
  }

  std::string p = TRI_LocateInstallDirectory();

  if (!p.empty()) {
    std::string e = "ICU_DATA=" + p + ICU_DESTINATION_DIRECTORY;
    e = StringUtils::replace(e, "\\", "\\\\");
    putenv(e.c_str());
  } else {
#ifdef _SYSCONFDIR_
    std::string SCDIR(_SYSCONFDIR_);
    SCDIR = StringUtils::replace(SCDIR, "/", "\\\\");
    std::string e = "ICU_DATA=" + SCDIR + "..\\..\\bin";
    e = StringUtils::replace(e, "\\", "\\\\");
    putenv(e.c_str());
#else

    p = TRI_LocateBinaryPath(nullptr);

    if (!p.empty()) {
      std::string e = "ICU_DATA=" + p + "\\";
      e = StringUtils::replace(e, "\\", "\\\\");
      putenv(e.c_str());
    } else {
      putenv("ICU_DATA=.\\\\");
    }
#endif
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a Windows error to a *nix system error
////////////////////////////////////////////////////////////////////////////////

int TRI_MapSystemError(DWORD error) {
  switch (error) {
    case ERROR_INVALID_FUNCTION:
      return EINVAL;
    case ERROR_FILE_NOT_FOUND:
      return ENOENT;
    case ERROR_PATH_NOT_FOUND:
      return ENOENT;
    case ERROR_TOO_MANY_OPEN_FILES:
      return EMFILE;
    case ERROR_ACCESS_DENIED:
      return EACCES;
    case ERROR_INVALID_HANDLE:
      return EBADF;
    case ERROR_NOT_ENOUGH_MEMORY:
      return ENOMEM;
    case ERROR_INVALID_DATA:
      return EINVAL;
    case ERROR_OUTOFMEMORY:
      return ENOMEM;
    case ERROR_INVALID_DRIVE:
      return ENODEV;
    case ERROR_NOT_SAME_DEVICE:
      return EXDEV;
    case ERROR_NO_MORE_FILES:
      return ENFILE;
    case ERROR_WRITE_PROTECT:
      return EROFS;
    case ERROR_BAD_UNIT:
      return ENODEV;
    case ERROR_SHARING_VIOLATION:
      return EACCES;
    case ERROR_LOCK_VIOLATION:
      return EACCES;
    case ERROR_SHARING_BUFFER_EXCEEDED:
      return ENOLCK;
    case ERROR_HANDLE_EOF:
      return ENODATA;
    case ERROR_HANDLE_DISK_FULL:
      return ENOSPC;
    case ERROR_NOT_SUPPORTED:
      return ENOSYS;
    case ERROR_REM_NOT_LIST:
      return ENFILE;
    case ERROR_DUP_NAME:
      return EEXIST;
    case ERROR_BAD_NETPATH:
      return EBADF;
    case ERROR_BAD_NET_NAME:
      return EBADF;
    case ERROR_FILE_EXISTS:
      return EEXIST;
    case ERROR_CANNOT_MAKE:
      return EPERM;
    case ERROR_INVALID_PARAMETER:
      return EINVAL;
    case ERROR_NO_PROC_SLOTS:
      return EAGAIN;
    case ERROR_BROKEN_PIPE:
      return EPIPE;
    case ERROR_OPEN_FAILED:
      return EIO;
    case ERROR_NO_MORE_SEARCH_HANDLES:
      return ENFILE;
    case ERROR_CALL_NOT_IMPLEMENTED:
      return ENOSYS;
    case ERROR_INVALID_NAME:
      return ENOENT;
    case ERROR_WAIT_NO_CHILDREN:
      return ECHILD;
    case ERROR_CHILD_NOT_COMPLETE:
      return EBUSY;
    case ERROR_DIR_NOT_EMPTY:
      return ENOTEMPTY;
    case ERROR_SIGNAL_REFUSED:
      return EIO;
    case ERROR_BAD_PATHNAME:
      return ENOENT;
    case ERROR_SIGNAL_PENDING:
      return EBUSY;
    case ERROR_MAX_THRDS_REACHED:
      return EAGAIN;
    case ERROR_BUSY:
      return EBUSY;
    case ERROR_ALREADY_EXISTS:
      return EEXIST;
    case ERROR_NO_SIGNAL_SENT:
      return EIO;
    case ERROR_FILENAME_EXCED_RANGE:
      return ENAMETOOLONG;
    case ERROR_META_EXPANSION_TOO_LONG:
      return EINVAL;
    case ERROR_INVALID_SIGNAL_NUMBER:
      return EINVAL;
    case ERROR_THREAD_1_INACTIVE:
      return EINVAL;
    case ERROR_BAD_PIPE:
      return EINVAL;
    case ERROR_PIPE_BUSY:
      return EBUSY;
    case ERROR_NO_DATA:
      return EPIPE;
    case ERROR_PIPE_NOT_CONNECTED:
      return EPIPE;
    case ERROR_MORE_DATA:
      return EAGAIN;
    case ERROR_DIRECTORY:
      return ENOTDIR;
    case ERROR_PIPE_CONNECTED:
      return EBUSY;
    case ERROR_PIPE_LISTENING:
      return EPIPE;
    case ERROR_NO_TOKEN:
      return EINVAL;
    case ERROR_PROCESS_ABORTED:
      return EFAULT;
    case ERROR_BAD_DEVICE:
      return ENODEV;
    case ERROR_BAD_USERNAME:
      return EINVAL;
    case ERROR_NOT_CONNECTED:
      return ENOLINK;
    case ERROR_OPEN_FILES:
      return EAGAIN;
    case ERROR_ACTIVE_CONNECTIONS:
      return EAGAIN;
    case ERROR_DEVICE_IN_USE:
      return EAGAIN;
    case ERROR_INVALID_AT_INTERRUPT_TIME:
      return EINTR;
    case ERROR_IO_DEVICE:
      return EIO;
    case ERROR_NOT_OWNER:
      return EPERM;
    case ERROR_END_OF_MEDIA:
      return ENOSPC;
    case ERROR_EOM_OVERFLOW:
      return ENOSPC;
    case ERROR_BEGINNING_OF_MEDIA:
      return ESPIPE;
    case ERROR_SETMARK_DETECTED:
      return ESPIPE;
    case ERROR_NO_DATA_DETECTED:
      return ENOSPC;
    case ERROR_POSSIBLE_DEADLOCK:
      return EDEADLOCK;
    case ERROR_CRC:
      return EIO;
    case ERROR_NEGATIVE_SEEK:
      return EINVAL;
    case ERROR_NOT_READY:
      return EBADF;
    case ERROR_DISK_FULL:
      return ENOSPC;
    case ERROR_NOACCESS:
      return EFAULT;
    case ERROR_FILE_INVALID:
      return ENXIO;
    default:
      return EINVAL;
  }
}

static HANDLE hEventLog = INVALID_HANDLE_VALUE;

bool TRI_InitWindowsEventLog(void) {
  hEventLog = RegisterEventSource(NULL, "ArangoDB");
  if (NULL == hEventLog) {
    // well, fail then.
    return false;
  }
  return true;
}

void TRI_CloseWindowsEventlog(void) {
  DeregisterEventSource(hEventLog);
  hEventLog = INVALID_HANDLE_VALUE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message to the windows event log.
/// we rather are keen on logging something at all then on being able to work
/// with fancy dynamic buffers; thus we work with a static buffer.
/// the arango internal logging will handle that usually.
////////////////////////////////////////////////////////////////////////////////

// No clue why there is no header for these...
#define MSG_INVALID_COMMAND ((DWORD)0xC0020100L)
#define UI_CATEGORY ((WORD)0x00000003L)
void TRI_LogWindowsEventlog(char const* func, char const* file, int line,
                            std::string const& message) {
  char buf[1024];
  char linebuf[32];
  LPCSTR logBuffers[] = {buf, file, func, linebuf, NULL};

  TRI_ASSERT(hEventLog != INVALID_HANDLE_VALUE);

  snprintf(linebuf, sizeof(linebuf), "%d", line);

  DWORD len = _snprintf(buf, sizeof(buf) - 1, "%s", message.c_str());
  buf[sizeof(buf) - 1] = '\0';

  // Try to get messages through to windows syslog...
  if (!ReportEvent(hEventLog, EVENTLOG_ERROR_TYPE, UI_CATEGORY,
                   MSG_INVALID_COMMAND, NULL, 4, 0, (LPCSTR*)logBuffers,
                   NULL)) {
    // well, fail then...
  }
}

void TRI_LogWindowsEventlog(char const* func, char const* file, int line,
                            char const* fmt, va_list ap) {
  char buf[1024];
  char linebuf[32];
  LPCSTR logBuffers[] = {buf, file, func, linebuf, NULL};

  TRI_ASSERT(hEventLog != INVALID_HANDLE_VALUE);

  snprintf(linebuf, sizeof(linebuf), "%d", line);

  DWORD len = _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
  buf[sizeof(buf) - 1] = '\0';

  // Try to get messages through to windows syslog...
  if (!ReportEvent(hEventLog, EVENTLOG_ERROR_TYPE, UI_CATEGORY,
                   MSG_INVALID_COMMAND, NULL, 4, 0, (LPCSTR*)logBuffers,
                   NULL)) {
    // well, fail then...
  }
}

void TRI_WindowsEmergencyLog(char const* func, char const* file, int line,
                             char const* fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  TRI_LogWindowsEventlog(func, file, line, fmt, ap);
  va_end(ap);
}

void ADB_WindowsEntryFunction() {
  int maxOpenFiles = 2048;
  int res = 0;

  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  // res = initializeWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initializeWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);

  if (res != 0) {
    _exit(EXIT_FAILURE);
  }

  res = initializeWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,
                          (char const*)(&maxOpenFiles));

  if (res != 0) {
    _exit(EXIT_FAILURE);
  }

  res = initializeWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    _exit(EXIT_FAILURE);
  }

  TRI_Application_Exit_SetExit(ADB_WindowsExitFunction);
}

TRI_serviceAbort_t serviceAbort = nullptr;

void TRI_SetWindowsServiceAbortFunction(TRI_serviceAbort_t f) {
  serviceAbort = f;
}

void ADB_WindowsExitFunction(int exitCode, void* data) {
  if (serviceAbort != nullptr) {
    serviceAbort();
  }
  int res = finalizeWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(EXIT_FAILURE);
  }

  exit(exitCode);
}
