////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include <WinSock2.h>  // must be before windows.h
#include <shellapi.h>
#include <windows.h>

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <iomanip>
#include <locale>

#include "Basics/Common.h"
#include "Basics/operating-system.h"

#include "win-utils.h"

#include <VersionHelpers.h>
#include <atlstr.h>
#include <crtdbg.h>
#include <malloc.h>
#include <string.h>

#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/directories.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb::basics;

std::wstring arangodb::basics::toWString(std::string const& validUTF8String) {
  int len =
      ::MultiByteToWideChar(CP_UTF8, NULL, validUTF8String.data(),
                            static_cast<int>(validUTF8String.size()), NULL, 0);
  std::wstring result;
  result.resize(len);
  ::MultiByteToWideChar(CP_UTF8, NULL, validUTF8String.data(),
                        static_cast<int>(validUTF8String.size()), result.data(),
                        len);
  return result;
}

std::string arangodb::basics::fromWString(wchar_t const* validUTF16String,
                                          std::size_t size) {
  int len = ::WideCharToMultiByte(CP_UTF8, NULL, validUTF16String,
                                  static_cast<int>(size), NULL, 0, NULL, NULL);
  std::string result;
  result.resize(len);
  ::WideCharToMultiByte(CP_UTF8, NULL, validUTF16String, static_cast<int>(size),
                        result.data(), len, NULL, NULL);
  return result;
}

std::string arangodb::basics::fromWString(
    std::wstring const& validUTF16String) {
  return fromWString(validUTF16String.data(), validUTF16String.size());
}

////////////////////////////////////////////////////////////////////////////////
// Callback function that is called when invalid parameters are passed to a CRT
// function. The MS documentations states:
//   > When the runtime calls the invalid parameter function, it usually means
//   > that a nonrecoverable error occurred. The invalid parameter handler
//   > function you supply should save any data it can and then abort. It should
//   > not return control to the main function unless you're confident that the
//   > error is recoverable.
// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/set-invalid-parameter-handler-set-thread-local-invalid-parameter-handler
//
// The behavior of the default invalid_parameter_handler is to simulate an
// unhandled SEH exception and let the Windows Error Reporting take over.
// However, there are potentially some cases that would trigger this callback.
// For example, ManagedDirectory::File::File has previously called TRI_DUP
// with an invalid file descriptor, which would result in a call to the
// invalid_parameter_handler.
//
// Since we cannot be sure whether there are some other cases, we use a custom
// callback that does _NOT_ follow the MS recommendation to abort the process,
// but simply hope that it is safe to ignore the error.
// However, we do log a message and that message should be taken VERY seriously!
////////////////////////////////////////////////////////////////////////////////
static void invalidParameterHandler(
    const wchar_t* expression,  // expression sent to function - NULL
    const wchar_t* function,    // name of function - NULL
    const wchar_t* file,        // file where code resides - NULL
    unsigned int line,          // line within file - NULL
    uintptr_t pReserved) {      // in case microsoft forget something

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  char buf[1024] = "";
  snprintf(buf, sizeof(buf) - 1,
           "Expression: %ls Function: %ls File: %ls Line: %d", expression,
           function, file, (int)line);
  buf[sizeof(buf) - 1] = '\0';
#endif

  LOG_TOPIC("e4644", ERR, arangodb::Logger::FIXME)
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      << "Invalid parameter passed: " << buf;
#else
      << "Invalid parameter passed";
#endif
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

    case TRI_WIN_INITIAL_SET_MAX_STD_IO: {
      int* newMax = (int*)(data);
      int result = _setmaxstdio(*newMax);
      if (result != *newMax) {
        return -1;
      }
      return 0;
    }

    default: {
      LOG_TOPIC("90c63", ERR, arangodb::Logger::FIXME)
          << "Invalid windows initialization called";
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

TRI_read_return_t TRI_READ_POINTER(HANDLE fd, void* Buffer, size_t length) {
  char* ptr = static_cast<char*>(Buffer);
  size_t remainLength = length;
  while (0 < remainLength) {
    DWORD read;
    if (ReadFile(fd, ptr, static_cast<DWORD>(remainLength), &read, nullptr)) {
      ptr += read;
      remainLength -= read;
    } else {
      auto err = GetLastError();
      if (err == ERROR_NO_DATA) {
        continue;
      } else if (err == ERROR_BROKEN_PIPE) {
        TRI_set_errno(TRI_ERROR_SYS_ERROR);
        LOG_TOPIC("87f53", ERR, arangodb::Logger::FIXME)
            << "cannot read, end-of-file";
        return static_cast<TRI_read_return_t>(length - remainLength);
      } else {
        TRI_set_errno(TRI_ERROR_SYS_ERROR);
        LOG_TOPIC("c9c0d", ERR, arangodb::Logger::FIXME)
            << "cannot read: " << TRI_LAST_ERROR_STR;
        return static_cast<TRI_read_return_t>(length - remainLength);
      }
    }
  }

  return static_cast<TRI_read_return_t>(length);
}

bool TRI_WRITE_POINTER(HANDLE fd, void const* buffer, size_t length) {
  char const* ptr = static_cast<char const*>(buffer);
  while (0 < length) {
    DWORD len = static_cast<DWORD>(length);
    DWORD written = 0;
    if (!WriteFile(fd, ptr, len, &written, nullptr)) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC("420b2", ERR, arangodb::Logger::FIXME)
          << "cannot write: " << TRI_LAST_ERROR_STR;
      return false;
    }
    ptr += written;
    length -= written;
  }

  return true;
}

FILE* TRI_FOPEN(char const* filename, char const* mode) {
  return ::fopen(filename, mode);
}

int TRI_CHDIR(char const* dirname) { return ::chdir(dirname); }

int TRI_STAT(char const* path, TRI_stat_t* buffer) {
  auto rc = ::_stat64(path, buffer);
  return rc;
}

char* TRI_GETCWD(char* buffer, int maxlen) {
  char* rc = nullptr;
  try {
    auto buf = std::make_unique<char[]>(maxlen);
    auto* rcw = ::_getcwd(buf.get(), maxlen);
    if (rcw != nullptr) {
      std::string rcs = rcw;
      if (rcs.length() + 1 < maxlen) {
        memcpy(buffer, rcs.c_str(), rcs.length() + 1);

        // tolower on hard-drive letter
        if ((rcs.length() >= 2) && (buffer[1] == ':') &&
            (::isupper(static_cast<unsigned char>(buffer[0])))) {
          buffer[0] = ::tolower(static_cast<unsigned char>(buffer[0]));
        }
        rc = buffer;
      }
    }
  } catch (...) {
  }
  return rc;
}

int TRI_MKDIR_WIN32(char const* dirname) { return ::mkdir(dirname); }

int TRI_RMDIR(char const* dirname) { return ::rmdir(dirname); }

int TRI_UNLINK(char const* filename) { return ::unlink(filename); }

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a Windows error to a *nix system error
////////////////////////////////////////////////////////////////////////////////

arangodb::Result translateWindowsError(DWORD error) {
  errno = TRI_MapSystemError(error);
  auto res = TRI_set_errno(TRI_ERROR_SYS_ERROR);
  return {res, windowsErrorToUTF8(error)};
}

std::string windowsErrorToUTF8(DWORD errorNum) {
  LPSTR buffer = nullptr;
  auto sg = arangodb::scopeGuard([&]() noexcept { ::LocalFree(buffer); });
  size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, errorNum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&buffer, 0, nullptr);
  if (size) {
    return std::string(buffer, size);
  }
  return "error translation failed";
}

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
  hEventLog = RegisterEventSourceW(NULL, L"ArangoDB");
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

