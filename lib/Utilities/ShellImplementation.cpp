////////////////////////////////////////////////////////////////////////////////
/// @brief implementation of basis class ShellImplementation
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ShellImplementation.h"

#include "Basics/tri-strings.h"

using namespace std;

namespace triagens {

// -----------------------------------------------------------------------------
// --SECTION--                                         class ShellImplementation
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new editor
////////////////////////////////////////////////////////////////////////////////

ShellImplementation::ShellImplementation (string const& history, 
                                          Completer* completer)
  : _current(),
    _historyFilename(history),
    _state(STATE_NONE),
    _completer(completer) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ShellImplementation::~ShellImplementation () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor prompt
////////////////////////////////////////////////////////////////////////////////

char* ShellImplementation::prompt (char const* the_prompt) {
  string dotdot;
  char const* p = the_prompt;
  size_t len1 = strlen(the_prompt);
  size_t len2 = len1;
  size_t lineno = 0;

  if (len1 < 3) {
    dotdot = "> ";
    len2 = 2;
  }
  else {
    dotdot = string(len1 - 2, '.') + "> ";
  }

  char const* sep = "";

  while (true) {
    // calling concrete implmentation of the shell
    char* result = getLine(p);

    p = dotdot.c_str();

    if (result == nullptr) {

      // give up, if the user pressed control-D on the top-most level
      if (_current.empty()) {
        return nullptr;
      }

      // otherwise clear current content
      _current.clear();
      break;
    }

    _current += sep;
    sep = "\n";
    ++lineno;

    // remove any the_prompt at the beginning of the line
    char* originalLine = result;
    bool c1 = strncmp(result, the_prompt, len1) == 0;
    bool c2 = strncmp(result, dotdot.c_str(), len2) == 0;

    while (c1 || c2) {
      if (c1) {
        result += len1;
      }
      else if (c2) {
        result += len2;
      }

      c1 = strncmp(result, the_prompt, len1) == 0;
      c2 = strncmp(result, dotdot.c_str(), len2) == 0;
    }

    // extend line and check
    _current += result;

    bool ok = _completer->isComplete(_current, lineno, strlen(result));

    // cannot use TRI_Free, because it was allocated by the system call readline
    TRI_SystemFree(originalLine);

    // stop if line is complete
    if (ok) {
      break;
    }
  }

  char* line = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, _current.c_str());
  _current.clear();

  return line;
}

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
