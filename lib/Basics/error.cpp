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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <cstring>
#include <unordered_map>

#include "Basics/Common.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/exitcodes.h"
#include "Basics/voc-errors.h"

/// @brief error number and system error
struct ErrorContainer {
  int _number = TRI_ERROR_NO_ERROR;
  int _sys = TRI_ERROR_NO_ERROR;
};

/// @brief holds the last error that occurred in the current thread
thread_local ErrorContainer LastError;

/// @brief the error messages, will be read-only after initialization
static std::unordered_map<int, char const*> ErrorMessages;
static std::unordered_map<int, char const*> ExitMessages;

/// @brief returns the last error
int TRI_errno() { return LastError._number; }

/// @brief returns the last error as string
char const* TRI_last_error() {
  int err = LastError._number;

  if (err == TRI_ERROR_SYS_ERROR) {
    return strerror(LastError._sys);
  }

  return TRI_errno_string(err);
}

/// @brief sets the last error
int TRI_set_errno(int error) {
  LastError._number = error;

  if (error == TRI_ERROR_SYS_ERROR) {
    LastError._sys = errno;
  } else {
    LastError._sys = 0;
  }

  return error;
}

/// @brief defines an exit code string
void TRI_set_exitno_string(int code, char const* msg) {
  TRI_ASSERT(msg != nullptr);

  if (!ExitMessages.try_emplace(code, msg).second) {
    // logic error, error number is redeclared
    printf("Error: duplicate declaration of exit code %i in %s:%i\n", code, __FILE__, __LINE__);
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }
}

/// @brief defines an error string
void TRI_set_errno_string(int code, char const* msg) {
  TRI_ASSERT(msg != nullptr);

  if (!ErrorMessages.try_emplace(code, msg).second) {
    // logic error, error number is redeclared
    printf("Error: duplicate declaration of error code %i in %s:%i\n", code,
           __FILE__, __LINE__);
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }
}

/// @brief return an error message for an error code
char const* TRI_errno_string(int code) noexcept {
  auto it = ErrorMessages.find(code);

  if (it == ErrorMessages.end()) {
    // return a hard-coded string as not all callers check for nullptr
    return "unknown error";
  }

  return (*it).second;
}

/// @brief initializes the error messages
void TRI_InitializeError() {
  TRI_InitializeErrorMessages();
  TRI_InitializeExitMessages();
}
