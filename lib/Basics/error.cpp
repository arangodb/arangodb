////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

/// @brief error number and system error
struct ErrorContainer {
  ErrorCode _number = TRI_ERROR_NO_ERROR;
  int _sys = 0;
};

/// @brief holds the last error that occurred in the current thread
thread_local ErrorContainer LastError;

/// @brief returns the last error
ErrorCode TRI_errno() { return LastError._number; }

/// @brief returns the last error as string
std::string_view TRI_last_error() {
  ErrorCode err = LastError._number;

  if (err == TRI_ERROR_SYS_ERROR) {
    return strerror(LastError._sys);
  }

  return TRI_errno_string(err);
}

/// @brief sets the last error
ErrorCode TRI_set_errno(ErrorCode error) {
  LastError._number = error;

  if (error == TRI_ERROR_SYS_ERROR) {
    LastError._sys = errno;
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
