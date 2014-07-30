////////////////////////////////////////////////////////////////////////////////
/// @brief line editor using linenoise
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

#include "LinenoiseShell.h"
#include "Utilities/Completer.h"
#include "Utilities/LineEditor.h"

extern "C" {
#include <linenoise.h>
}

#include "BasicsC/files.h"

using namespace std;
using namespace triagens;

namespace {
  static Completer * COMPLETER;

  static void LinenoiseCompletionGenerator(char const* text, linenoiseCompletions * lc) {
    vector<string> alternatives;

    if (COMPLETER) {
      COMPLETER->getAlternatives(text, alternatives);
      LineEditor::sortAlternatives(alternatives);

      for (vector<string>::const_iterator i = alternatives.begin(); i != alternatives.end(); ++i) {
        linenoiseAddCompletion(lc, (*i).c_str());
      }
    }
    lc->multiLine = 1;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                              class LinenoiseShell
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new editor
////////////////////////////////////////////////////////////////////////////////

LinenoiseShell::LinenoiseShell(std::string const& history, Completer * completer)
: ShellImplementation(history, completer) {
  COMPLETER = completer;
  linenoiseSetCompletionCallback(LinenoiseCompletionGenerator);
}

LinenoiseShell::~LinenoiseShell() {
  COMPLETER = 0;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor open
////////////////////////////////////////////////////////////////////////////////

bool LinenoiseShell::open(bool) {
  linenoiseHistoryLoad(historyPath().c_str());

  _state = STATE_OPENED;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor shutdown
////////////////////////////////////////////////////////////////////////////////

bool LinenoiseShell::close() {
  if (_state != STATE_OPENED) {
    // avoid duplicate saving of history
    return true;
  }

  _state = STATE_CLOSED;

  return writeHistory();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the history file path
////////////////////////////////////////////////////////////////////////////////

string LinenoiseShell::historyPath() {
  string path;

  // get home directory
  char* p = TRI_HomeDirectory();

  if (p != 0) {
    path.append(p);
    TRI_Free(TRI_CORE_MEM_ZONE, p);

    if (!path.empty() && path[path.size() - 1] != TRI_DIR_SEPARATOR_CHAR) {
      path.push_back(TRI_DIR_SEPARATOR_CHAR);
    }
  }

  path.append(_historyFilename);

  return path;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add to history
////////////////////////////////////////////////////////////////////////////////

void LinenoiseShell::addHistory(char const* str) {
  if (*str == '\0') {
    return;
  }

  linenoiseHistoryAdd(str);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save history
////////////////////////////////////////////////////////////////////////////////

bool LinenoiseShell::writeHistory() {
  linenoiseHistorySave(historyPath().c_str());

  return true;
}

char* LinenoiseShell::getLine(char const* input) {
  return linenoise(input);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
