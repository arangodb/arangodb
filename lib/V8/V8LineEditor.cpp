////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

#include "V8LineEditor.h"

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "Basics/operating-system.h"
#include "Basics/tri-strings.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Utilities/Completer.h"
#include "Utilities/ShellBase.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif

using namespace arangodb;

namespace {
static arangodb::Mutex singletonMutex;
static arangodb::V8LineEditor* singleton = nullptr;
}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// @brief the active instance of the editor
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief signal handler for CTRL-C
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

static bool SignalHandler(DWORD eventType) {
  switch (eventType) {
    case CTRL_BREAK_EVENT:
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT: {
      // get the instance of the console
      MUTEX_LOCKER(mutex, ::singletonMutex);
      auto instance = ::singleton;

      if (instance != nullptr) {
        if (instance->isExecutingCommand()) {
          v8::Isolate* isolate = instance->isolate();

          if (!isolate->IsExecutionTerminating()) {
            isolate->TerminateExecution();
          }
        }

        instance->signal();
      }

      return true;
    }
    default: { return true; }
  }
}

#else

static void SignalHandler(int /*signal*/) {
  // get the instance of the console
  MUTEX_LOCKER(mutex, ::singletonMutex);
  auto instance = ::singleton;

  if (instance != nullptr) {
    if (instance->isExecutingCommand()) {
      v8::Isolate* isolate = instance->isolate();

      if (!isolate->IsExecutionTerminating()) {
        isolate->TerminateExecution();
      }
    }

    instance->signal();
  }
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief V8Completer
////////////////////////////////////////////////////////////////////////////////

namespace {
class V8Completer : public Completer {
 public:
  V8Completer() {}

  ~V8Completer() = default;

 public:
  bool isComplete(std::string const& source, size_t /*lineno*/) override final {
    int openParen = 0;
    int openBrackets = 0;
    int openBraces = 0;
    int openStrings = 0;  // only used for template strings, which can be multi-line
    int openComments = 0;

    enum line_parse_state_e {
      NORMAL,            // start
      NORMAL_1,          // from NORMAL: seen a single /
      DOUBLE_QUOTE,      // from NORMAL: seen a single "
      DOUBLE_QUOTE_ESC,  // from DOUBLE_QUOTE: seen a backslash
      SINGLE_QUOTE,      // from NORMAL: seen a single '
      SINGLE_QUOTE_ESC,  // from SINGLE_QUOTE: seen a backslash
      BACKTICK,          // from NORMAL: seen a single `
      BACKTICK_ESC,      // from BACKTICK: seen a backslash
      MULTI_COMMENT,     // from NORMAL_1: seen a *
      MULTI_COMMENT_1,   // from MULTI_COMMENT, seen a *
      SINGLE_COMMENT     // from NORMAL_1; seen a /
    };

    char const* ptr = source.c_str();
    char const* end = ptr + source.length();
    line_parse_state_e state = NORMAL;

    while (ptr < end) {
      if (state == DOUBLE_QUOTE) {
        if (*ptr == '\\') {
          state = DOUBLE_QUOTE_ESC;
        } else if (*ptr == '"') {
          state = NORMAL;
        }

        ++ptr;
      } else if (state == DOUBLE_QUOTE_ESC) {
        state = DOUBLE_QUOTE;
        ptr++;
      } else if (state == SINGLE_QUOTE) {
        if (*ptr == '\\') {
          state = SINGLE_QUOTE_ESC;
        } else if (*ptr == '\'') {
          state = NORMAL;
        }

        ++ptr;
      } else if (state == SINGLE_QUOTE_ESC) {
        state = SINGLE_QUOTE;
        ptr++;
      } else if (state == BACKTICK) {
        if (*ptr == '\\') {
          state = BACKTICK_ESC;
        } else if (*ptr == '`') {
          state = NORMAL;
          --openStrings;
        }

        ++ptr;
      } else if (state == BACKTICK_ESC) {
        state = BACKTICK;
        ptr++;
      } else if (state == MULTI_COMMENT) {
        if (*ptr == '*') {
          state = MULTI_COMMENT_1;
        }

        ++ptr;
      } else if (state == MULTI_COMMENT_1) {
        if (*ptr == '/') {
          state = NORMAL;
          --openComments;
        }

        ++ptr;
      } else if (state == SINGLE_COMMENT) {
        ++ptr;

        if (ptr == end || *ptr == '\n') {
          state = NORMAL;
          --openComments;
        }
      } else if (state == NORMAL_1) {
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
            state = NORMAL;  // try again, do not change ptr
            break;
        }
      } else {
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

    return (openParen <= 0 && openBrackets <= 0 && openBraces <= 0 &&
            openStrings <= 0 && openComments <= 0);
  }

  std::vector<std::string> alternatives(char const* text) override final {
    std::vector<std::string> result;

    // locate global object or sub-object
    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Handle<v8::Object> current = context->Global();
    std::string path;
    std::string prefix;

    if (*text != '\0') {
      std::vector<std::string> splitted = basics::StringUtils::split(text, '.');

      if (1 < splitted.size()) {
        for (size_t i = 0; i < splitted.size() - 1; ++i) {
          v8::Local<v8::String> name = TRI_V8_STD_STRING(isolate, splitted[i]);

          if (!current->Has(context, name).FromMaybe(false)) {
            return result;
          }

          v8::MaybeLocal<v8::Value> maybeVal = current->Get(context, name);

          if (maybeVal.IsEmpty()) {
            return result;
          }

          v8::Local<v8::Value> val = maybeVal.FromMaybe(v8::Local<v8::Value>());
          if (val.IsEmpty() || !val->IsObject()) {
            return result;
          }

          current = val->ToObject(context).FromMaybe(v8::Local<v8::Object>());
          path += splitted[i] + '.';
        }

        prefix = std::move(splitted[splitted.size() - 1]);
      } else {
        prefix = text;
      }
    } else {
      prefix = std::string(text);
    }

    v8::HandleScope scope(isolate);

    // compute all possible completions
    v8::Handle<v8::Array> properties;
    v8::Local<v8::String> cpl = TRI_V8_ASCII_STRING(isolate, "_COMPLETIONS");

    if (current->HasOwnProperty(context, cpl).FromMaybe(false)) {
      v8::Handle<v8::Value> funcVal = current->Get(context, cpl).FromMaybe(v8::Local<v8::Value>());

      if (funcVal->IsFunction()) {
        v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(funcVal);
        // assign a dummy entry to the args array even if we don't need it.
        // this prevents "error C2466: cannot allocate an array of constant size
        // 0" in MSVC
        v8::Handle<v8::Value> args[] = {v8::Null(isolate)};

        try {
          v8::Handle<v8::Value> cpls = func->Call(context, current, 0, args).FromMaybe(v8::Handle<v8::Value>());

          if (cpls->IsArray()) {
            properties = v8::Handle<v8::Array>::Cast(cpls);
          }
        } catch (...) {
          // silently ignore errors here
        }
      }
    } else {
      properties = current->GetPropertyNames(context).FromMaybe(v8::Handle<v8::Array>());
    }

    // locate
    try {
      if (!properties.IsEmpty()) {
        uint32_t const n = properties->Length();
        result.reserve(static_cast<size_t>(n));

        for (uint32_t i = 0; i < n; ++i) {
          v8::Handle<v8::Value> v = properties->Get(context, i).FromMaybe(v8::Handle<v8::Value>());

          TRI_Utf8ValueNFC str(isolate, v);
          char const* s = *str;

          if (s != nullptr && *s) {
            std::string suffix = (current->Get(context, v).FromMaybe(v8::Local<v8::Value>())->IsFunction()) ? "()" : "";
            std::string name = path + s + suffix;

            if (prefix.empty() || prefix[0] == '\0' ||
                TRI_IsPrefixString(s, prefix.c_str())) {
              result.emplace_back(name);
            }
          }
        }
      }
    } catch (...) {
      // ignore errors in case of OOM
    }

    return result;
  }
};
}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new editor
////////////////////////////////////////////////////////////////////////////////

V8LineEditor::V8LineEditor(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                           std::string const& history)
    : LineEditor(), _isolate(isolate), _context(context), _executingCommand(false) {
  // register global instance

  {
    MUTEX_LOCKER(mutex, ::singletonMutex);
    TRI_ASSERT(::singleton == nullptr);
    ::singleton = this;
  }

  // create shell
  _shell = ShellBase::buildShell(history, new V8Completer());

// handle control-c
#ifdef _WIN32
  int res = SetConsoleCtrlHandler((PHANDLER_ROUTINE)SignalHandler, true);

  if (res == 0) {
    LOG_TOPIC("f87ea", ERR, arangodb::Logger::FIXME)
        << "unable to install signal handler";
  }

#else
  struct sigaction sa;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = &SignalHandler;

  int res = sigaction(SIGINT, &sa, nullptr);

  if (res != 0) {
    LOG_TOPIC("d7234", ERR, arangodb::Logger::FIXME)
        << "unable to install signal handler";
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the editor
////////////////////////////////////////////////////////////////////////////////

V8LineEditor::~V8LineEditor() {
  // unregister global instance
  MUTEX_LOCKER(mutex, ::singletonMutex);
  TRI_ASSERT(::singleton != nullptr);
  ::singleton = nullptr;
}
