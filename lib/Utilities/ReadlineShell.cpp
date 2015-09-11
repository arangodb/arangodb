////////////////////////////////////////////////////////////////////////////////
/// @brief line editor using readline
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ReadlineShell.h"

#include "Basics/tri-strings.h"
#include "Utilities/Completer.h"
#include "Utilities/LineEditor.h"

#include <readline/readline.h>
#include <readline/history.h>
#include <v8.h>

using namespace std;
using namespace triagens;
using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

namespace {
  Completer* COMPLETER;
}

static char WordBreakCharacters[] = {
  ' ', '\t', '\n', '"', '\\', '\'', '`', '@',
  '<', '>', '=', ';', '|', '&', '{', '}', '(', ')',
  '\0'
};

static char* CompletionGenerator (char const* text, int state) {
  static size_t currentIndex;
  static vector<string> result;

  // compute the possible completion
  if (state == 0) {
    result = COMPLETER->alternatives(text);
    LineEditor::sortAlternatives(result);
  }

  if (currentIndex < result.size()) {
    return TRI_SystemDuplicateString(result[currentIndex++].c_str());
  }
      
  currentIndex = 0;
  result.clear();
  return nullptr;
}

static char** AttemptedCompletion (char const* text, int start, int end) {
  char** result = rl_completion_matches(text, CompletionGenerator);
  rl_attempted_completion_over = true;

  if (result != nullptr && result[0] != nullptr && result[1] == nullptr) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function that readline calls periodically while waiting
/// for input and being idle
////////////////////////////////////////////////////////////////////////////////

static int ReadlineIdle () {
  auto instance = ReadlineShell::instance();

  if (instance != nullptr && instance->getLoopState() == 2) {
    rl_done = 1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function that readline calls when the input is completed
////////////////////////////////////////////////////////////////////////////////

static void ReadlineInputCompleted (char* value) {
  // if we don't clear the prompt here, readline will display it instantly
  // the user pressed the return key. this is not desired because when we
  // wait for input afterwards, readline will display the prompt again
  rl_set_prompt("");
  
  auto instance = ReadlineShell::instance();

  if (instance != nullptr) {
    if (instance->getLoopState() == 2) {

      // CTRL-C received
      rl_done = 1;

      // replace current input with nothing
      rl_replace_line("", 0);

      instance->setLastInput("");
    }
    else if (value == nullptr) {
      rl_done = 1;
      rl_replace_line("", 0);
      instance->setLoopState(3);
      instance->setLastInput("");
    }
    else {
      instance->setLoopState(1);
      instance->setLastInput(value == 0 ? "" : value);
    }
  }
    
  if (value != nullptr) {
    // avoid memleak
    TRI_SystemFree(value);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                               class ReadlineShell
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief (sole) instance of a ReadlineShell
////////////////////////////////////////////////////////////////////////////////

std::atomic<ReadlineShell*> ReadlineShell::_instance(nullptr);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new editor
////////////////////////////////////////////////////////////////////////////////

ReadlineShell::ReadlineShell (std::string const& history, Completer* completer)
  : ShellImplementation(history, completer),
    _loopState(0),
    _lastInput(),
    _lastInputWasEmpty(false) {

  COMPLETER = completer;

  rl_initialize();

  rl_attempted_completion_function = AttemptedCompletion;
  rl_completer_word_break_characters = WordBreakCharacters;

  // register ourselves
  TRI_ASSERT(_instance == nullptr);
  _instance = this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ReadlineShell::~ReadlineShell () {
  // unregister ourselves
  _instance = nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                       ShellImplementation methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief handle a signal
////////////////////////////////////////////////////////////////////////////////

void ReadlineShell::signal () {
  // set the global state, so the readline input loop can react on it
  setLoopState(2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor open
////////////////////////////////////////////////////////////////////////////////

bool ReadlineShell::open (bool autoComplete) {
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

bool ReadlineShell::close () {
  if (_state != STATE_OPENED) {
    // avoid duplicate saving of history
    return true;
  }

  _state = STATE_CLOSED;

  bool res = writeHistory();

  clear_history();

  HIST_ENTRY** hist = history_list();

  if (hist != nullptr) {
    TRI_SystemFree(hist);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the history file path
////////////////////////////////////////////////////////////////////////////////

string ReadlineShell::historyPath () {
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

void ReadlineShell::addHistory (const string& str) {
  if (str.empty()) {
    return;
  }

  history_set_pos(history_length - 1);

  if (current_history()) {
    do {
      if (strcmp(current_history()->line, str.c_str()) == 0) {
        HIST_ENTRY* e = remove_history(where_history());

        if (e != nullptr) {
          free_history_entry(e);
        }

        break;
      }
    }
    while (previous_history());
  }

  add_history(str.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save history
////////////////////////////////////////////////////////////////////////////////

bool ReadlineShell::writeHistory () {
  return (write_history(historyPath().c_str()) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read a line from the input
////////////////////////////////////////////////////////////////////////////////

string ReadlineShell::getLine (const string& prompt, bool& eof) {
  setLoopState(0);

  rl_event_hook = ReadlineIdle;
  rl_callback_handler_install(prompt.c_str(), ReadlineInputCompleted);

  int state;

  eof = false;

  do {
    rl_callback_read_char();
    state = getLoopState();
  }
  while (state == 0);

  rl_callback_handler_remove();

  if (state == 2) {
    setLastInput("");

    if (_lastInputWasEmpty) {
      eof = true;
    }
    else {
      _lastInputWasEmpty = true;
    }
  }
  else if (state == 3) {
    setLastInput("");
    eof = true;
    _lastInputWasEmpty = false;
  }
  else {
    _lastInputWasEmpty = false;
  }

  return _lastInput; 
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
