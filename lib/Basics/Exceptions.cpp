////////////////////////////////////////////////////////////////////////////////
/// @brief basic exceptions
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Exceptions.h"

using namespace std;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief controls if backtraces are printed with exceptions
////////////////////////////////////////////////////////////////////////////////

static bool WithBackTrace = false;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                               class TriagensError
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for all errors
////////////////////////////////////////////////////////////////////////////////

TriagensError::TriagensError (string const& type, string const& details, char const* file, int line)
  : _type(type),
    _details(details),
    _file(file),
    _line(line) {
  _message = "exception in '" + _file + "' at line " + StringUtils::itoa(_line) + ": "
           + "type = '" + _type + "'";

  if (! details.empty()) {
    _message += " details = '" + _details + "'";
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
#if HAVE_BACKTRACE
  TRI_GetBacktrace(_message);
#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

TriagensError::~TriagensError () throw () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the error messages
////////////////////////////////////////////////////////////////////////////////

char const * TriagensError::what () const throw() {
  return _message.c_str();
}

// -----------------------------------------------------------------------------
// --SECTION--                                               class InternalError
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for internal errors
////////////////////////////////////////////////////////////////////////////////

InternalError::InternalError (string const& details, char const* file, int line)
  : TriagensError("internal error", details, file, line) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for internal errors
////////////////////////////////////////////////////////////////////////////////

InternalError::InternalError (std::exception const& ex, char const* file, int line)
  : TriagensError("internal exception", ex.what(), file, line) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

InternalError::~InternalError () throw () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                            class OutOfMemoryError
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for out-of-memory errors
////////////////////////////////////////////////////////////////////////////////

OutOfMemoryError::OutOfMemoryError (char const* file, int line)
  : TriagensError("out-of-memory", "", file, line) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

OutOfMemoryError::~OutOfMemoryError () throw () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   class FileError
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for file errors
////////////////////////////////////////////////////////////////////////////////

FileError::FileError (string const& func,
                      string const& details,
                      string const& filename,
                      string const& mode,
                      int error,
                      char const* file,
                      int line)
  : TriagensError("file-error", details, file, line),
    _filename(filename),
    _mode(mode),
    _error(error) {
  if (! mode.empty()) {
    _message += " mode = '" + _mode + "'";
  }

  if (_error != 0) {
    _message += " errno = " + StringUtils::itoa(_error) + ""
             +  " error = '" + strerror(_error) + "'";
  }

  if (! _filename.empty()) {
    _message += " file = '" + _filename + "'";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

FileError::~FileError () throw () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the filename
////////////////////////////////////////////////////////////////////////////////

void FileError::setFilename (string const& filename) {
  _filename = filename;

  if (! _filename.empty()) {
    _message += " file = '" + _filename + "'";
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  class ParseError
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for parse errors
////////////////////////////////////////////////////////////////////////////////

ParseError::ParseError (string const& details,
                        int lineNumber,
                        char const* file,
                        int line)
  : TriagensError("parse-error", details, file, line),
    _lineNumber(lineNumber) {
  if (_lineNumber != -1) {
    _message += " line-number = '" + StringUtils::itoa(_lineNumber) + "'";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ParseError::~ParseError () throw () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the line number
////////////////////////////////////////////////////////////////////////////////

void ParseError::setLineNumber (int lineNumber) {
  _lineNumber = lineNumber;

  if (_lineNumber != -1) {
    _message += " line-number = '" + StringUtils::itoa(_lineNumber) + "'";
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                              class ParameterError
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for parameter errors
////////////////////////////////////////////////////////////////////////////////

ParameterError::ParameterError (string const& parameter,
                                string const& details,
                                string const& func,
                                char const* file,
                                int line)
  : TriagensError("parameter-error", details, file, line),
    _parameter(parameter),
    _func(func) {
  _message += " parameter = '" + _parameter + "'";

  if (! _func.empty()) {
    _message += " func = '" + _func + "'";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ParameterError::~ParameterError () throw () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   class Exception
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief controls whether a backtrace is created for each exception
////////////////////////////////////////////////////////////////////////////////

void Exception::SetVerbose (bool verbose) {
  WithBackTrace = verbose;
}

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
/// @brief returns the error message
////////////////////////////////////////////////////////////////////////////////

string Exception::message () const throw () {
  return _errorMessage;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the error code
////////////////////////////////////////////////////////////////////////////////

int Exception::code () const throw () {
  return _code;
}
        
////////////////////////////////////////////////////////////////////////////////
/// @brief adds to the message
////////////////////////////////////////////////////////////////////////////////

void Exception::addToMessage (string More) {
  _errorMessage += More;
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

const char* Exception::what () const throw () {
  return _errorMessage.c_str();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct an error message from a template string
////////////////////////////////////////////////////////////////////////////////
        
std::string Exception::FillExceptionString (int code, ...) {
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
  buffer[sizeof(buffer) - 1] = '\0'; // Windows

  return std::string(buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct an error message from a template string
////////////////////////////////////////////////////////////////////////////////
        
std::string Exception::FillFormatExceptionString (char const* format, ...) {
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
  buffer[sizeof(buffer) - 1] = '\0'; // Windows

  return std::string(buffer);
}


// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
