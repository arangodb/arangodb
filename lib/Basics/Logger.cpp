////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Logger.h"

using namespace arangodb;

namespace {
std::atomic_int_fast16_t NEXT_TOPIC_ID(0);
}

std::atomic<LogLevel> Logger::_level(LogLevel::INFO);

LogTopic::LogTopic(std::string const& name)
    : LogTopic(name, LogLevel::DEFAULT) {}

LogTopic::LogTopic(std::string const& name, LogLevel level)
    : _topicId(NEXT_TOPIC_ID.fetch_add(1, std::memory_order_seq_cst)),
      _level(level) {
  _topics.set(_topicId);
}

LogTopic LogTopic::operator|(LogTopic const& that) {
  LogTopic result = *this;

  if (result._level < that._level) {
    result._topicId = MAX_LOG_TOPICS;
    result._topics |= that._topics;

    if (result._level < that._level) {
      result._level.store(that._level, std::memory_order_relaxed);
    }
  }

  return result;
}

LogTopic Logger::COLLECTOR("collector");
LogTopic Logger::COMPACTOR("compactor");
LogTopic Logger::PERFORMANCE("performance");
LogTopic Logger::REQUESTS("request");

void Logger::setLevel(LogLevel level) {
  _level = level;
}
