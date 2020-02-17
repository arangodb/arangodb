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

#ifndef ARANGODB_BASICS_WIN__UTILS_H
#define ARANGODB_BASICS_WIN__UTILS_H 1

#include <string>
#include <IntSafe.h>

#include "Basics/Result.h"

// .............................................................................
// Called before anything else starts - initializes whatever is required to be
// initialized.
// .............................................................................

typedef enum {
  TRI_WIN_INITIAL_SET_DEBUG_FLAG,
  TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER,
  TRI_WIN_INITIAL_SET_MAX_STD_IO,
  TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL
} TRI_win_initialize_e;

int initializeWindows(const TRI_win_initialize_e, char const*);

void ADB_WindowsEntryFunction();
void ADB_WindowsExitFunction(int exitCode, void* data);

// .............................................................................
// This function uses the CreateFile windows method rather than _open which
// then will allow the application to rename files on the fly.
// .............................................................................

int TRI_createFile(char const* filename, int openFlags, int modeFlags);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a file for windows
////////////////////////////////////////////////////////////////////////////////

int TRI_OPEN_WIN32(char const* filename, int openFlags);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a Windows error to a *nix system error
////////////////////////////////////////////////////////////////////////////////

int TRI_MapSystemError(DWORD);
std::string windowsErrorToUTF8(DWORD);
arangodb::Result translateWindowsError(DWORD);

////////////////////////////////////////////////////////////////////////////////
/// @brief open/close the windows eventlog. Call on start / shutdown
////////////////////////////////////////////////////////////////////////////////

bool TRI_InitWindowsEventLog(void);
void TRI_CloseWindowsEventlog(void);

typedef void (*TRI_serviceAbort_t)(uint16_t exitCode);

void TRI_SetWindowsServiceAbortFunction(TRI_serviceAbort_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message to the windows event log.
/// we rather are keen on logging something at all then on being able to work
/// with fancy dynamic buffers; thus we work with a static buffer.
/// the arango internal logging will handle that usually.
////////////////////////////////////////////////////////////////////////////////

void TRI_LogWindowsEventlog(char const* func, char const* file, int line, std::string const&);

void TRI_LogWindowsEventlog(char const* func, char const* file, int line,
                            char const* fmt, va_list ap);

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message to the windows event log.
/// this wrapper (and the macro) are similar to regular log facilities.
/// they should however only be used in panic situations.
////////////////////////////////////////////////////////////////////////////////

void TRI_WindowsEmergencyLog(char const* func, char const* file, int line,
                             char const* fmt, ...);

#define LOG_FATAL_WINDOWS(...)                                              \
  do {                                                                      \
    TRI_WindowsEmergencyLog(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__); \
  } while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief detects whether an FD is connected to a (cygwin-)tty.
////////////////////////////////////////////////////////////////////////////////
int _cyg_isatty(int fd);

////////////////////////////////////////////////////////////////////////////////
/// @brief detects whether an FD is connected to a cygwin-tty.
////////////////////////////////////////////////////////////////////////////////
int _is_cyg_tty(int fd);

////////////////////////////////////////////////////////////////////////////////
// returns true if the terminal window knows how to handle ANSI color codes.
////////////////////////////////////////////////////////////////////////////////
bool terminalKnowsANSIColors();

////////////////////////////////////////////////////////////////////////////////
// returns returns the filename in conjunction with a handle
// only visible, if winndef.h was previously included for HANDLE.
////////////////////////////////////////////////////////////////////////////////

#ifdef _WINDEF_
std::string getFileNameFromHandle(HANDLE fileHandle);
#endif

#endif
