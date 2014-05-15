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

#include "ReadlineShell.h"
#include "Completer.h"

#include <readline/readline.h>
#include <readline/history.h>

#include "BasicsC/tri-strings.h"

using namespace std;
using namespace triagens;

namespace {
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
    if (! v8::Context::InContext()) {
      return 0;
    }

    // locate global object or sub-object
    v8::Handle<v8::Object> current = v8::Context::GetCurrent()->Global();
    string path;
    char* prefix;

    if (*text != '\0') {
      TRI_vector_string_t splitted = TRI_SplitString(text, '.');

      if (1 < splitted._length) {
        for (size_t i = 0;  i < splitted._length - 1;  ++i) {
          v8::Handle<v8::String> name = v8::String::New(splitted._buffer[i]);

          if (! current->Has(name)) {
            TRI_DestroyVectorString(&splitted);
            return 0;
          }

          v8::Handle<v8::Value> val = current->Get(name);

          if (! val->IsObject()) {
            TRI_DestroyVectorString(&splitted);
            return 0;
          }

          current = val->ToObject();
          path = path + splitted._buffer[i] + ".";
        }

        prefix = TRI_DuplicateString(splitted._buffer[splitted._length - 1]);
      }
      else {
        prefix = TRI_DuplicateString(text);
      }

      TRI_DestroyVectorString(&splitted);
    }
    else {
      prefix = TRI_DuplicateString(text);
    }

    v8::HandleScope scope;

    // compute all possible completions
    v8::Handle<v8::Array> properties;
    v8::Handle<v8::String> cpl = v8::String::New("_COMPLETIONS");

    if (current->HasOwnProperty(cpl)) {
      v8::Handle<v8::Value> funcVal = current->Get(cpl);

      if (funcVal->IsFunction()) {
        v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(funcVal);
        v8::Handle<v8::Value> args;
        v8::Handle<v8::Value> cpls = func->Call(current, 0, &args);

        if (cpls->IsArray()) {
          properties = v8::Handle<v8::Array>::Cast(cpls);
        }
      }
    }
    else {
      properties = current->GetPropertyNames();
    }

    // locate
    if (! properties.IsEmpty()) {
      const uint32_t n = properties->Length();

      for (uint32_t i = 0;  i < n;  ++i) {
        v8::Handle<v8::Value> v = properties->Get(i);

        TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, v);
        char const* s = *str;

        if (s != 0 && *s) {
          string suffix = (current->Get(v)->IsFunction()) ? "()" : "";
          string name = path + s + suffix;

          if (*prefix == '\0' || TRI_IsPrefixString(s, prefix)) {
            result.push_back(name);
          }
        }
      }
    }

    currentIndex = 0;

    TRI_FreeString(TRI_CORE_MEM_ZONE, prefix);
  }

  if (currentIndex < result.size()) {
    return TRI_SystemDuplicateString(result[currentIndex++].c_str());
  }
  else {
    result.clear();
    return 0;
  }
}


static char** AttemptedCompletion (char const* text, int start, int end) {
  char** result;

  result = completion_matches(text, CompletionGenerator);
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
  stifle_history(MAX_HISTORY_ENTRIES);

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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
