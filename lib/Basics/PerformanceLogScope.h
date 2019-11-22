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

#ifndef ARANGODB_BASICS_PERFORMANCE_LOG_SCOPE_H
#define ARANGODB_BASICS_PERFORMANCE_LOG_SCOPE_H 1

#include "Basics/Common.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

namespace arangodb {

class PerformanceLogScope {
 public:
  PerformanceLogScope(PerformanceLogScope const&) = delete;
  PerformanceLogScope& operator=(PerformanceLogScope const&) = delete;

  explicit PerformanceLogScope(std::string const& message, double minElapsedTime = 0.0)
      : _message(message), _start(TRI_microtime()), _minElapsedTime(minElapsedTime) {
    LOG_TOPIC("f2a96", TRACE, Logger::PERFORMANCE) << _message;
  }

  ~PerformanceLogScope() {
    double const elapsed = TRI_microtime() - _start;

    if (elapsed >= _minElapsedTime) {
      LOG_TOPIC("4ada1", TRACE, Logger::PERFORMANCE)
          << "[timer] " << Logger::FIXED(elapsed) << " s, " << _message;
    }
  }

 private:
  std::string const _message;
  double const _start;
  double const _minElapsedTime;
};

}  // namespace arangodb

#endif
