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

#include <cstddef>
#include <sstream>
#include <string>

#include "Logger/LogLevel.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"

namespace arangodb {
/// @brief base class for regular logging and audit logging streams.
/// do _not_ add virtual methods here, as this will be bad for efficiency.
class LoggerStreamBase {
 public:
  LoggerStreamBase(LoggerStreamBase const&) = delete;
  LoggerStreamBase& operator=(LoggerStreamBase const&) = delete;

  LoggerStreamBase();
  explicit LoggerStreamBase(bool enabled);

  // intentionally not virtual, as such objects can be created _very_ often!
  ~LoggerStreamBase() = default;

 public:
  LoggerStreamBase& operator<<(LogLevel const& level) noexcept;

  LoggerStreamBase& operator<<(LogTopic const& topic) noexcept;

  LoggerStreamBase& operator<<(Logger::FIXED const& duration) noexcept;

  LoggerStreamBase& operator<<(Logger::LINE const& line) noexcept;

  LoggerStreamBase& operator<<(Logger::FILE const& file) noexcept;

  LoggerStreamBase& operator<<(Logger::FUNCTION const& function) noexcept;

  LoggerStreamBase& operator<<(Logger::LOGID const& logid) noexcept;

  template<typename T>
  LoggerStreamBase& operator<<(T const& obj) noexcept {
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
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool const _enabled;
#endif
  std::string_view _logid;
  std::string_view _file;
  std::string_view _function;
};

class LoggerStream : public LoggerStreamBase {
 public:
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  explicit LoggerStream(bool enabled);
#endif
  LoggerStream();
  ~LoggerStream();
};

}  // namespace arangodb
