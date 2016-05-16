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

#ifndef ARANGODB_LOGGER_LOG_APPENDER_FILE_H
#define ARANGODB_LOGGER_LOG_APPENDER_FILE_H 1

#include "Logger/LogAppender.h"

namespace arangodb {
class LogAppenderFile : public LogAppender {
 public:
  static void reopen();
  static void close();

 public:
  LogAppenderFile(std::string const& filename, std::string const& filter);
  ~LogAppenderFile();

  bool logMessage(LogLevel, std::string const& message,
                  size_t offset) override final;

  std::string details() override final;

 private:
  void writeLogFile(int, char const*, ssize_t);

 private:
  static std::vector<std::pair<int, std::string>> _fds;

 private:
  ssize_t _pos;

  /// @brief a reusable buffer for log messages
  char* _buffer;
  /// @brief allocation size of the buffer
  size_t _bufferSize;
};
}

#endif
