////////////////////////////////////////////////////////////////////////////////
/// @brief V8 line editor
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "MRLineEditor.h"

#include <readline/readline.h>
#include <readline/history.h>

#include "BasicsC/strings.h"

#if RL_READLINE_VERSION >= 0x0500
#define completion_matches rl_completion_matches
#endif

#ifdef TRI_ENABLE_MRUBY
extern "C" {
#include "mruby.h"
#include "compile.h"
}
#endif

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                class MRLineEditor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief word break characters
////////////////////////////////////////////////////////////////////////////////

#if 0
static char WordBreakCharacters[] = {
    ' ', '\t', '\n', '"', '\\', '\'', '`', '@', 
    '<', '>', '=', ';', '|', '&', '{', '}', '(', ')',
    '\0'
};
#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRLineEditor
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief completion generator
////////////////////////////////////////////////////////////////////////////////

static char* CompletionGenerator (char const* text, int state) {
  return 0;

#if 0
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
                  
        v8::String::Utf8Value str(v);
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

    TRI_FreeString(prefix);
  }

  if (currentIndex < result.size()) {
    return TRI_DuplicateString(result[currentIndex++].c_str());
  }
  else {
    result.clear();
    return 0;
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief attempted completion
////////////////////////////////////////////////////////////////////////////////

#if 0
static char** AttemptedCompletion (char const* text, int start, int end) {
  char** result;

  result = completion_matches(text, CompletionGenerator);
  rl_attempted_completion_over = true;

  if (result != 0 && result[0] != 0 && result[1] == 0) {
    size_t n = strlen(result[0]);

    if (result[0][n-1] == ')') {
      result[0][n-1] = '\0';

#if RL_READLINE_VERSION >= 0x0500
      rl_completion_suppress_append = 1;
#endif
    }
  }

  return result;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRLineEditor
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new editor
////////////////////////////////////////////////////////////////////////////////

MRLineEditor::MRLineEditor (MR_state_t* mrs, string const& history)
  : LineEditor(history), _current(), _mrs(mrs) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRLineEditor
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor open
////////////////////////////////////////////////////////////////////////////////

bool MRLineEditor::open (const bool autoComplete) {
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

bool MRLineEditor::isComplete (string const& source, size_t lineno, size_t column) {
 
#ifdef TRI_ENABLE_MRUBY 
  char const* msg = "syntax error, unexpected $end";
  char* text = TRI_DuplicateString(source.c_str());

  struct mrb_parser_state* p = mrb_parse_nstring_ext(&_mrs->_mrb, text, source.size());
  TRI_FreeString(TRI_CORE_MEM_ZONE, text);

  // out of memory?
  if (p == 0) {
    return true;
  }

  // no error or strange error
  if (p->tree != 0) {
    return true;
  }

  // check for end-of-line
  if (0 < p->nerr) {
    if (TRI_EqualString2(p->error_buffer[0].message, msg, strlen(msg))) {
      return false;
    }
  }
#endif

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
