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

#include <cstdarg>
#include <cstdio>
#include <type_traits>

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#include <cstring>
#endif

#include "Exceptions.h"

#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;

// All other constructors delegate to this one.
Exception::Exception(ErrorCode code, std::string&& errorMessage,
                     SourceLocation location) noexcept
    : _errorMessage(std::move(errorMessage)), _location(location), _code(code) {
  appendLocation();
}
Exception::Exception(ErrorCode code, SourceLocation location)
    : Exception(code, TRI_errno_string(code), location) {}
Exception::Exception(Result const& result, SourceLocation location)
    : Exception(result.errorNumber(), result.errorMessage(), location) {}
Exception::Exception(Result&& result, SourceLocation location) noexcept
    : Exception(result.errorNumber(), std::move(result).errorMessage(),
                location) {}
Exception::Exception(ErrorCode code, std::string_view errorMessage,
                     SourceLocation location)
    : Exception(code, std::string{errorMessage}, location) {}
Exception::Exception(ErrorCode code, const char* errorMessage,
                     SourceLocation location)
    : Exception(code, std::string{errorMessage}, location) {}

Exception::Exception(ErrorCode code, char const* file, int line)
    : Exception(code, SourceLocation(file, line)) {}
Exception::Exception(arangodb::Result const& result, char const* file, int line)
    : Exception(result, SourceLocation(file, line)) {}
Exception::Exception(arangodb::Result&& result, char const* file,
                     int line) noexcept
    : Exception(std::move(result), SourceLocation(file, line)) {}
Exception::Exception(ErrorCode code, std::string_view errorMessage,
                     char const* file, int line)
    : Exception(code, errorMessage, SourceLocation(file, line)) {}
Exception::Exception(ErrorCode code, std::string&& errorMessage,
                     char const* file, int line) noexcept
    : Exception(code, std::move(errorMessage), SourceLocation(file, line)) {}
Exception::Exception(ErrorCode code, char const* errorMessage, char const* file,
                     int line)
    : Exception(code, errorMessage, SourceLocation(file, line)) {}

/// @brief returns the error message
std::string const& Exception::message() const noexcept { return _errorMessage; }

/// @brief returns the error code
ErrorCode Exception::code() const noexcept { return _code; }

/// @brief return exception message
char const* Exception::what() const noexcept { return _errorMessage.c_str(); }

/// @brief append original error location to message
void Exception::appendLocation() noexcept try {
  if (_code == TRI_ERROR_INTERNAL) {
    _errorMessage += std::string(" (exception location: ") +
                     _location.file_name() + ":" +
                     std::to_string(_location.line()) +
                     "). Please report this error to arangodb.com";
  } else if (_code == TRI_ERROR_OUT_OF_MEMORY ||
             _code == TRI_ERROR_NOT_IMPLEMENTED) {
    _errorMessage += std::string(" (exception location: ") +
                     _location.file_name() + ":" +
                     std::to_string(_location.line()) + ")";
  }
} catch (...) {
  // this function is called from the exception constructor, so it should
  // not itself throw another exception
}

/// @brief construct an error message from a template string
std::string Exception::FillExceptionString(ErrorCode code, ...) {
  // Note that we rely upon the string being null-terminated.
  // The string_view doesn't guarantee that, but we know that all error messages
  // are null-terminated.
  char const* format = TRI_errno_string(code).data();
  TRI_ASSERT(format != nullptr);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // Obviously the formatstring of the error code has to support parameters.
  TRI_ASSERT(strchr(format, '%') != nullptr);
#endif

  char buffer[1024];
  va_list ap;
  va_start(ap, code);
  int length = vsnprintf(buffer, sizeof(buffer) - 1, format, ap);
  va_end(ap);
  buffer[sizeof(buffer) - 1] = '\0';  // Windows

  if (length < 0) {
    // error in vsnprintf
    return std::string(format);
  }

  return std::string(buffer, size_t(length));
}

/// @brief construct an error message from a template string
std::string Exception::FillFormatExceptionString(char const* format, ...) {
  TRI_ASSERT(format != nullptr);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // Format #1 should come from the macro...
  TRI_ASSERT(strchr(format, '%') != nullptr);
  // Obviously the user has to give us a format string.
  TRI_ASSERT(strchr(strchr(format, '%'), '%') != nullptr);
#endif

  char buffer[1024];
  va_list ap;
  va_start(ap, format);
  int length = vsnprintf(buffer, sizeof(buffer) - 1, format, ap);
  va_end(ap);
  buffer[sizeof(buffer) - 1] = '\0';  // Windows

  if (length < 0) {
    // error in vsnprintf
    return std::string(format);
  }

  return std::string(buffer, size_t(length));
}

[[noreturn]] void ::arangodb::basics::helper::dieWithLogMessage(
    char const* errorMessage) {
  LOG_TOPIC("1d250", FATAL, Logger::FIXME)
      << "Failed to create an error message, giving up. " << errorMessage;
  FATAL_ERROR_EXIT();
}

[[noreturn]] void ::arangodb::basics::helper::logAndAbort(const char* what) {
  LOG_TOPIC("fa7a1", FATAL, ::arangodb::Logger::CRASH) << what;
  TRI_ASSERT(false);
  FATAL_ERROR_ABORT();
}
