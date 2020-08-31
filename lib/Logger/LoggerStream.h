////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
class LoggerStreamBase {
 public:
  LoggerStreamBase(LoggerStreamBase const&) = delete;
  LoggerStreamBase& operator=(LoggerStreamBase const&) = delete;

  LoggerStreamBase();
  virtual ~LoggerStreamBase() = default;

 public:
  LoggerStreamBase& operator<<(LogLevel const& level) noexcept;

  LoggerStreamBase& operator<<(LogTopic const& topic) noexcept;

  LoggerStreamBase& operator<<(Logger::BINARY const& binary);

  LoggerStreamBase& operator<<(Logger::CHARS const& chars);

  LoggerStreamBase& operator<<(Logger::RANGE const& range);

  LoggerStreamBase& operator<<(Logger::FIXED const& duration);

  LoggerStreamBase& operator<<(Logger::LINE const& line) noexcept;

  LoggerStreamBase& operator<<(Logger::FILE const& file) noexcept;

  LoggerStreamBase& operator<<(Logger::FUNCTION const& function) noexcept;

  LoggerStreamBase& operator<<(Logger::LOGID const& logid) noexcept;

  template <typename T>
  LoggerStreamBase& operator<<(T const& obj) {
    try {
      _out << obj;
    } catch (...) {
      // ignore any errors here. logging should not have side effects
    }
    return *this;
  }

 protected:
  std::stringstream _out;
  size_t _topicId;
  LogLevel _level;
  int _line;
  char const* _logid;
  char const* _file;
  char const* _function;
};

class LoggerStream : public LoggerStreamBase {
 public:
  ~LoggerStream();
};

}  // namespace arangodb

#endif
