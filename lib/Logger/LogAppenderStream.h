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

#include "Logger/LogAppender.h"
#include "Logger/LogLevel.h"

namespace arangodb {
struct LogMessage;

class LogAppenderStream : public LogAppender {
 public:
  LogAppenderStream(std::string const& filename, int fd);
  ~LogAppenderStream() = default;

  void logMessage(LogMessage const& message) override final;

  virtual std::string details() const override = 0;

  int fd() const { return _fd; }

 protected:
  void updateFd(int fd) { _fd = fd; }

  virtual void writeLogMessage(LogLevel level, size_t topicId,
                               std::string const& message) = 0;

  /// @brief maximum size for reusable log buffer
  /// if the buffer exceeds this size, it will be freed after the log
  /// message was produced. otherwise it will be kept for recycling
  static constexpr size_t maxBufferSize = 64 * 1024;

  /// @brief file descriptor
  int _fd;

  /// @brief whether or not we should use colors
  bool _useColors;
};

}  // namespace arangodb
