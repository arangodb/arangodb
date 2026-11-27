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
////////////////////////////////////////////////////////////////////////////////

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

#include "LoggerStream.h"

#include "Basics/debugging.h"
#include "Logger/Logger.h"

namespace arangodb {

LoggerStreamBase::LoggerStreamBase(bool enabled,
                                   std::optional<LogSecurity> security)
    : _topicId(LogTopic::GLOBAL_LOG_TOPIC),
      _level(LogLevel::DEFAULT),
      _security(security),
      _line(0),
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _enabled(enabled),
#endif
      _logid(),
      _file(),
      _function() {
  // a security event must belong to at least one known category. an empty
  // mask (e.g. `LogSecurity{}`) or one with only unknown bits would produce an
  // empty classifier label. use std::nullopt instead to mark a message as "not
  // a security event".
  TRI_ASSERT(!security.has_value() || isValidSecurity(*security))
      << "invalid security category mask "
      << static_cast<std::uint16_t>(security.value_or(LogSecurity{}));
}

LoggerStreamBase::LoggerStreamBase() : LoggerStreamBase(true, std::nullopt) {}

LoggerStreamBase& LoggerStreamBase::operator<<(LogLevel const& level) noexcept {
  _level = level;
  return *this;
}

LoggerStreamBase& LoggerStreamBase::operator<<(LogTopic const& topic) noexcept {
  _topicId = topic.id();
  return *this;
}

LoggerStreamBase& LoggerStreamBase::operator<<(
    Logger::FIXED const& value) noexcept {
  try {
    std::ostringstream tmp;
    tmp << std::setprecision(value._precision) << std::fixed << value._value;
    _out << tmp.view();
  } catch (...) {
    // ignore any errors here. logging should not have side effects
  }

  return *this;
}

LoggerStreamBase& LoggerStreamBase::operator<<(
    Logger::LINE const& line) noexcept {
  _line = line._line;
  return *this;
}

LoggerStreamBase& LoggerStreamBase::operator<<(
    Logger::FILE const& file) noexcept {
  _file = file._file;
  return *this;
}

LoggerStreamBase& LoggerStreamBase::operator<<(
    Logger::FUNCTION const& function) noexcept {
  _function = function._function;
  return *this;
}

LoggerStreamBase& LoggerStreamBase::operator<<(
    Logger::LOGID const& logid) noexcept {
  _logid = logid._logid;
  return *this;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
LoggerStream::LoggerStream(bool enabled)
    : LoggerStreamBase(enabled, std::nullopt) {}

LoggerStream::LoggerStream(bool enabled, std::optional<LogSecurity> security)
    : LoggerStreamBase(enabled, security) {}
#endif

LoggerStream::LoggerStream() : LoggerStreamBase(true, std::nullopt) {}

LoggerStream::LoggerStream(std::optional<LogSecurity> security)
    : LoggerStreamBase(true, security) {}

LoggerStream::~LoggerStream() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // this instance variable can only be false in maintainer mode.
  // so save the check here if it is known to be always true.
  if (!_enabled) {
    return;
  }
#endif

  try {
    // get a view on the stream's underlying buffer, without copying it
    Logger::log(_logid, _function, _file, _line, _level, _topicId, _out.view(),
                _security);
  } catch (...) {
    try {
      // logging the error may fail as well, and we should never throw in the
      // dtor
      std::cerr << "failed to log: " << _out.view() << std::endl;
    } catch (...) {
    }
  }
}

}  // namespace arangodb