// No clue why there is no header for these...
#define MSG_INVALID_COMMAND ((DWORD)0xC0020100L)
#define UI_CATEGORY ((WORD)0x00000003L)
void TRI_LogWindowsEventlog(char const* func, char const* file, int line,
                            std::string const& message) {
  char buf[1024];
  char linebuf[32];

  TRI_ASSERT(hEventLog != INVALID_HANDLE_VALUE);

  snprintf(linebuf, sizeof(linebuf), "%d", line);

  DWORD len = _snprintf(buf, sizeof(buf) - 1, "%s", message.c_str());
  buf[sizeof(buf) - 1] = '\0';

  LPCSTR buffers[] = {buf, file, func, linebuf, nullptr};
  // Try to get messages through to windows syslog...
  if (!ReportEventA(hEventLog, EVENTLOG_ERROR_TYPE, UI_CATEGORY,
                    MSG_INVALID_COMMAND, NULL, 4, 0, buffers, NULL)) {
    // well, fail then...
  }
}

void TRI_LogWindowsEventlog(char const* func, char const* file, int line,
                            char const* fmt, va_list ap) {
  char buf[1024];
  char linebuf[32];

  TRI_ASSERT(hEventLog != INVALID_HANDLE_VALUE);

  snprintf(linebuf, sizeof(linebuf), "%d", line);

  DWORD len = _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
  buf[sizeof(buf) - 1] = '\0';

  LPCSTR buffers[] = {buf, file, func, linebuf, nullptr};
  // Try to get messages through to windows syslog...
  if (!ReportEventA(hEventLog, EVENTLOG_ERROR_TYPE, UI_CATEGORY,
                    MSG_INVALID_COMMAND, NULL, 4, 0, buffers, NULL)) {
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

  _set_invalid_parameter_handler(invalidParameterHandler);

  res = initializeWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,
                          (char const*)(&maxOpenFiles));

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
    serviceAbort(exitCode);
  }

  _exit(exitCode);
}

