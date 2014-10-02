////////////////////////////////////////////////////////////////////////////////
/// @brief V8 line editor
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

#include "V8LineEditor.h"
#include "Utilities/ShellImplFactory.h"

#include "Basics/tri-strings.h"
#include "V8/v8-utils.h"

#include "Basics/StringUtils.h"

using namespace std;
// -----------------------------------------------------------------------------
// --SECTION--                                                 class V8Completer
// -----------------------------------------------------------------------------

bool V8Completer::isComplete(std::string const& source, size_t lineno, size_t column) {
  char const* ptr;
  char const* end;
  int openParen;
  int openBrackets;
  int openBraces;


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
/// @brief  computes all strings which begins with the given text
////////////////////////////////////////////////////////////////////////////////

void V8Completer::getAlternatives(char const * text, vector<string> & result) {
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
          return;
        }

        v8::Handle<v8::Value> val = current->Get(name);

        if (! val->IsObject()) {
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

  TRI_FreeString(TRI_CORE_MEM_ZONE, prefix);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                class V8LineEditor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new editor
////////////////////////////////////////////////////////////////////////////////

V8LineEditor::V8LineEditor (v8::Handle<v8::Context> context, std::string const& history)
  : LineEditor(history), _context(context),  _completer(V8Completer()) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the editor
////////////////////////////////////////////////////////////////////////////////

V8LineEditor::~V8LineEditor () {
  // nothing
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------
//
void V8LineEditor::initializeShell() {
  ShellImplFactory factory;
  _shellImpl = factory.buildShell(_history, &_completer);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
