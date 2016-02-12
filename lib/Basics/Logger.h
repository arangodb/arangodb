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
/// @author Achim Brandt
/// @author Dr. Frank Celler
///
/// Portions of the code are:
///
/// Copyright (c) 1999, Google Inc.
/// All rights reserved.
//
/// Redistribution and use in source and binary forms, with or without
/// modification, are permitted provided that the following conditions are
/// met:
//
///     * Redistributions of source code must retain the above copyright
///       notice, this list of conditions and the following disclaimer.
///     * Redistributions in binary form must reproduce the above
///       copyright notice, this list of conditions and the following
///       disclaimer
///       in the documentation and/or other materials provided with the
///       distribution.
///     * Neither the name of Google Inc. nor the names of its
///       contributors may be used to endorse or promote products derived
///       from this software without specific prior written permission.
///
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
/// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
/// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
/// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
/// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
/// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
/// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
/// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
/// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
/// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
/// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///
/// Author: Ray Sidney
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_BASIC_LOGGER_H
#define LIB_BASIC_LOGGER_H 1

#include "Basics/Common.h"

#include <iosfwd>
#include <sstream>

////////////////////////////////////////////////////////////////////////////////
/// @brief maximal number of log topics
////////////////////////////////////////////////////////////////////////////////

#define MAX_LOG_TOPICS 64

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message
////////////////////////////////////////////////////////////////////////////////

#define LOG(a)                                                        \
  !arangodb::Logger::isEnabled((arangodb::LogLevel::a))               \
      ? (void)0                                                       \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()            \
                                  << (arangodb::LogLevel::a)          \
                                  << arangodb::Logger::LINE(__LINE__) \
                                  << arangodb::Logger::FILE(__FILE__) \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message for a topic
////////////////////////////////////////////////////////////////////////////////

#define LOG_TOPIC(a, b)                                               \
  !arangodb::Logger::isEnabled((arangodb::LogLevel::a), (b))          \
      ? (void)0                                                       \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()            \
                                  << (arangodb::LogLevel::a) << (b)   \
                                  << arangodb::Logger::LINE(__LINE__) \
                                  << arangodb::Logger::FILE(__FILE__) \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message given that a condition is true
////////////////////////////////////////////////////////////////////////////////

#define LOG_IF(a, cond)                                               \
  !(arangodb::Logger::isEnabled((arangodb::LogLevel::a)) && (cond))   \
      ? (void)0                                                       \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()            \
                                  << (arangodb::LogLevel::a)          \
                                  << arangodb::Logger::LINE(__LINE__) \
                                  << arangodb::Logger::FILE(__FILE__) \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message for a topic given that a condition is true
////////////////////////////////////////////////////////////////////////////////

#define LOG_TOPIC_IF(a, b, cond)                                         \
  !(arangodb::Logger::isEnabled((arangodb::LogLevel::a), (b)) && (cond)) \
      ? (void)0                                                          \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()               \
                                  << (arangodb::LogLevel::a) << (b)      \
                                  << arangodb::Logger::LINE(__LINE__)    \
                                  << arangodb::Logger::FILE(__FILE__)    \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message every N.the time
////////////////////////////////////////////////////////////////////////////////

#define LOG_EVERY_N_VARNAME(base, line) LOG_EVERY_N_VARNAME_CONCAT(base, line)
#define LOG_EVERY_N_VARNAME_CONCAT(base, line) base##line

#define LOG_OCCURRENCES LOG_EVERY_N_VARNAME(occurrences_, __LINE__)
#define LOG_OCCURRENCES_MOD_N LOG_EVERY_N_VARNAME(occurrences_mod_n_, __LINE__)

#define LOG_EVERY_N(a, n)                                             \
  static int LOG_OCCURRENCES = 0, LOG_OCCURRENCES_MOD_N = 0;          \
  ++LOG_OCCURRENCES;                                                  \
  if (++LOG_OCCURRENCES_MOD_N > n) LOG_OCCURRENCES_MOD_N -= n;        \
  if (LOG_OCCURRENCES_MOD_N == 1)                                     \
  !(arangodb::Logger::isEnabled((arangodb::LogLevel::a)) &&           \
    (LOG_OCCURRENCES_MOD_N == 1))                                     \
      ? (void)0                                                       \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()            \
                                  << (arangodb::LogLevel::a)          \
                                  << arangodb::Logger::LINE(__LINE__) \
                                  << arangodb::Logger::FILE(__FILE__) \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message for a topic every N.the time
////////////////////////////////////////////////////////////////////////////////

