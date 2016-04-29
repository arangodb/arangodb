////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_BASICS_TIMERS_H
#define LIB_BASICS_TIMERS_H 1

#include "Basics/Common.h"

#ifdef USE_DEV_TIMERS

#define TIMER_START(name)                                            \
  arangodb::basics::Timers::starts[arangodb::basics::Timers::name] = \
      TRI_microtime()
#define TIMER_STOP(name)                                                \
  arangodb::basics::Timers::totals[arangodb::basics::Timers::name] +=   \
      TRI_microtime() -                                                 \
      arangodb::basics::Timers::starts[arangodb::basics::Timers::name]; \
  ++arangodb::basics::Timers::counts[arangodb::basics::Timers::name]

#else

#define TIMER_START(name)  // do { } while (false)
#define TIMER_STOP(name)   // do { } while (false)

#endif

namespace arangodb {
namespace basics {

class Timers {
  Timers() = delete;

 public:
  enum TimerType : int {
    TIMER_MIN = 0,

    JS_INSERT_ALL,
    JS_INSERT_VPACK_TO_V8,
    JS_INSERT_V8_TO_VPACK,
    JS_INSERT_V8_TO_VPACK2,
    JS_INSERT_CREATE_TRX,
    JS_INSERT_INSERT,
    
    JS_DOCUMENT_ALL,
    JS_DOCUMENT_CREATE_TRX,
    JS_DOCUMENT_DOCUMENT,
    JS_DOCUMENT_VPACK_TO_V8,

    TRANSACTION_DOCUMENT_LOCAL,
    TRANSACTION_DOCUMENT_DOCUMENT_DOCUMENT,
    TRANSACTION_DOCUMENT_WORK_FOR_ONE,
    TRANSACTION_DOCUMENT_EXTRACT,

    TRANSACTION_INSERT_DOCUMENT_INSERT,
    TRANSACTION_INSERT_LOCAL,
    TRANSACTION_INSERT_BUILD_DOCUMENT_IDENTITY,
    TRANSACTION_INSERT_WORK_FOR_ONE,
    TRANSACTION_NEW_OBJECT_FOR_INSERT,
    TRANSACTION_CREATE_VPACK_INSERT_MARKER,

    TIMER_MAX
  };

  static void clear();
  static std::map<std::string, std::pair<double, uint64_t>> get();
  static std::string translateName(TimerType type) {
    switch (type) {
      case JS_INSERT_ALL:
        return "JS_INSERT_ALL";
      case JS_INSERT_V8_TO_VPACK:
        return "JS_INSERT_V8_TO_VPACK";
      case JS_INSERT_V8_TO_VPACK2:
        return "JS_INSERT_V8_TO_VPACK2";
      case JS_INSERT_VPACK_TO_V8:
        return "JS_INSERT_VPACK_TO_V8";
      case JS_INSERT_CREATE_TRX:
        return "JS_INSERT_CREATE_TRX";
      case JS_INSERT_INSERT:
        return "JS_INSERT_INSERT";
    
      case JS_DOCUMENT_ALL:
        return "JS_DOCUMENT_ALL";
      case JS_DOCUMENT_CREATE_TRX:
        return "JS_DOCUMENT_CREATE_TRX";
      case JS_DOCUMENT_DOCUMENT:
        return "JS_DOCUMENT_DOCUMENT";
      case JS_DOCUMENT_VPACK_TO_V8:
        return "JS_DOCUMENT_VPACK_TO_V8";
    
      case TRANSACTION_DOCUMENT_LOCAL:
        return "TRANSACTION_DOCUMENT_LOCAL";
      case TRANSACTION_DOCUMENT_DOCUMENT_DOCUMENT:
        return "TRANSACTION_DOCUMENT_DOCUMENT_DOCUMENT";
      case TRANSACTION_DOCUMENT_WORK_FOR_ONE:
        return "TRANSACTION_DOCUMENT_WORK_FOR_ONE";
      case TRANSACTION_DOCUMENT_EXTRACT:
        return "TRANSACTION_DOCUMENT_EXTRACT";

      case TRANSACTION_INSERT_LOCAL:
        return "TRANSACTION_INSERT_LOCAL";
      case TRANSACTION_INSERT_BUILD_DOCUMENT_IDENTITY:
        return "TRANSACTION_INSERT_BUILD_DOCUMENT_IDENTITY";
      case TRANSACTION_INSERT_WORK_FOR_ONE:
        return "TRANSACTION_INSERT_WORK_FOR_ONE";
      case TRANSACTION_INSERT_DOCUMENT_INSERT:
        return "TRANSACTION_INSERT_DOCUMENT_INSERT";
      case TRANSACTION_NEW_OBJECT_FOR_INSERT:
        return "TRANSACTION_NEW_OBJECT_FOR_INSERT";
      case TRANSACTION_CREATE_VPACK_INSERT_MARKER:
        return "TRANSACTION_CREATE_VPACK_INSERT_MARKER";

      case TIMER_MIN:
      case TIMER_MAX: {
        break;
      }
    }
    return "UNKNOWN";
  }

  static std::vector<double> starts;
  static std::vector<double> totals;
  static std::vector<uint64_t> counts;
};
}
}

#endif
