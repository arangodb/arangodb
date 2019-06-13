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
class LogAppenderStream : public LogAppender {
 public:
  LogAppenderStream(std::string const& filename, std::string const& filter, int fd);
  ~LogAppenderStream() {}

  void logMessage(LogLevel, std::string const& message, size_t offset) override final;

  virtual std::string details() override = 0;

  int fd() const { return _fd; }

 protected:
  void updateFd(int fd) { _fd = fd; }

  // determine the required length of the output buffer for the log message
  size_t determineOutputBufferSize(std::string const& message) const;

  // write the log message into the already allocated output buffer
  size_t writeIntoOutputBuffer(std::string const& message);

  virtual void writeLogMessage(LogLevel, char const*, size_t) = 0;

  /// @brief maximum size for reusable log buffer
  /// if the buffer exceeds this size, it will be freed after the log
  /// message was produced. otherwise it will be kept for recycling
  static constexpr size_t maxBufferSize = 64 * 1024;

 private:
  /// @brief a reusable buffer for log messages
  std::unique_ptr<char[]> _buffer;

  /// @brief allocation size of the buffer
  size_t _bufferSize;

 protected:
  /// @brief file descriptor
  int _fd;

  /// @brief whether or not we should use colors
  bool _useColors;

  /// @brief whether or not to escape special chars in log output
  bool const _escape;
};

class LogAppenderFile : public LogAppenderStream {
 public:
  LogAppenderFile(std::string const& filename, std::string const& filter);

  void writeLogMessage(LogLevel, char const*, size_t) override final;

  std::string details() override final;

 public:
  static void reopenAll();
  static void closeAll();
  static std::vector<std::tuple<int, std::string, LogAppenderFile*>> getFds() {
    return _fds;
  }
  static void setFds(std::vector<std::tuple<int, std::string, LogAppenderFile*>> const& fds) {
    _fds = fds;
  }
  static void clear();

  static void setFileMode(int mode) { _fileMode = mode; }
  static void setFileGroup(int group) { _fileGroup = group; }

 private:
  static std::vector<std::tuple<int, std::string, LogAppenderFile*>> _fds;
  static int _fileMode;
  static int _fileGroup;

  std::string _filename;
};

class LogAppenderStdStream : public LogAppenderStream {
 public:
  LogAppenderStdStream(std::string const& filename, std::string const& filter, int fd);
  ~LogAppenderStdStream();

  std::string details() override final { return std::string(); }

  static void writeLogMessage(int fd, bool useColors, LogLevel, char const* p,
                              size_t length, bool appendNewline);

 private:
  void writeLogMessage(LogLevel, char const*, size_t) override final;
};

class LogAppenderStderr final : public LogAppenderStdStream {
 public:
  explicit LogAppenderStderr(std::string const& filter);
};

class LogAppenderStdout final : public LogAppenderStdStream {
 public:
  explicit LogAppenderStdout(std::string const& filter);
};

}  // namespace arangodb

#endif
