////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_TESTS_MOCKS_LOG_LEVEL_CHANGER_H
#define ARANGODB_TESTS_MOCKS_LOG_LEVEL_CHANGER_H 1

#include "Logger/LogLevel.h"
#include "Logger/LogTopic.h"

namespace arangodb {
namespace tests {

// sets specified topic to specified level in constructor, resets to previous
// value in destructor
template <arangodb::LogTopic& topic, arangodb::LogLevel level>
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

// suppresses the internal IResearch logging
class IResearchLogSuppressor {
 public:
  IResearchLogSuppressor();
};

}  // namespace tests
}  // namespace arangodb

#endif
