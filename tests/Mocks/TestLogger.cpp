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

#include "TestLogger.h"

#include "Logger/LogAppender.h"
#include "Logger/Logger.h"

namespace arangodb::tests {

void initializeTestLogger() {
  Logger::initialize(false, 10000);
  Logger::addAppender(Logger::defaultLogGroup(), "-");
}

ScopedAlwaysLogSecurityEvents::ScopedAlwaysLogSecurityEvents()
    : _old(Logger::alwaysLogSecurityEvents()) {}

ScopedAlwaysLogSecurityEvents::~ScopedAlwaysLogSecurityEvents() {
  Logger::setAlwaysLogSecurityEvents(_old);
}

ScopedTestLogger::~ScopedTestLogger() { restart(/*useJson*/ false); }

void ScopedTestLogger::restart(
    bool useJson, std::shared_ptr<LogAppender> const& globalAppender) {
  Logger::shutdown();
  Logger::setUseJson(useJson);
  initializeTestLogger();
  if (globalAppender != nullptr) {
    Logger::addGlobalAppender(Logger::defaultLogGroup(), globalAppender);
  }
}

}  // namespace arangodb::tests
