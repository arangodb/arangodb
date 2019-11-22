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

#ifndef ARANGODB_LOGGER_LOGGER_STREAM_H
#define ARANGODB_LOGGER_LOGGER_STREAM_H 1

#include <cstddef>
#include <sstream>
#include <string>

#include "Logger/LogLevel.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"

namespace arangodb {
class LoggerStream {
 public:
  LoggerStream(LoggerStream const&) = delete;
  LoggerStream& operator=(LoggerStream const&) = delete;

  LoggerStream()
      : _topicId(LogTopic::MAX_LOG_TOPICS),
        _level(LogLevel::DEFAULT),
        _line(0),
        _file(nullptr),
        _function(nullptr) {}

  ~LoggerStream();

 public:
  LoggerStream& operator<<(LogLevel const& level) {
    _level = level;
    return *this;
  }

  LoggerStream& operator<<(LogTopic const& topic) {
    _topicId = topic.id();
    _out << topic.displayName();
    return *this;
  }

  LoggerStream& operator<<(Logger::BINARY const& binary);

  LoggerStream& operator<<(Logger::CHARS const& chars);

  LoggerStream& operator<<(Logger::RANGE const& range);

  LoggerStream& operator<<(Logger::FIXED const& duration);

  LoggerStream& operator<<(Logger::LINE const& line) {
    _line = line._line;
    return *this;
  }

  LoggerStream& operator<<(Logger::FILE const& file) {
    _file = file._file;
    return *this;
  }

  LoggerStream& operator<<(Logger::FUNCTION const& function) {
    _function = function._function;
    return *this;
  }

  template <typename T>
  LoggerStream& operator<<(T const& obj) {
    try {
      _out << obj;
    } catch (...) {
      // ignore any errors here. logging should not have side effects
    }
    return *this;
  }

 private:
  std::stringstream _out;
  size_t _topicId;
  LogLevel _level;
  int _line;
  char const* _file;
  char const* _function;
};
}  // namespace arangodb

#endif
