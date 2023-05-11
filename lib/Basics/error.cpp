////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <cstring>
#include <unordered_map>

#include <frozen/unordered_map.h>

#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/error-registry.h"
#include "Basics/exitcodes.h"
#include "Basics/voc-errors.h"

#if defined(_WIN32)
#include <windows.h>
#endif

/// @brief error number and system error
struct ErrorContainer {
  ErrorCode _number = TRI_ERROR_NO_ERROR;

#if defined(_WIN32)
  DWORD _sys = 0;
#else
  int _sys = 0;
#endif
};

/// @brief holds the last error that occurred in the current thread
thread_local ErrorContainer LastError;

/// @brief returns the last error
ErrorCode TRI_errno() { return LastError._number; }

/// @brief returns the last error as string
std::string TRI_last_error() {
  ErrorCode err = LastError._number;

  if (err == TRI_ERROR_SYS_ERROR) {
#if defined(_WIN32)
    if (LastError._sys == 0) {
      return {};
    }
    LPSTR messageBuffer = nullptr;
    std::size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, LastError._sys, 0, (LPSTR)&messageBuffer, 0, nullptr);
    if (size == 0) {
      return "failed to get error message, the error code is " +
             std::to_string(LastError._sys);
    }
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
#else
    return strerror(LastError._sys);
#endif
  }

  return std::string{TRI_errno_string(err)};
}

/// @brief sets the last error
ErrorCode TRI_set_errno(ErrorCode error) {
  LastError._number = error;

  if (error == TRI_ERROR_SYS_ERROR) {
#if defined(_WIN32)
    LastError._sys = GetLastError();
#else
    LastError._sys = errno;
#endif
  } else {
    LastError._sys = 0;
  }

  return error;
}

/// @brief return an error message for an error code
std::string_view TRI_errno_string(ErrorCode code) noexcept {
  using arangodb::error::ErrorMessages;
  auto it = ErrorMessages.find(code);

  if (it == ErrorMessages.end()) {
    // return a hard-coded string as not all callers check for nullptr
    return "unknown error";
  }

  return (*it).second;
}
