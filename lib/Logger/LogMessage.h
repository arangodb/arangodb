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

#pragma once

#include <string>

#include "Logger/LogLevel.h"

namespace arangodb {

struct LogMessage {
  LogMessage(LogMessage const&) = delete;
  LogMessage& operator=(LogMessage const&) = delete;

  LogMessage(std::string_view function, std::string_view file, int line,
             LogLevel level, size_t topicId, std::string&& message,
             uint32_t offset, bool shrunk) noexcept;

  /// @brief whether or no the message was already shrunk
  bool shrunk() const noexcept { return _shrunk; }

  /// @brief shrink log message to at most maxLength bytes (plus "..." appended)
  void shrink(std::size_t maxLength);

  /// @brief all details about the log message. we need to
  /// keep all this data around and not just the big log
  /// message string, because some LogAppenders will refer
  /// to individual components such as file, line etc.

  /// @brief function name of log message source code location
  std::string_view _function;
  /// @brief file of log message source code location
  std::string_view _file;
  /// @brief line of log message source code location
  int const _line;
  /// @brief log level
  LogLevel const _level;

  /// @brief id of log topic
  size_t const _topicId;
  /// @biref the actual log message
  std::string _message;
  /// @brief byte offset where actual message starts (i.e. excluding prologue)
  uint32_t _offset;
  /// @brief whether or not the log message was already shrunk (used to
  /// prevent duplicate shrinking of message)
  bool _shrunk;
};

}  // namespace arangodb
