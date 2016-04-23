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

#ifdef USE_TIMERS

#define TIMER_START(name) arangodb::basics::Timers::starts[arangodb::basics::Timers::name] = TRI_microtime()
#define TIMER_STOP(name) arangodb::basics::Timers::totals[arangodb::basics::Timers::name] += TRI_microtime() - arangodb::basics::Timers::starts[arangodb::basics::Timers::name]; ++arangodb::basics::Timers::counts[arangodb::basics::Timers::name]

#else

#define TIMER_START(name) do { } while (false)
#define TIMER_STOP(name) do { } while (false)

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

    TIMER_MAX
  };

  static void clear();
  static std::map<std::string, std::pair<double, uint64_t>> get();
  static std::string translateName(TimerType);
  
  static std::vector<double> starts;
  static std::vector<double> totals;
  static std::vector<uint64_t> counts;
};
}
}

#endif