#define LOG_TOPIC_EVERY_N(a, b, n)                                    \
  static int LOG_OCCURRENCES = 0, LOG_OCCURRENCES_MOD_N = 0;          \
  ++LOG_OCCURRENCES;                                                  \
  if (++LOG_OCCURRENCES_MOD_N > n) LOG_OCCURRENCES_MOD_N -= n;        \
  if (LOG_OCCURRENCES_MOD_N == 1)                                     \
  !(arangodb::Logger::isEnabled((arangodb::LogLevel::a), (b)) &&      \
    (LOG_OCCURRENCES_MOD_N == 1))                                     \
      ? (void)0                                                       \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()            \
                                  << (arangodb::LogLevel::a) << (b)   \
                                  << arangodb::Logger::LINE(__LINE__) \
                                  << arangodb::Logger::FILE(__FILE__) \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief LogLevel
////////////////////////////////////////////////////////////////////////////////
#ifdef TRI_UNDEF_ERR
#undef ERR
#endif
enum class LogLevel {
  DEFAULT = 0,
  FATAL = 1,
  ERR = 2,
  WARN = 3,
  INFO = 4,
  DEBUG = 5,
  TRACE = 6
};

////////////////////////////////////////////////////////////////////////////////
/// @brief LogBuffer
///
/// This class is used to store a number of log messages in the server
/// for retrieval. This messages are truncated and overwritten without
/// warning.
////////////////////////////////////////////////////////////////////////////////

struct LogBuffer {
  uint64_t _id;
  LogLevel _level;
  time_t _timestamp;
  char _message[256];
};

////////////////////////////////////////////////////////////////////////////////
/// @brief LogTopic
////////////////////////////////////////////////////////////////////////////////

class LogTopic {
  LogTopic& operator=(LogTopic const&) = delete;

 public:
  explicit LogTopic(std::string const& name);

  LogTopic(std::string const& name, LogLevel level);

  LogTopic(LogTopic const& that) noexcept : _id(that._id), _name(that._name) {
    _level.store(that._level, std::memory_order_relaxed);
  }

  LogTopic(LogTopic&& that) noexcept : _id(that._id), _name(that._name) {
    _level.store(that._level, std::memory_order_relaxed);
  }

 public:
  size_t id() const { return _id; }
  std::string const& name() const { return _name; }
  LogLevel level() const { return _level.load(std::memory_order_relaxed); }

  void setLogLevel(LogLevel level) {
    _level.store(level, std::memory_order_relaxed);
  }

 private:
  size_t _id;
  std::string _name;
  std::atomic<LogLevel> _level;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Logger
///
/// This class provides various static members which can be used as logging
/// streams. Output to the logging stream is appended by using the operator <<,
/// as soon as a line is completed, an implicit endl will be used to flush the
/// stream. Each line of output is prefixed by some informational data.
///
///
/// options:
///    log.level info
///    log.level compactor=debug
///    log.level replication=trace
///
///    log.output compactor=file:/a/b/c
///    log.output replication=syslog:xxxx
///    log.output performance=+
///    log.output file:/c/d/ef
///
/// deprecated:
///
///     log.file x          => log.output file:x
///     log.requests-file y => log.output requests=file:y
///     log.performance     => log.level performance=info
////////////////////////////////////////////////////////////////////////////////

class Logger {
  Logger(Logger const&) = delete;
  Logger& operator=(Logger const&) = delete;

  friend class LoggerStream;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief predefined topics
  //////////////////////////////////////////////////////////////////////////////

  static LogTopic COLLECTOR;
  static LogTopic COMPACTOR;
  static LogTopic MMAP;
  static LogTopic PERFORMANCE;
  static LogTopic QUERIES;
  static LogTopic REPLICATION;
  static LogTopic REQUESTS;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief duration
  //////////////////////////////////////////////////////////////////////////////

  struct DURATION {
    explicit DURATION(double duration, int precision = 6)
        : _duration(duration), _precision(precision){};
    double _duration;
    int _precision;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief range
  //////////////////////////////////////////////////////////////////////////////

  struct RANGE {
    RANGE(void const* baseAddress, size_t size)
        : baseAddress(baseAddress), size(size){};
    void const* baseAddress;
    size_t size;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief line number helper class
  //////////////////////////////////////////////////////////////////////////////

  struct LINE {
    explicit LINE(long int line) : _line(line){};
    long int _line;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief filename helper class
  //////////////////////////////////////////////////////////////////////////////

  struct FILE {
    explicit FILE(char const* file) : _file(file){};
    char const* _file;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief function helper class
  //////////////////////////////////////////////////////////////////////////////

