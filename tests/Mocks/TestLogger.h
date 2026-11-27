////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include <memory>

namespace arangodb {
class LogAppender;

namespace tests {

/// @brief starts the logger the way the test binary uses it: unthreaded, and
/// writing to stdout.
void initializeTestLogger();

/// @brief restores the global "always log security events" flag of the Logger
/// to its previous value on destruction.
class ScopedAlwaysLogSecurityEvents {
 public:
  ScopedAlwaysLogSecurityEvents();
  ~ScopedAlwaysLogSecurityEvents();

 private:
  bool const _old;
};

/// @brief for tests which change the global logger setup, i.e. its output
/// format or appenders, or which shut it down. restores the setup of the test
/// binary and the "always log security events" flag on destruction.
class ScopedTestLogger {
 public:
  ~ScopedTestLogger();

  /// @brief restarts the logger with the given output format and an optional
  /// additional global appender. the output format can only be changed while
  /// logging is inactive, and shutting down the logger removes all appenders,
  /// so the default ones are added again.
  void restart(bool useJson,
               std::shared_ptr<LogAppender> const& globalAppender = nullptr);

 private:
  ScopedAlwaysLogSecurityEvents _alwaysLogSecurityEvents;
};

}  // namespace tests
}  // namespace arangodb
