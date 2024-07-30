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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stddef.h>
#include <atomic>
#include <string>
#include <utility>
#include <vector>

#include "Logger/LogLevel.h"

namespace arangodb {

using TopicName = std::string_view;

class LogTopic {
  LogTopic& operator=(LogTopic const&) = delete;

 public:
  static constexpr size_t GLOBAL_LOG_TOPIC = 64;

  // pseudo topic to address all log topics
  static constexpr TopicName ALL = "all";

  static std::vector<std::pair<TopicName, LogLevel>> logLevelTopics();
  static void setLogLevel(TopicName, LogLevel);
  static LogTopic* lookup(TopicName);
  static TopicName lookup(size_t topicId);

  virtual ~LogTopic() = default;

  template<typename Topic>
  LogTopic(Topic);

  LogTopic(LogTopic const& that)
      : _id(that._id), _name(that._name), _displayName(that._displayName) {
    _level.store(that._level, std::memory_order_relaxed);
  }

  LogTopic(LogTopic&& that) noexcept
      : _id(that._id),
        _name(std::move(that._name)),
        _displayName(std::move(that._displayName)) {
    _level.store(that._level, std::memory_order_relaxed);
  }

  size_t id() const { return _id; }
  TopicName name() const { return _name; }
  std::string const& displayName() const { return _displayName; }
  LogLevel level() const { return _level.load(std::memory_order_relaxed); }

  virtual void setLogLevel(LogLevel level) {
    _level.store(level, std::memory_order_relaxed);
  }

 private:
  LogTopic(TopicName name, LogLevel level, size_t id);

  size_t const _id;
  TopicName _name;
  std::string _displayName;
  std::atomic<LogLevel> _level;
};
}  // namespace arangodb
