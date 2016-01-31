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
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_BASIC_LOGGER_H
#define LIB_BASIC_LOGGER_H 1

#include "Basics/logging.h"

#include <bitset>
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
/// @brief logs a message for a subject
////////////////////////////////////////////////////////////////////////////////

#define LOG_TOPIC(a, b)                                               \
  !arangodb::Logger::isEnabled((arangodb::LogLevel::a), (b))          \
      ? (void)0                                                       \
      : arangodb::LogVoidify() & (arangodb::LoggerStream()            \
                                  << (arangodb::LogLevel::a) << (b)   \
                                  << arangodb::Logger::LINE(__LINE__) \
                                  << arangodb::Logger::FILE(__FILE__) \
                                  << arangodb::Logger::FUNCTION(__FUNCTION__))

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief available log levels
////////////////////////////////////////////////////////////////////////////////

enum class LogLevel {
  DEFAULT = 0,
  FATAL = 1,
  ERROR = 2,
  WARNING = 3,
  INFO = 4,
  DEBUG = 5,
  TRACE = 6
};

////////////////////////////////////////////////////////////////////////////////
/// @brief LogTopic
///
/// Note that combining topics is possible, but expensive and should be
/// avoided in DEBUG or TRACE.
////////////////////////////////////////////////////////////////////////////////

class LogTopic {
  LogTopic& operator=(LogTopic const&) = delete;

 public:
  LogTopic(std::string const& name);

  LogTopic(std::string const& name, LogLevel level);

  LogTopic(LogTopic const& that) noexcept : _topicId(that._topicId),
                                            _topics(that._topics) {
    _level.store(that._level, std::memory_order_relaxed);
  }

  LogTopic(LogTopic&& that) noexcept : _topicId(that._topicId),
                                       _topics(std::move(that._topics)) {
    _level.store(that._level, std::memory_order_relaxed);
  }

 public:
  LogLevel level() const { return _level.load(std::memory_order_relaxed); }
  void setLevel(LogLevel level) { _level.store(level, std::memory_order_relaxed); }
  std::bitset<MAX_LOG_TOPICS> const& bits() const { return _topics; }

  LogTopic operator|(LogTopic const&);

 private:
  size_t _topicId;
  std::bitset<MAX_LOG_TOPICS> _topics;
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
///
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
  static LogTopic PERFORMANCE;
  static LogTopic QUERIES;
  static LogTopic REQUESTS;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief duration
  //////////////////////////////////////////////////////////////////////////////

  struct DURATION {
    explicit DURATION(double duration, int precision = 6) : _duration(duration), _precision(precision){};
    double _duration;
    int _precision;
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
  /// @brief sets the global log level
  //////////////////////////////////////////////////////////////////////////////

  static void setLevel(LogLevel);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief determines the global log level
  //////////////////////////////////////////////////////////////////////////////

  static LogLevel logLevel();
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a string description for the log level
  //////////////////////////////////////////////////////////////////////////////

  static char const* translateLogLevel(LogLevel);

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
    return (int)level <=
     (int)((topic.level() == LogLevel::DEFAULT) ? _level.load(std::memory_order_relaxed)
     : topic.level());
  }

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
      : _level(LogLevel::DEFAULT),
        _line(0),
        _file(nullptr),
        _function(nullptr) {}

  ~LoggerStream() {
    TRI_Log(_function, _file, _line, (TRI_log_level_e)(int)_level,
            TRI_LOG_SEVERITY_HUMAN, "%s", _out.str().c_str());
  }

 public:
  LoggerStream& operator<<(LogLevel level) {
    _level = level;
    return *this;
  }

  LoggerStream& operator<<(LogTopic topics) {
    _topics = topics.bits();
    return *this;
  }

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
  std::bitset<MAX_LOG_TOPICS> _topics;
  LogLevel _level;
  long int _line;
  char const* _file;
  char const* _function;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief LoggerStream
////////////////////////////////////////////////////////////////////////////////

class LogVoidify {
 public:
  LogVoidify() {}
  void operator&(LoggerStream const&) {}
};
}

std::ostream& operator<<(std::ostream&, arangodb::LogLevel);

#endif
