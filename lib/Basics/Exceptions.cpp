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

#include "Exceptions.h"

using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief controls if backtraces are printed with exceptions
////////////////////////////////////////////////////////////////////////////////

static bool WithBackTrace = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief controls whether a backtrace is created for each exception
////////////////////////////////////////////////////////////////////////////////

void Exception::SetVerbose(bool verbose) { WithBackTrace = verbose; }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, without format string
////////////////////////////////////////////////////////////////////////////////

Exception::Exception(int code, char const* file, int line)
    : _errorMessage(TRI_errno_string(code)),
      _file(file),
      _line(line),
      _code(code) {
  
  appendLocation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, for creating an exception with an already created
/// error message (normally based on error templates containing %s, %d etc.)
////////////////////////////////////////////////////////////////////////////////

Exception::Exception(int code, std::string const& errorMessage,
                     char const* file, int line)
    : _errorMessage(errorMessage), _file(file), _line(line), _code(code) {

  appendLocation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, for creating an exception with an already created
/// error message (normally based on error templates containing %s, %d etc.)
////////////////////////////////////////////////////////////////////////////////

Exception::Exception(int code, char const* errorMessage, char const* file,
                     int line)
    : _errorMessage(errorMessage), _file(file), _line(line), _code(code) {
  
  appendLocation();
}

Exception::~Exception() throw() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the error message
////////////////////////////////////////////////////////////////////////////////

std::string Exception::message() const throw() { return _errorMessage; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the error code
////////////////////////////////////////////////////////////////////////////////

int Exception::code() const throw() { return _code; }

////////////////////////////////////////////////////////////////////////////////
/// @brief adds to the message
////////////////////////////////////////////////////////////////////////////////

void Exception::addToMessage(std::string const& more) { _errorMessage += more; }

////////////////////////////////////////////////////////////////////////////////
/// @brief return exception message
////////////////////////////////////////////////////////////////////////////////

char const* Exception::what() const throw() { return _errorMessage.c_str(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief append original error location to message
////////////////////////////////////////////////////////////////////////////////

void Exception::appendLocation () {
  if (_code == TRI_ERROR_INTERNAL) {
    _errorMessage += std::string(" (location: ") + _file + ":" + std::to_string(_line) + "). Please report this error to arangodb.com";
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
#if HAVE_BACKTRACE
  if (WithBackTrace) {
    _errorMessage += std::string("\n\n");
    TRI_GetBacktrace(_errorMessage);
    _errorMessage += std::string("\n\n");
  }
#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct an error message from a template string
////////////////////////////////////////////////////////////////////////////////

std::string Exception::FillExceptionString(int code, ...) {
  char const* format = TRI_errno_string(code);
  TRI_ASSERT(format != nullptr);

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // Obviously the formatstring of the error code has to support parameters.
  TRI_ASSERT(strchr(format, '%') != nullptr);
#endif

  char buffer[1024];
  va_list ap;
  va_start(ap, code);
  vsnprintf(buffer, sizeof(buffer) - 1, format, ap);
  va_end(ap);
  buffer[sizeof(buffer) - 1] = '\0';  // Windows

  return std::string(buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct an error message from a template string
////////////////////////////////////////////////////////////////////////////////

std::string Exception::FillFormatExceptionString(char const* format, ...) {
  TRI_ASSERT(format != nullptr);

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // Format #1 should come from the macro...
  TRI_ASSERT(strchr(format, '%') != nullptr);
  // Obviously the user has to give us a format string.
  TRI_ASSERT(strchr(strchr(format, '%'), '%') != nullptr);
#endif

  char buffer[1024];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer) - 1, format, ap);
  va_end(ap);
  buffer[sizeof(buffer) - 1] = '\0';  // Windows

  return std::string(buffer);
}
