////////////////////////////////////////////////////////////////////////////////
/// @brief basic exceptions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Exceptions.h"

using namespace std;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for all errors
////////////////////////////////////////////////////////////////////////////////

TriagensError::TriagensError (string const& type, string const& details, char const* file, int line)
  : _type("unknown"),
    _details(details),
    _file(file),
    _line(line) {
  _message = "exception in '" + _file + "' at line " + StringUtils::itoa(_line) + ": "
           + "type = '" + _type + "'";

  if (! details.empty()) {
    _message += " details = '" + _details + "'";
  }
}



TriagensError::~TriagensError () throw () {
}



char const * TriagensError::what () const throw() {
  return _message.c_str();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for internal errors
////////////////////////////////////////////////////////////////////////////////

InternalError::InternalError (string const& details, char const* file, int line)
  : TriagensError("internal error", details, file, line) {
}



InternalError::InternalError (std::exception const& ex, char const* file, int line)
  : TriagensError("internal exception", ex.what(), file, line) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for out-of-memory errors
////////////////////////////////////////////////////////////////////////////////

OutOfMemoryError::OutOfMemoryError (char const* file, int line)
  : TriagensError("out-of-memory", "", file, line) {
}

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



FileError::~FileError () throw () {
}



void FileError::setFilename (string const& filename) {
  _filename = filename;

  if (! _filename.empty()) {
    _message += " file = '" + _filename + "'";
  }
}

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



void ParseError::setLineNumber (int lineNumber) {
  _lineNumber = lineNumber;

  if (_lineNumber != -1) {
    _message += " line-number = '" + StringUtils::itoa(_lineNumber) + "'";
  }
}

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



ParameterError::~ParameterError () throw () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
