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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_UTILITIES_TIMER_H
#define ARANGODB_UTILITIES_TIMER_H 1

#include <Logger/Logger.h>
#include <chrono>
#include <string>

namespace arangodb {
namespace Utilities {

// This class is intended for debugging purpose only and is not meant
// to be included into any released code.
struct timer {
  using ns = ::std::chrono::nanoseconds;
  using clock = ::std::chrono::high_resolution_clock;
  using point = ::std::chrono::time_point<clock, ns>;
  std::string _name;
  point _start;
  bool _released;

  timer(std::string const& name = "unnamed")
      : _name(" - " + name), _start(clock::now()), _released(false) {}

  ~timer() {
    if (!_released) {
      release();
    }
  }

  void release() {
    LOG_TOPIC("f94c5", ERR, Logger::FIXME)
        << "## ## ## timer" << _name << ":" << std::fixed
        << diff(_start, clock::now()) / (double)std::giga::num << "s";
    _released = true;
  }

  std::uint64_t diff(point const& start, point const& end) {
    return (end - start).count();
  }
};

}  // namespace Utilities
}  // namespace arangodb
#endif
