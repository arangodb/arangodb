////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_LOGGER_LOG_APPENDER_FILE_H
#define ARANGODB_LOGGER_LOG_APPENDER_FILE_H 1

#include <stddef.h>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

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

  // determine the required length of the output buffer for the log message
  size_t determineOutputBufferSize(std::string const& message) const;

  // write the log message into the already allocated output buffer
  size_t writeIntoOutputBuffer(std::string const& message);

  virtual void writeLogMessage(LogLevel level, size_t topicId, char const* buffer, size_t len) = 0;

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
  explicit LogAppenderFile(std::string const& filename);
  ~LogAppenderFile();

  void writeLogMessage(LogLevel level, size_t topicId, char const* buffer, size_t len) override final;

  std::string details() const override final;

  std::string const& filename() const { return _filename; }

 public:
  static void reopenAll();
  static void closeAll();

#ifdef ARANGODB_USE_GOOGLE_TESTS
  static std::vector<std::tuple<int, std::string, LogAppenderFile*>> getAppenders();

  static void setAppenders(std::vector<std::tuple<int, std::string, LogAppenderFile*>> const& fds);
#endif

  static void setFileMode(int mode) { _fileMode = mode; }
  static void setFileGroup(int group) { _fileGroup = group; }

 private:
  std::string const _filename;

  static std::mutex _openAppendersMutex;
  static std::vector<LogAppenderFile*> _openAppenders;

  static int _fileMode;
  static int _fileGroup;
};

class LogAppenderStdStream : public LogAppenderStream {
 public:
  LogAppenderStdStream(std::string const& filename, int fd);
  ~LogAppenderStdStream();

  std::string details() const override final { return std::string(); }

  static void writeLogMessage(int fd, bool useColors, LogLevel level, size_t topicId, 
                              char const* p, size_t length, bool appendNewline);

 private:
  void writeLogMessage(LogLevel level, size_t topicId, char const* buffer, size_t len) override final;
};

class LogAppenderStderr final : public LogAppenderStdStream {
 public:
  LogAppenderStderr();
};

class LogAppenderStdout final : public LogAppenderStdStream {
 public:
  LogAppenderStdout();
};

}  // namespace arangodb

#endif