  struct FUNCTION {
    explicit FUNCTION(char const* function) : _function(function){};
    char const* _function;
  };

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new appender
  //////////////////////////////////////////////////////////////////////////////

  static void addAppender(std::string const&, bool, std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief determines the global log level
  //////////////////////////////////////////////////////////////////////////////

  static LogLevel logLevel();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief determines the log levels of the topics
  //////////////////////////////////////////////////////////////////////////////

  static std::vector<std::pair<std::string, LogLevel>> logLevelTopics();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the global log level
  //////////////////////////////////////////////////////////////////////////////

  static void setLogLevel(LogLevel);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the log level from a string
  ///
  /// set the global level: info
  /// set a topic level:    performance=info
  //////////////////////////////////////////////////////////////////////////////

  static void setLogLevel(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the log level from a vector of string
  //////////////////////////////////////////////////////////////////////////////

  static void setLogLevel(std::vector<std::string> const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the output prefix
  //////////////////////////////////////////////////////////////////////////////

  static void setOutputPrefix(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the line number mode
  //////////////////////////////////////////////////////////////////////////////

  static void setShowLineNumber(bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the thread identifier mode
  //////////////////////////////////////////////////////////////////////////////

  static void setShowThreadIdentifier(bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the local time mode
  //////////////////////////////////////////////////////////////////////////////

  static void setUseLocalTime(bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a string description for the log level
  //////////////////////////////////////////////////////////////////////////////

  static std::string const& translateLogLevel(LogLevel);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks if logging is enabled for log level
  //////////////////////////////////////////////////////////////////////////////

  static bool isEnabled(LogLevel level) {
    return (int)level <= (int)_level.load(std::memory_order_relaxed);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks if logging is enabled for log topic
  //////////////////////////////////////////////////////////////////////////////

  static bool isEnabled(LogLevel level, LogTopic const& topic) {
    return (int)level <= (int)((topic.level() == LogLevel::DEFAULT)
                                   ? _level.load(std::memory_order_relaxed)
                                   : topic.level());
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the last log entries
  //////////////////////////////////////////////////////////////////////////////

  static std::vector<LogBuffer> bufferedEntries(LogLevel level, uint64_t start,
                                                bool upToLevel);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initializes the logging component
  //////////////////////////////////////////////////////////////////////////////

  static void initialize(bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief shuts down the logging components
  //////////////////////////////////////////////////////////////////////////////

  static void shutdown(bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief reopens all log appenders
  //////////////////////////////////////////////////////////////////////////////

  static void reopen();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief tries to flush the logging
  //////////////////////////////////////////////////////////////////////////////

  static void flush();

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief current log level
  //////////////////////////////////////////////////////////////////////////////

  static std::atomic<LogLevel> _level;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief LoggerStream
///
/// Helper acting as a output stream.
////////////////////////////////////////////////////////////////////////////////

class LoggerStream {
  LoggerStream(LoggerStream const&) = delete;
  LoggerStream& operator=(LoggerStream const&) = delete;

 public:
  LoggerStream()
      : _topicId(MAX_LOG_TOPICS),
        _level(LogLevel::DEFAULT),
        _line(0),
        _file(nullptr),
        _function(nullptr) {}

  ~LoggerStream();

 public:
  LoggerStream& operator<<(LogLevel level) {
    _level = level;
    return *this;
  }

  LoggerStream& operator<<(LogTopic topic) {
    _topicId = topic.id();
    _out << "{" + topic.name() << "} ";
    return *this;
  }

  LoggerStream& operator<<(Logger::RANGE range);

  LoggerStream& operator<<(Logger::DURATION duration);

  LoggerStream& operator<<(Logger::LINE line) {
    _line = line._line;
    return *this;
  }

  LoggerStream& operator<<(Logger::FILE file) {
    _file = file._file;
    return *this;
  }

  LoggerStream& operator<<(Logger::FUNCTION function) {
    _function = function._function;
    return *this;
  }

  template <typename T>
  LoggerStream& operator<<(T obj) {
    _out << obj;
    return *this;
  }

 private:
  std::stringstream _out;
  size_t _topicId;
  LogLevel _level;
  long int _line;
  char const* _file;
  char const* _function;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief helper class for macros
////////////////////////////////////////////////////////////////////////////////

class LogVoidify {
 public:
  LogVoidify() {}
  void operator&(LoggerStream const&) {}
};
}

std::ostream& operator<<(std::ostream&, arangodb::LogLevel);

#endif
