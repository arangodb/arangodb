////////////////////////////////////////////////////////////////////////////////
/// @brief line editor using linenoise
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

#include "LinenoiseShell.h"

extern "C" {
#include <linenoise.h>
}

#include <vector>

#include "BasicsC/tri-strings.h"
#include "BasicsC/files.h"

using namespace std;
using namespace triagens;

namespace {
  static void LinenoiseCompletionGenerator(char const* text, linenoiseCompletions * lc) {
    vector<string> completions;
    // locate global object or sub-object
    v8::Handle<v8::Object> current = v8::Context::GetCurrent()->Global();
    string path;
    char* prefix;

    if (*text != '\0') {
      TRI_vector_string_t splitted = TRI_SplitString(text, '.');

      if (1 < splitted._length) {
        for (size_t i = 0; i < splitted._length - 1; ++i) {
          v8::Handle<v8::String> name = v8::String::New(splitted._buffer[i]);

          if (!current->Has(name)) {
            TRI_DestroyVectorString(&splitted);
            return;
          }

          v8::Handle<v8::Value> val = current->Get(name);

          if (!val->IsObject()) {
            TRI_DestroyVectorString(&splitted);
            return;
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
    if (!properties.IsEmpty()) {
      const uint32_t n = properties->Length();

      for (uint32_t i = 0; i < n; ++i) {
        v8::Handle<v8::Value> v = properties->Get(i);

        TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, v);
        char const* s = *str;

        if (s != 0 && *s) {
          string suffix = (current->Get(v)->IsFunction()) ? "()" : "";
          string name = path + s + suffix;

          if (*prefix == '\0' || TRI_IsPrefixString(s, prefix)) {
            linenoiseAddCompletion(lc, name.c_str());
            completions.push_back(name);
          }
        }
      }
    }


    lc->multiLine = 1;
    TRI_FreeString(TRI_CORE_MEM_ZONE, prefix);

  }

  static void AttemptedCompletion(char const* text, linenoiseCompletions * lc) {
    LinenoiseCompletionGenerator(text, lc);
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
  linenoiseSetCompletionCallback(AttemptedCompletion);
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

char * LinenoiseShell::getLine(char const * input) {
  return linenoise(input);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