// Detect cygwin ssh / terminals
int _cyg_isatty(int fd) {
  // detect standard windows ttys:
  if (_isatty(fd)) {
    return 1;
  }

  // stupid hack to allow forcing a tty..need to understand this better
  // and create a thorough fix..without this the logging stuff will not
  // log to the foreground which is super annoying for debugging the
  // resilience tests
  char* forcetty = getenv("FORCE_WINDOWS_TTY");
  if (forcetty != nullptr) {
    return strcmp(forcetty, "1") == 0;
  }

  HANDLE fh;

  char buff[sizeof(FILE_NAME_INFO) + sizeof(WCHAR) * MAX_PATH];
  FILE_NAME_INFO* FileInformation = (FILE_NAME_INFO*)buff;

  /* get the HANDLE for the filedescriptor. */
  fh = (HANDLE)_get_osfhandle(fd);
  if (!fh || fh == INVALID_HANDLE_VALUE) {
    return 0;
  }

  /* Cygwin consoles are pipes. If its not, no reason to continue: */
  if (GetFileType(fh) != FILE_TYPE_PIPE) {
    return 0;
  }

  if (!GetFileInformationByHandleEx(fh, FileNameInfo, FileInformation,
                                    sizeof(buff))) {
    return 0;
  }

  // we expect something along the lines of:
  // \cygwin-0eb90a57d5759b7b-pty3-to-master?? - if we find it its a tty.
  PWCHAR cp = (PWCHAR)FileInformation->FileName;
  if (!wcsncmp(cp, L"\\cygwin-", 8) && !wcsncmp(cp + 24, L"-pty", 4)) {
    cp = wcschr(cp + 28, '-');
    if (!cp) {
      return 0;
    }

    if (!wcsncmp(cp, L"-from-master", sizeof("-from-master") - 1) ||
        !wcsncmp(cp, L"-to-master", sizeof("-to-master") - 1)) {
      return 1;
    }
  }
  errno = EINVAL;
  return 0;
}

// Detect cygwin ssh / terminals
int _is_cyg_tty(int fd) {
  // detect standard windows ttys:
  if (_isatty(fd)) {
    return 0;
  }

  HANDLE fh;

  char buff[sizeof(FILE_NAME_INFO) + sizeof(WCHAR) * MAX_PATH];
  FILE_NAME_INFO* FileInformation = (FILE_NAME_INFO*)buff;

  /* get the HANDLE for the filedescriptor. */
  fh = (HANDLE)_get_osfhandle(fd);
  if (!fh || fh == INVALID_HANDLE_VALUE) {
    return 0;
  }

  /* Cygwin consoles are pipes. If its not, no reason to continue: */
  if (GetFileType(fh) != FILE_TYPE_PIPE) {
    return 0;
  }

  if (!GetFileInformationByHandleEx(fh, FileNameInfo, FileInformation,
                                    sizeof(buff))) {
    return 0;
  }

  // we expect something along the lines of:
  // \cygwin-0eb90a57d5759b7b-pty3-to-master?? - if we find it its a tty.
  PWCHAR cp = (PWCHAR)FileInformation->FileName;
  if (!wcsncmp(cp, L"\\cygwin-", 8) && !wcsncmp(cp + 24, L"-pty", 4)) {
    cp = wcschr(cp + 28, '-');
    if (!cp) {
      return 0;
    }

    if (!wcsncmp(cp, L"-from-master", sizeof("-from-master") - 1) ||
        !wcsncmp(cp, L"-to-master", sizeof("-to-master") - 1)) {
      return 1;
    }
  }
  errno = EINVAL;
  return 0;
}

bool terminalKnowsANSIColors() {
  if (_is_cyg_tty(STDOUT_FILENO)) {
    // Its a cygwin shell, expected to understand ANSI color codes.
    return true;
  }

  // Windows 8 onwards the CMD window understands ANSI-Colorcodes.
  return IsWindows8OrGreater();
}

std::string getFileNameFromHandle(HANDLE fileHandle) {
  char buff[sizeof(FILE_NAME_INFO) + sizeof(WCHAR) * MAX_PATH];
  FILE_NAME_INFO* FileInformation = (FILE_NAME_INFO*)buff;

  if (!GetFileInformationByHandleEx(fileHandle, FileNameInfo, FileInformation,
                                    sizeof(buff))) {
    return std::string();
  }
  return std::string((LPCTSTR)CString(FileInformation->FileName));
}
