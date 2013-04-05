////////////////////////////////////////////////////////////////////////////////
/// @brief V8 line editor
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

#include "V8LineEditor.h"

#include <readline/readline.h>
#include <readline/history.h>

#include "BasicsC/tri-strings.h"
#include "V8/v8-utils.h"

#if RL_READLINE_VERSION >= 0x0500
#define completion_matches rl_completion_matches
#endif

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                class V8LineEditor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief word break characters
////////////////////////////////////////////////////////////////////////////////

static char WordBreakCharacters[] = {
    ' ', '\t', '\n', '"', '\\', '\'', '`', '@',
    '<', '>', '=', ';', '|', '&', '{', '}', '(', ')',
    '\0'
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8LineEditor
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief completion generator
////////////////////////////////////////////////////////////////////////////////

static char* CompletionGenerator (char const* text, int state) {
  static size_t currentIndex;
  static vector<string> result;
  char* prefix;

  // compute the possible completion
  if (state == 0) {
    if (! v8::Context::InContext()) {
      return 0;
    }

    // locate global object or sub-object
    v8::Handle<v8::Object> current = v8::Context::GetCurrent()->Global();
    string path;

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
      size_t n = properties->Length();

      for (size_t i = 0;  i < n;  ++i) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief attempted completion
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8LineEditor
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new editor
////////////////////////////////////////////////////////////////////////////////

V8LineEditor::V8LineEditor (v8::Handle<v8::Context> context, std::string const& history)
  : LineEditor(history), _context(context) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8LineEditor
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor open
////////////////////////////////////////////////////////////////////////////////

bool V8LineEditor::open (const bool autoComplete) {
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
    // variable will turn the selection list of at least on Ubuntu
    rl_variable_bind("show-all-if-ambiguous", "1");

    // use readlines built-in page-wise completer
    rl_variable_bind("page-completions", "1");
#endif

    rl_attempted_completion_function = AttemptedCompletion;
    rl_completer_word_break_characters = WordBreakCharacters;

    rl_bind_key('\t', rl_complete);
  }

  return LineEditor::open(autoComplete);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup LineEditor
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief check if line is complete
////////////////////////////////////////////////////////////////////////////////

bool V8LineEditor::isComplete (string const& source, size_t, size_t) {
  char const* ptr;
  char const* end;
  int openParen;
  int openBrackets;
  int openBraces;

  enum {
    NORMAL,             // start
    NORMAL_1,           // from NORMAL: seen a single /
    DOUBLE_QUOTE,       // from NORMAL: seen a single "
    DOUBLE_QUOTE_ESC,   // from DOUBLE_QUOTE: seen a backslash
    SINGLE_QUOTE,       // from NORMAL: seen a single '
    SINGLE_QUOTE_ESC,   // from SINGLE_QUOTE: seen a backslash
    MULTI_COMMENT,      // from NORMAL_1: seen a *
    MULTI_COMMENT_1,    // from MULTI_COMMENT, seen a *
    SINGLE_COMMENT      // from NORMAL_1; seen a /
  }
  state;

  openParen = 0;
  openBrackets = 0;
  openBraces = 0;

  ptr = source.c_str();
  end = ptr + source.length();
  state = NORMAL;

  while (ptr < end) {
    if (state == DOUBLE_QUOTE) {
      if (*ptr == '\\') {
        state = DOUBLE_QUOTE_ESC;
      }
      else if (*ptr == '"') {
        state = NORMAL;
      }

      ++ptr;
    }
    else if (state == DOUBLE_QUOTE_ESC) {
      state = DOUBLE_QUOTE;
      ptr++;
    }
    else if (state == SINGLE_QUOTE) {
      if (*ptr == '\\') {
        state = SINGLE_QUOTE_ESC;
      }
      else if (*ptr == '\'') {
        state = NORMAL;
      }

      ++ptr;
    }
    else if (state == SINGLE_QUOTE_ESC) {
      state = SINGLE_QUOTE;
      ptr++;
    }
    else if (state == MULTI_COMMENT) {
      if (*ptr == '*') {
        state = MULTI_COMMENT_1;
      }

      ++ptr;
    }
    else if (state == MULTI_COMMENT_1) {
      if (*ptr == '/') {
        state = NORMAL;
      }

      ++ptr;
    }
    else if (state == SINGLE_COMMENT) {
      ++ptr;

      if (ptr == end) {
        state = NORMAL;
      }
    }
    else if (state == NORMAL_1) {
      switch (*ptr) {
        case '/':
          state = SINGLE_COMMENT;
          ++ptr;
          break;

        case '*':
          state = MULTI_COMMENT;
          ++ptr;
          break;

        default:
          state = NORMAL; // try again, do not change ptr
          break;
      }
    }
    else {
      switch (*ptr) {
        case '"':
          state = DOUBLE_QUOTE;
          break;

        case '\'':
          state = SINGLE_QUOTE;
          break;

        case '/':
          state = NORMAL_1;
          break;

        case '(':
          ++openParen;
          break;

        case ')':
          --openParen;
          break;

        case '[':
          ++openBrackets;
          break;

        case ']':
          --openBrackets;
          break;

        case '{':
          ++openBraces;
          break;

        case '}':
          --openBraces;
          break;

        case '\\':
          ++ptr;
          break;
      }

      ++ptr;
    }
  }

  return openParen <= 0 && openBrackets <= 0 && openBraces <= 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
