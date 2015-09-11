////////////////////////////////////////////////////////////////////////////////
/// @brief V8 line editor
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "V8LineEditor.h"
#include "Basics/logging.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Utilities/ShellImplementation.h"
#include "Utilities/ShellImplFactory.h"
#include "V8/v8-utils.h"

using namespace std;
using namespace triagens;
using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief signal handler for CTRL-C
////////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32

static void SignalHandler (int signal) {
  // get the instance of the console
  auto instance = V8LineEditor::instance();

  if (instance != nullptr) {
    if (instance->isExecutingCommand()) {
      v8::Isolate* isolate = instance->isolate();
  
      if (! v8::V8::IsExecutionTerminating(isolate)) {
        v8::V8::TerminateExecution(isolate);
      }
    }

    instance->signal();
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 class V8Completer
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 Completer methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool V8Completer::isComplete (std::string const& source, size_t lineno) {
  int openParen    = 0;
  int openBrackets = 0;
  int openBraces   = 0;
  int openStrings  = 0;  // only used for template strings, which can be multi-line
  int openComments = 0;

  enum line_parse_state_e {
    NORMAL,             // start
    NORMAL_1,           // from NORMAL: seen a single /
    DOUBLE_QUOTE,       // from NORMAL: seen a single "
    DOUBLE_QUOTE_ESC,   // from DOUBLE_QUOTE: seen a backslash
    SINGLE_QUOTE,       // from NORMAL: seen a single '
    SINGLE_QUOTE_ESC,   // from SINGLE_QUOTE: seen a backslash
    BACKTICK,           // from NORMAL: seen a single `
    BACKTICK_ESC,       // from BACKTICK: seen a backslash
    MULTI_COMMENT,      // from NORMAL_1: seen a *
    MULTI_COMMENT_1,    // from MULTI_COMMENT, seen a *
    SINGLE_COMMENT      // from NORMAL_1; seen a /
  };

  char const* ptr = source.c_str();
  char const* end = ptr + source.length();
  line_parse_state_e state = NORMAL;

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
    else if (state == BACKTICK) {
      if (*ptr == '\\') {
        state = BACKTICK_ESC;
      }
      else if (*ptr == '`') {
        state = NORMAL;
        --openStrings;
      }

      ++ptr;
    }
    else if (state == BACKTICK_ESC) {
      state = BACKTICK;
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
        --openComments;
      }

      ++ptr;
    }
    else if (state == SINGLE_COMMENT) {
      ++ptr;

      if (ptr == end || *ptr == '\n') {
        state = NORMAL;
        --openComments;
      }
    }
    else if (state == NORMAL_1) {
      switch (*ptr) {
        case '/':
          state = SINGLE_COMMENT;
          ++openComments;
          ++ptr;
          break;

        case '*':
          state = MULTI_COMMENT;
          ++openComments;
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

        case '`':
          state = BACKTICK;
          ++openStrings;
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

  return (openParen <= 0 && openBrackets <= 0 && openBraces <= 0 && openStrings <= 0 && openComments <= 0);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

vector<string> V8Completer::alternatives (char const * text) {
  vector<string> result;

  // locate global object or sub-object
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Handle<v8::Object> current = context->Global();
  string path;
  char* prefix;

  if (*text != '\0') {
    TRI_vector_string_t splitted = TRI_SplitString(text, '.');

    if (1 < splitted._length) {
      for (size_t i = 0;  i < splitted._length - 1;  ++i) {
        v8::Handle<v8::String> name = TRI_V8_STRING(splitted._buffer[i]);

        if (! current->Has(name)) {
          TRI_DestroyVectorString(&splitted);
          return result;
        }

        v8::Handle<v8::Value> val = current->Get(name);

        if (! val->IsObject()) {
          TRI_DestroyVectorString(&splitted);
          return result;
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

  v8::HandleScope scope(isolate);

  // compute all possible completions
  v8::Handle<v8::Array> properties;
  v8::Handle<v8::String> cpl = TRI_V8_ASCII_STRING("_COMPLETIONS");

  if (current->HasOwnProperty(cpl)) {
    v8::Handle<v8::Value> funcVal = current->Get(cpl);

    if (funcVal->IsFunction()) {
      v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(funcVal);
      v8::Handle<v8::Value> args;

      try {
        v8::Handle<v8::Value> cpls = func->Call(current, 0, &args);

        if (cpls->IsArray()) {
          properties = v8::Handle<v8::Array>::Cast(cpls);
        }
      }
      catch (...) {
        // silently ignore errors here
      }
    }
  }
  else {
    properties = current->GetPropertyNames();
  }

  // locate
  try {
    if (! properties.IsEmpty()) {
      uint32_t const n = properties->Length();
      result.reserve(static_cast<size_t>(n));

      for (uint32_t i = 0;  i < n;  ++i) {
        v8::Handle<v8::Value> v = properties->Get(i);

        TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, v);
        char const* s = *str;

        if (s != nullptr && *s) {
          string suffix = (current->Get(v)->IsFunction()) ? "()" : "";
          string name = path + s + suffix;

          if (*prefix == '\0' || TRI_IsPrefixString(s, prefix)) {
            result.emplace_back(name);
          }
        }
      }
    }
  }
  catch (...) {
    // ignore errors in case of OOM
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, prefix);
  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                class V8LineEditor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                          static private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the active instance of the editor
////////////////////////////////////////////////////////////////////////////////

std::atomic<V8LineEditor*> V8LineEditor::_instance(nullptr);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new editor
////////////////////////////////////////////////////////////////////////////////

V8LineEditor::V8LineEditor (v8::Isolate* isolate,
                            v8::Handle<v8::Context> context, 
                            std::string const& history) 
  : LineEditor(history), 
    _isolate(isolate),
    _context(context),  
    _completer(V8Completer()),
    _executingCommand(false) {

  // register global instance
  TRI_ASSERT(_instance.load() == nullptr);
  _instance.store(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the editor
////////////////////////////////////////////////////////////////////////////////

V8LineEditor::~V8LineEditor () {
  // unregister global instance
  TRI_ASSERT(_instance.load() != nullptr);
  _instance.store(nullptr);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the shell implementation supports colors
////////////////////////////////////////////////////////////////////////////////

bool V8LineEditor::supportsColors () const {
  if (_shellImpl == nullptr) {
    return false;
  }

  return _shellImpl->supportsColors();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup a signal handler for CTRL-C
////////////////////////////////////////////////////////////////////////////////

void V8LineEditor::setupCtrlCHandler () {
#ifndef _WIN32
  if (_shellImpl != nullptr &&
      _shellImpl->supportsCtrlCHandler()) {
    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = &SignalHandler;

    int res = sigaction(SIGINT, &sa, 0);

    if (res != 0) {
      LOG_ERROR("unable to install signal handler");
    }
  }
#endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

void V8LineEditor::initializeShell () {
  _shellImpl = ShellImplFactory::buildShell(_history, &_completer);
  
  setupCtrlCHandler();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
