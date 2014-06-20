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
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

Exception::Exception (int code,
                      string const& details,
                      char const* file,
                      int line)
  : _details(details),
    _file(file),
    _line(line),
    _code(code) {
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
  string message("exception in '");
  message.append(_file);
  message.append("' at line ");
  message.append(basics::StringUtils::itoa(_line));
  message.append(": ");
  message += this->message();

  return message.c_str();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return exception message
////////////////////////////////////////////////////////////////////////////////

string Exception::message () const throw () {
  string message(TRI_errno_string(_code));

  if (! _details.empty()) {
    message.append(": ");
    message.append(_details);
  }

  return message;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return exception code
////////////////////////////////////////////////////////////////////////////////

int Exception::code () const throw () {
  return _code;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
