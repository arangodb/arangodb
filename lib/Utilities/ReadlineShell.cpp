////////////////////////////////////////////////////////////////////////////////
/// @brief line editor using readline
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


#include "ReadlineShell.h"
#include "Utilities/Completer.h"
#include "Utilities/LineEditor.h"

#include <iostream>
#include <readline/readline.h>
#include <readline/history.h>

#include "Basics/tri-strings.h"

#include <vector>

#include <v8.h>

using namespace std;
using namespace triagens;

namespace {

  Completer * COMPLETER;

  static char WordBreakCharacters[] = {
    ' ', '\t', '\n', '"', '\\', '\'', '`', '@',
    '<', '>', '=', ';', '|', '&', '{', '}', '(', ')',
    '\0'
  };

  static char* CompletionGenerator (char const* text, int state) {
    static size_t currentIndex;
    static vector<string> result;
    // compute the possible completion
    if(state == 0) {
      COMPLETER->getAlternatives(text, result);
      LineEditor::sortAlternatives(result);
    }

    if (currentIndex < result.size()) {
      return TRI_SystemDuplicateString(result[currentIndex++].c_str());
    }
    else {
      currentIndex = 0;
      result.clear();
      return 0;
    }
  }


  static char** AttemptedCompletion (char const* text, int start, int end) {
    char** result;

    result = rl_completion_matches(text, CompletionGenerator);
    rl_attempted_completion_over = true;

    if (result != 0 && result[0] != 0 && result[1] == 0) {
      size_t n = strlen(result[0]);

      if (result[0][n - 1] == ')') {
        result[0][n - 1] = '\0';
      }
    }

#if RL_READLINE_VERSION >= 0x0500
    // issue #289
    rl_completion_suppress_append = 1;
#endif

    return result;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                               class ReadlineShell
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new editor
////////////////////////////////////////////////////////////////////////////////

ReadlineShell::ReadlineShell(std::string const& history, Completer *completer)
  : ShellImplementation(history, completer) {

    COMPLETER = completer;

    rl_initialize();

    rl_attempted_completion_function = AttemptedCompletion;
    rl_completer_word_break_characters = WordBreakCharacters;

#ifndef __APPLE__
  rl_catch_signals = 0;
#endif
}


// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor open
////////////////////////////////////////////////////////////////////////////////

bool ReadlineShell::open(bool autoComplete) {
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
  //stifle_history(MAX_HISTORY_ENTRIES);
  stifle_history(1000);

  _state = STATE_OPENED;

  return read_history(historyPath().c_str()) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor shutdown
////////////////////////////////////////////////////////////////////////////////

bool ReadlineShell::close() {
  if (_state != STATE_OPENED) {
    // avoid duplicate saving of history
    return true;
  }

  _state = STATE_CLOSED;

  bool res = writeHistory();

  clear_history();
#ifndef __APPLE__
  HIST_ENTRY** hist = history_list();
  if (hist != nullptr) {
    TRI_SystemFree(hist);
  }
#endif

#ifndef __APPLE__
  // reset state of the terminal to what it was before readline()
  rl_cleanup_after_signal();
#endif

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the history file path
////////////////////////////////////////////////////////////////////////////////

string ReadlineShell::historyPath() {
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

void ReadlineShell::addHistory(char const* str) {
  if (*str == '\0') {
    return;
  }

  history_set_pos(history_length-1);

  if (current_history()) {
    do {
      if (strcmp(current_history()->line, str) == 0) {
#ifndef __APPLE__
        HIST_ENTRY* e = remove_history(where_history());
        if (e != nullptr) {
          free_history_entry(e);
        }
#else
        remove_history(where_history());
#endif
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

bool ReadlineShell::writeHistory() {
  return (write_history(historyPath().c_str()) == 0);
}

char * ReadlineShell::getLine(char const * input) {
  return readline(input);
}
// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
