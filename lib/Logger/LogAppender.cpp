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
/// @author Achim Brandt
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "LogAppender.h"
#include <atomic>
#include <cstddef>

#include "Basics/RecursiveLocker.h"
#include "Logger/LogLevel.h"
#include "Logger/LogMessage.h"
#include "Logger/LogTopic.h"
#include "Logger/Topics.h"

namespace arangodb {

LogAppender::LogAppender() {
  for (std::size_t i = 0; i < logger::kNumTopics; ++i) {
    auto* topic = LogTopic::topicForId(i);
    if (topic != nullptr) {
      _defaultLevels[i] = topic->level();
    } else {
      _defaultLevels[i] = LogLevel::DEFAULT;
    }
  }
  resetLevelsToDefault();
}

void LogAppender::logMessageGuarded(LogMessage const& message) {
  auto level = _topicLevels[message._topicId].load(std::memory_order_relaxed);
  if (level == LogLevel::DEFAULT) {
    level = LogLevel::INFO;
  }

  if (message._level <= level) {
    // Only one thread is allowed to actually write logs to the file.
    // We use a recusive lock here, just in case writing the log message
    // causes a crash, in this case we may trigger another force-direct
    // log. This is not very likely, but it is better to be safe than sorry.
    RECURSIVE_WRITE_LOCKER(_logOutputMutex, _logOutputMutexOwner);
    logMessage(message);
  }
}

void LogAppender::setCurrentLevelsAsDefault() {
  for (std::size_t i = 0; i < logger::kNumTopics; ++i) {
    _defaultLevels[i] = _topicLevels[i].load(std::memory_order_relaxed);
  }
}

void LogAppender::resetLevelsToDefault() {
  for (std::size_t i = 0; i < logger::kNumTopics; ++i) {
    _topicLevels[i].store(_defaultLevels[i], std::memory_order_relaxed);
  }
}

auto LogAppender::getLogLevel(LogTopic const& topic) -> LogLevel {
  return _topicLevels[topic.id()].load(std::memory_order_relaxed);
}

void LogAppender::setLogLevel(LogTopic const& topic, LogLevel level) {
  _topicLevels[topic.id()].store(level, std::memory_order_relaxed);
}

auto LogAppender::getLogLevels() -> std::unordered_map<LogTopic*, LogLevel> {
  std::unordered_map<LogTopic*, LogLevel> result;
  result.reserve(logger::kNumTopics);
  for (std::size_t i = 0; i < logger::kNumTopics; ++i) {
    auto* topic = LogTopic::topicForId(i);
    if (topic != nullptr) {
      result.emplace(topic, _topicLevels[i].load(std::memory_order_relaxed));
    }
  }
  return result;
}

}  // namespace arangodb
