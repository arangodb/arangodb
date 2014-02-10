////////////////////////////////////////////////////////////////////////////////
/// @brief line editor using readline
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "LineEditor.h"

#include <readline/readline.h>
#include <readline/history.h>

#include "BasicsC/tri-strings.h"

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                  class LineEditor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new editor
////////////////////////////////////////////////////////////////////////////////

LineEditor::LineEditor (std::string const& history)
  : _current(),
    _historyFilename(history),
    _state(STATE_NONE) {
  rl_initialize();

#ifndef __APPLE__
  rl_catch_signals = 0;
#endif  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

LineEditor::~LineEditor () {
  close();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor open
////////////////////////////////////////////////////////////////////////////////

bool LineEditor::open (bool autoComplete) {
  if (autoComplete) {

    // issue #289: do not append a space after completion
    rl_completion_append_character = '\0';

    // works only in Readline 4.2+
#if RL_READLINE_VERSION >= 0x0500
    // enable this to turn on the visual bell - evil!
    // rl_variable_bind("prefer-visible-bell", "1");

    // use this for single-line editing as in mongodb shell
    // rl_variable_bind("horizontal-scroll-mode", "1");

    // show matching parentheses
    rl_set_paren_blink_timeout(1 * 1000 * 1000);
    rl_variable_bind("blink-matching-paren", "1");

    // show selection list when completion is ambiguous. not setting this
    // variable will turn the selection list off at least on Ubuntu
    rl_variable_bind("show-all-if-ambiguous", "1");

    // use readline's built-in page-wise completer
    rl_variable_bind("page-completions", "1");
#endif

    rl_bind_key('\t', rl_complete);
  }

  using_history();
  stifle_history(MAX_HISTORY_ENTRIES);

  _state = STATE_OPENED;

  return read_history(historyPath().c_str()) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor shutdown
////////////////////////////////////////////////////////////////////////////////

bool LineEditor::close () {
  if (_state != STATE_OPENED) {
    // avoid duplicate saving of history
    return true;
  }

  _state = STATE_CLOSED;

  bool res = writeHistory();
    
#ifndef __APPLE__
  // reset state of the terminal to what it was before readline()
  rl_cleanup_after_signal();
#endif  

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the history file path
////////////////////////////////////////////////////////////////////////////////

string LineEditor::historyPath () {
  string path;

  if (getenv("HOME")) {
    path.append(getenv("HOME"));
    path += '/';
  }

  path.append(_historyFilename);

  return path;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add to history
////////////////////////////////////////////////////////////////////////////////

void LineEditor::addHistory (char const* str) {
  if (*str == '\0') {
    return;
  }

  history_set_pos(history_length-1);

  if (current_history()) {
    do {
      if (strcmp(current_history()->line, str) == 0) {
        remove_history(where_history());
        break;
      }
    }
    while (previous_history());
  }

  add_history(str);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save history
////////////////////////////////////////////////////////////////////////////////

bool LineEditor::writeHistory () {
  return (write_history(historyPath().c_str()) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor prompt
////////////////////////////////////////////////////////////////////////////////

char* LineEditor::prompt (char const* prompt) {
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
    char* result = readline(p);

    p = dotdot.c_str();

    if (result == 0) {

      // give up, if the user pressed control-D on the top-most level
      if (_current.empty()) {
        return 0;
      }

      // otherwise clear current content
      _current.clear();
      break;
    }

    _current += sep;
    sep = "\n";
    ++lineno;

    // remove any prompt at the beginning of the line
    char* originalLine = result;
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

    bool ok = isComplete(_current, lineno, strlen(result));

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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
