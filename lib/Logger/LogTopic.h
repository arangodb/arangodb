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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_LOGGER_LOG_TOPIC_H
#define ARANGODB_LOGGER_LOG_TOPIC_H 1

#include <stddef.h>
#include <atomic>
#include <string>
#include <utility>
#include <vector>

#include "Logger/LogLevel.h"

namespace arangodb {
class LogTopic {
  LogTopic& operator=(LogTopic const&) = delete;

 public:
  static size_t const MAX_LOG_TOPICS = 64;

 public:
  static std::vector<std::pair<std::string, LogLevel>> logLevelTopics();
  static void setLogLevel(std::string const&, LogLevel);
  static LogTopic* lookup(std::string const&);
  static std::string lookup(size_t topicId);

 public:
  explicit LogTopic(std::string const& name);
  virtual ~LogTopic() = default;

  LogTopic(std::string const& name, LogLevel level);

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

 public:
  size_t id() const { return _id; }
  std::string const& name() const { return _name; }
  std::string const& displayName() const { return _displayName; }
  LogLevel level() const { return _level.load(std::memory_order_relaxed); }

  virtual void setLogLevel(LogLevel level) {
    _level.store(level, std::memory_order_relaxed);
  }

 private:
  size_t const _id;
  std::string const _name;
  std::string _displayName;
  std::atomic<LogLevel> _level;
};
}  // namespace arangodb

#endif
