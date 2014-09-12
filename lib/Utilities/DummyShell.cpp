////////////////////////////////////////////////////////////////////////////////
/// @brief line editor using getline
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

#include "DummyShell.h"

#include "Basics/tri-strings.h"
#include "Basics/files.h"

using namespace std;
using namespace triagens;

// -----------------------------------------------------------------------------
// --SECTION--                                                  class DummyShell
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new editor
////////////////////////////////////////////////////////////////////////////////

DummyShell::DummyShell (std::string const& history,
                        Completer* completer)
  : ShellImplementation(history, completer) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

DummyShell::~DummyShell () {
  close();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor open
////////////////////////////////////////////////////////////////////////////////

bool DummyShell::open (bool) {
  _state = STATE_OPENED;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor shutdown
////////////////////////////////////////////////////////////////////////////////

bool DummyShell::close () {
  _state = STATE_CLOSED;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor prompt
////////////////////////////////////////////////////////////////////////////////

char* DummyShell::prompt (char const* prompt) {
  string dotdot;
  char const* p = prompt;
  size_t len1 = strlen(prompt);
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
    fprintf(stdout, "%s", p);
    fflush(stdout);

    string line;
    getline(cin, line);

    p = dotdot.c_str();

    if (cin.eof()) {
      return nullptr;
    }

    _current += sep;
    sep = "\n";
    ++lineno;

    // remove any prompt at the beginning of the line
    char* result = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, line.c_str());
    bool c1 = strncmp(result, prompt, len1) == 0;
    bool c2 = strncmp(result, dotdot.c_str(), len2) == 0;

    while (c1 || c2) {
      if (c1) {
        result += len1;
      }
      else if (c2) {
        result += len2;
      }

      c1 = strncmp(result, prompt, len1) == 0;
      c2 = strncmp(result, dotdot.c_str(), len2) == 0;
    }

    // extend line and check
    _current += result;

    bool ok = _completer->isComplete(_current, lineno, strlen(result));

    // stop if line is complete
    if (ok) {
      break;
    }
  }

  char* line = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, _current.c_str());

  _current.clear();

  return line;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the history file path
////////////////////////////////////////////////////////////////////////////////

string DummyShell::historyPath () {
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add to history
////////////////////////////////////////////////////////////////////////////////

void DummyShell::addHistory (char const* str) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save history
////////////////////////////////////////////////////////////////////////////////

bool DummyShell::writeHistory () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the characters which the user has typed
/// @arg  is the prompt of the shell
/// Note: this is the interface between our shell world and some implementation
///       of key events (linenoise, readline)
////////////////////////////////////////////////////////////////////////////////

char* DummyShell::getLine (char const* prompt) {
  return this->prompt(prompt);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
