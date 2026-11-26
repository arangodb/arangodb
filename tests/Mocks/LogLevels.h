////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Logger/LogLevel.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"

#include <memory>

namespace arangodb {
class LogAppender;

namespace tests {

/// @brief restarts the logger with the given output format, and optionally
/// with an additional global appender. output format settings can only be
/// changed while logging is inactive, and shutting down the logger removes all
/// appenders (also global ones), so the default ones have to be re-added. this
/// mirrors the logger setup in tests/main.cpp.
void restartLogger(bool useJson,
                   std::shared_ptr<LogAppender> const& globalAppender);

// sets specified topic to specified level in constructor, resets to previous
// value in destructor
template<arangodb::LogTopic& topic, arangodb::LogLevel level>
class LogSuppressor {
 public:
  LogSuppressor() : _oldLevel(topic.level()) {
    if (_oldLevel == LogLevel::DEFAULT || _oldLevel > level) {
      topic.setLogLevel(level);
    }
  }

  ~LogSuppressor() { topic.setLogLevel(_oldLevel); }

 private:
  LogLevel const _oldLevel;
};

// restores the global "always log security events" flag of the Logger to its
// previous value in destructor
class ScopedAlwaysLogSecurityEvents {
 public:
  ScopedAlwaysLogSecurityEvents() : _old(Logger::alwaysLogSecurityEvents()) {}
  ~ScopedAlwaysLogSecurityEvents() { Logger::setAlwaysLogSecurityEvents(_old); }

 private:
  bool const _old;
};

// suppresses the internal IResearch logging
class IResearchLogSuppressor {
 public:
  IResearchLogSuppressor();
};

}  // namespace tests
}  // namespace arangodb
