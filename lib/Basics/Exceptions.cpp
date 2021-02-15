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

#include <stdio.h>
#include <cstdarg>
#include <type_traits>

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#include <cstring>
#endif

#include "Exceptions.h"

#include "Basics/debugging.h"
#include "Basics/error.h"

using namespace arangodb;
using namespace arangodb::basics;

/// @brief constructor, without format string
Exception::Exception(ErrorCode code, char const* file, int line)
    : _errorMessage(TRI_errno_string(code)), _file(file), _line(line), _code(code) {
  appendLocation();
}

Exception::Exception(arangodb::Result const& result, char const* file, int line)
    : _errorMessage(result.errorMessage()),
      _file(file),
      _line(line),
      _code(result.errorNumber()) {
  appendLocation();
}

Exception::Exception(arangodb::Result&& result, char const* file, int line)
    : _errorMessage(std::move(result).errorMessage()),  // cast to rvalueref so the error
                                                        // string gets moved out
      _file(file),
      _line(line),
      _code(result.errorNumber()) {
  appendLocation();
}

/// @brief constructor, for creating an exception with an already created
/// error message (normally based on error templates containing %s, %d etc.)
Exception::Exception(ErrorCode code, std::string_view errorMessage, char const* file, int line)
    : _errorMessage(errorMessage), _file(file), _line(line), _code(code) {
  appendLocation();
}

/// @brief constructor, for creating an exception with an already created
/// error message (normally based on error templates containing %s, %d etc.)
Exception::Exception(ErrorCode code, std::string&& errorMessage, char const* file, int line)
    : _errorMessage(std::move(errorMessage)), _file(file), _line(line), _code(code) {
  appendLocation();
}

/// @brief constructor, for creating an exception with an already created
/// error message (normally based on error templates containing %s, %d etc.)
Exception::Exception(ErrorCode code, char const* errorMessage, char const* file, int line)
    : _errorMessage(errorMessage), _file(file), _line(line), _code(code) {
  appendLocation();
}

/// @brief returns the error message
std::string const& Exception::message() const noexcept { return _errorMessage; }

/// @brief returns the error code
ErrorCode Exception::code() const noexcept { return _code; }

/// @brief adds to the message
void Exception::addToMessage(std::string const& more) { _errorMessage += more; }

/// @brief return exception message
char const* Exception::what() const noexcept { return _errorMessage.c_str(); }

/// @brief append original error location to message
void Exception::appendLocation() noexcept try {
  if (_code == TRI_ERROR_INTERNAL) {
    _errorMessage += std::string(" (exception location: ") + _file + ":" +
                     std::to_string(_line) +
                     "). Please report this error to arangodb.com";
  } else if (_code == TRI_ERROR_OUT_OF_MEMORY || _code == TRI_ERROR_NOT_IMPLEMENTED) {
    _errorMessage += std::string(" (exception location: ") + _file + ":" +
                     std::to_string(_line) + ")";
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
