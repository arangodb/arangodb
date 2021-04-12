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
/// @author Achim Brandt
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////
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

#ifndef ARANGODB_LOGGER_LOGGER_H
#define ARANGODB_LOGGER_LOGGER_H 1

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/threads.h"
#include "Logger/LogLevel.h"
#include "Logger/LogTimeFormat.h"
#include "Logger/LogTopic.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
class LogGroup;
class LogThread;

////////////////////////////////////////////////////////////////////////////////
/// @brief message container
////////////////////////////////////////////////////////////////////////////////

struct LogMessage {
  LogMessage(LogMessage const&) = delete;
  LogMessage& operator=(LogMessage const&) = delete;

  LogMessage(char const* function, char const* file, int line,
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
  char const* _function;
  /// @brief file of log message source code location
  char const* _file;
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
  friend class LogThread;
  friend class LogAppenderStream;
  friend class LogAppenderFile;

 public:
  static LogTopic AGENCY;
  static LogTopic AGENCYCOMM;
  static LogTopic AGENCYSTORE;
  static LogTopic AQL;
  static LogTopic AUTHENTICATION;
  static LogTopic AUTHORIZATION;
  static LogTopic BACKUP;
  static LogTopic CACHE;
  static LogTopic CLUSTER;
  static LogTopic CLUSTERCOMM;
  static LogTopic COLLECTOR;
  static LogTopic COMMUNICATION;
  static LogTopic CONFIG;
  static LogTopic CRASH;
  static LogTopic DEVEL;
  static LogTopic DUMP;
  static LogTopic ENGINES;
  static LogTopic FIXME;
  static LogTopic FLUSH;
  static LogTopic GRAPHS;
  static LogTopic HEARTBEAT;
  static LogTopic HTTPCLIENT;
  static LogTopic MAINTENANCE;
  static LogTopic MEMORY;
  static LogTopic MMAP;
  static LogTopic PERFORMANCE;
  static LogTopic PREGEL;
  static LogTopic QUERIES;
  static LogTopic REPLICATION;
  static LogTopic REQUESTS;
  static LogTopic RESTORE;
  static LogTopic ROCKSDB;
  static LogTopic SECURITY;
  static LogTopic SSL;
  static LogTopic STARTUP;
  static LogTopic STATISTICS;
  static LogTopic SUPERVISION;
  static LogTopic SYSCALL;
  static LogTopic THREADS;
  static LogTopic TRANSACTIONS;
  static LogTopic TTL;
  static LogTopic VALIDATION;
  static LogTopic V8;
  static LogTopic VIEWS;

 public:
  struct FIXED {
    explicit FIXED(double value, int precision = 6) noexcept
        : _value(value), _precision(precision) {}
    double _value;
    int _precision;
  };

  struct CHARS {
    CHARS(char const* data, size_t size) noexcept 
         : data(data), size(size) {}
    char const* data;
    size_t size;
  };

  struct BINARY {
    BINARY(void const* baseAddress, size_t size) noexcept
        : baseAddress(baseAddress), size(size) {}
    explicit BINARY(std::string const& data) noexcept 
        : BINARY(data.data(), data.size()) {}
    void const* baseAddress;
    size_t size;
  };

  struct RANGE {
    RANGE(void const* baseAddress, size_t size) noexcept
        : baseAddress(baseAddress), size(size) {}
    void const* baseAddress;
    size_t size;
  };

  struct LINE {
    explicit LINE(int line) noexcept 
        : _line(line) {}
    int _line;
  };

  struct FILE {
    explicit FILE(char const* file) noexcept 
        : _file(file) {}
    char const* _file;
  };

  struct FUNCTION {
    explicit FUNCTION(char const* function) noexcept 
        : _function(function) {}
    char const* _function;
  };

  struct LOGID {
    explicit LOGID(char const* logid) noexcept 
        : _logid(logid) {}
    char const* _logid;
  };

 public:
  static LogGroup& defaultLogGroup();
  static LogLevel logLevel();
  static std::vector<std::pair<std::string, LogLevel>> logLevelTopics();
  static void setLogLevel(LogLevel);
  static void setLogLevel(std::string const&);
  static void setLogLevel(std::vector<std::string> const&);

  static void setRole(char role);
  static void setOutputPrefix(std::string const&);
  static void setHostname(std::string const&);
  static void setShowIds(bool);
  static bool getShowIds() { return _showIds; };
  static void setShowLineNumber(bool);
  static void setShowRole(bool);
  static bool getShowRole() { return _showRole; };
  static void setShortenFilenames(bool);
  static void setShowProcessIdentifier(bool);
  static void setShowThreadIdentifier(bool);
  static void setShowThreadName(bool);
  static void setUseColor(bool);
  static bool getUseColor() { return _useColor; };
  static void setUseEscaped(bool);
  static bool getUseEscaped() { return _useEscaped; };
  static bool getUseLocalTime() { return LogTimeFormats::isLocalFormat(_timeFormat); }
  static void setTimeFormat(LogTimeFormats::TimeFormat);
  static void setKeepLogrotate(bool);
  static void setLogRequestParameters(bool);
  static bool logRequestParameters() { return _logRequestParameters; }
  static void setUseJson(bool);
  static LogTimeFormats::TimeFormat timeFormat() { return _timeFormat; }

  // can be called after fork()
  static void clearCachedPid() { _cachedPid = 0; }

  static bool translateLogLevel(std::string const& l, bool isGeneral, LogLevel& level) noexcept;

  static std::string const& translateLogLevel(LogLevel) noexcept;

  static void log(char const* logid, char const* function, char const* file, int line,
                  LogLevel level, size_t topicId, std::string const& message);

  static void append(LogGroup&, std::unique_ptr<LogMessage>& msg,
                     bool forceDirect,
                     std::function<void(std::unique_ptr<LogMessage>&)> const& inactive =
                         [](std::unique_ptr<LogMessage>&) -> void {});

  static bool isEnabled(LogLevel level) {
    return (int)level <= (int)_level.load(std::memory_order_relaxed);
  }
  static bool isEnabled(LogLevel level, LogTopic const& topic) {
    return (int)level <= (int)((topic.level() == LogLevel::DEFAULT)
                                   ? _level.load(std::memory_order_relaxed)
                                   : topic.level());
  }

 public:
  static void initialize(application_features::ApplicationServer&, bool);
  static void shutdown();
  static void shutdownLogThread();
  static void flush() noexcept;

 private:
  static Mutex _initializeMutex;

  // these variables might be changed asynchronously
  static std::atomic<bool> _active;
  static std::atomic<LogLevel> _level;

  // these variables must be set before calling initialized
  static LogTimeFormats::TimeFormat _timeFormat;
  static bool _showLineNumber;
  static bool _shortenFilenames;
  static bool _showProcessIdentifier;
  static bool _showThreadIdentifier;
  static bool _showThreadName;
  static bool _showRole;
  static bool _threaded;
  static bool _useColor;
  static bool _useEscaped;
  static bool _keepLogRotate;
  static bool _logRequestParameters;
  static bool _showIds;
  static bool _useJson;
  static char _role;  // current server role to log
  static TRI_pid_t _cachedPid;
  static std::string _outputPrefix;
  static std::string _hostname;

  static std::unique_ptr<LogThread> _loggingThread;
};
}  // namespace arangodb

#endif
