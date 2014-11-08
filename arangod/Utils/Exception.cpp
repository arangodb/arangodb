////////////////////////////////////////////////////////////////////////////////
/// @brief arango exceptions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Exception.h"
#include "Basics/StringUtils.h"

using namespace std;
using namespace triagens::arango;

////////////////////////////////////////////////////////////////////////////////
/// @brief controls if backtraces are printed with exceptions
////////////////////////////////////////////////////////////////////////////////

static bool WithBackTrace = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, without format string
////////////////////////////////////////////////////////////////////////////////

Exception::Exception (int code,
                      char const* file,
                      int line)
  : _errorMessage(TRI_errno_string(code)),
    _file(file),
    _line(line),
    _code(code) {
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
/// @brief constructor, for creating an exception with an already created
/// error message (normally based on error templates containing %s, %d etc.)
////////////////////////////////////////////////////////////////////////////////

Exception::Exception (int code,
                      string const& errorMessage,
                      char const* file,
                      int line)
  : _errorMessage(errorMessage),
    _file(file),
    _line(line),
    _code(code) {

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
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

Exception::~Exception () throw () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return exception message
////////////////////////////////////////////////////////////////////////////////

char const* Exception::what () const throw () {
  // we have to use an instance member here because we should not return a 
  // pointer (c_str()) to the internals of a stack object (stack object will
  // be destroyed when function is left...)
  // additionally, we should not create new string values here as this might
  // throw exceptions - but this function is marked to throw no exceptions!
  /*
  std::string message = "exception in '";
  message.append(_file);
  message.append("' at line ");
  message.append(basics::StringUtils::itoa(_line));
  message.append(": ");
  message += this->message();

  return message.c_str();
  */

  return _errorMessage.c_str();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct an error message from a template string
////////////////////////////////////////////////////////////////////////////////
        
std::string Exception::FillExceptionString (int code, 
                                            ...) {
  char const* format = TRI_errno_string(code);
  TRI_ASSERT(format != nullptr);

  char buffer[1024];
  va_list ap;
  va_start(ap, code);
  vsnprintf(buffer, sizeof(buffer) - 1, format, ap);
  va_end(ap);
  buffer[sizeof(buffer) - 1] = '\0'; // Windows

  return std::string(buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief controls whether a backtrace is created for each exception
////////////////////////////////////////////////////////////////////////////////

void Exception::SetVerbose (bool verbose) {
  WithBackTrace = verbose;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
