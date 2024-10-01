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

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Basics/ReadWriteLock.h"
#include "Basics/threads.h"
#include "Inspection/Status.h"
#include "Logger/Appenders.h"
#include "Logger/LogLevel.h"
#include "Logger/LogTimeFormat.h"
#include "Logger/LogTopic.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
class LogGroup;
class LogThread;

struct LogLevels {
  std::optional<LogLevel> all;
  std::unordered_map<LogTopic*, LogLevel> topics;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, LogLevels& levels)
      -> inspection::Status {
    if constexpr (Inspector::isLoading) {
      std::unordered_map<std::string_view, LogLevel> map;
      auto res = f.apply(map);
      if (!res.ok()) {
        return res;
      }

      for (auto const& [topicName, value] : map) {
        if (topicName == "all") {
          levels.all = value;
        } else {
          auto topic = LogTopic::lookup(topicName);
          if (topic == nullptr) {
            return inspection::Status("Unknown log topic " +
                                      std::string(topicName));
          }
          levels.topics[topic] = value;
        }
      }
      return {};
    } else {
      TRI_ASSERT(!levels.all.has_value());
      std::unordered_map<std::string_view, LogLevel> map;
      for (auto& v : levels.topics) {
        map.emplace(v.first->name(), v.second);
      }
      return f.apply(map);
    }
  }
};

struct AppendersLogLevelConfig {
  LogLevels global;
  std::unordered_map<std::string, LogLevels> appenders;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, AppendersLogLevelConfig& value) {
    return f.object(value).fields(
        f.field("global", value.global).fallback(decltype(value.global){}),
        f.field("appenders", value.appenders)
            .fallback(decltype(value.appenders){}));
  }
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
  friend struct LogAppenderFileFactory;

 public:
  static LogTopic AGENCY;
  static LogTopic AGENCYCOMM;
  static LogTopic AGENCYSTORE;
  static LogTopic AQL;
  static LogTopic AUTHENTICATION;
  static LogTopic AUTHORIZATION;
  static LogTopic BACKUP;
  static LogTopic BENCH;
  static LogTopic CACHE;
  static LogTopic CLUSTER;
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
  static LogTopic LICENSE;
  static LogTopic MAINTENANCE;
  static LogTopic MEMORY;
  static LogTopic QUERIES;
  static LogTopic REPLICATION;
  static LogTopic REPLICATION2;
  static LogTopic REPLICATED_STATE;
  static LogTopic REPLICATED_WAL;
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
  static LogTopic DEPRECATION;

 public:
  struct FIXED {
    explicit FIXED(double value, int precision = 6) noexcept
        : _value(value), _precision(precision) {}
    double _value;
    int _precision;
  };

  struct LINE {
    explicit LINE(int line) noexcept : _line(line) {}
    int _line;
  };

  struct FILE {
    explicit FILE(std::string_view file) noexcept : _file(file) {}
    std::string_view _file;
  };

  struct FUNCTION {
    explicit FUNCTION(std::string_view function) noexcept
        : _function(function) {}
    std::string_view _function;
  };

  struct LOGID {
    explicit LOGID(std::string_view logid) noexcept : _logid(logid) {}
    std::string_view _logid;
  };

 public:
  static constexpr std::string_view logThreadName = "Logging";

  static LogGroup& defaultLogGroup();
  static LogLevel logLevel();
  static std::unordered_set<std::string> structuredLogParams();
  static auto logLevelTopics() -> std::unordered_map<LogTopic*, LogLevel>;
  static auto getLogLevels() -> LogLevels;
  static auto getAppendersConfig() -> AppendersLogLevelConfig;

  static void resetLevelsToDefault();
  static void setLogLevel(LogLevel);
  static void setLogLevel(std::string const&);
  static void setLogLevel(TopicName topic, LogLevel level);
  static void setLogLevel(LogTopic&, LogLevel level);
  static void setLogLevel(std::vector<std::string> const&);
  static void setLogLevel(LogLevels const&);
  [[nodiscard]] static Result setLogLevel(AppendersLogLevelConfig const&);

  static std::unordered_map<std::string, bool> parseStringParams(
      std::vector<std::string> const&);
  static void setLogStructuredParamsOnServerStart(
      std::vector<std::string> const&);
  static void setLogStructuredParams(
      std::unordered_map<std::string, bool> const& paramsAndValues);
  static void setRole(char role);
  static void setOutputPrefix(std::string const&);
  static void setHostname(std::string const&);
  static void setShowIds(bool);
  static bool getShowIds() { return _showIds; };
  static void setShowLineNumber(bool);
  static void setShowRole(bool);
  static void setShortenFilenames(bool);
  static void setShowProcessIdentifier(bool);
  static void setShowThreadIdentifier(bool);
  static void setShowThreadName(bool);
  static void setUseColor(bool);
  static bool getUseColor() { return _useColor; };
  static void setUseControlEscaped(bool);
  static void setUseUnicodeEscaped(bool);
  static void setEscaping();
  static bool getUseControlEscaped() { return _useControlEscaped; };
  static bool getUseUnicodeEscaped() { return _useUnicodeEscaped; };
  static bool getUseLocalTime() {
    return LogTimeFormats::isLocalFormat(_timeFormat);
  }
  static void setTimeFormat(LogTimeFormats::TimeFormat);
  static void setKeepLogrotate(bool);
  static void setLogRequestParameters(bool);
  static bool logRequestParameters() { return _logRequestParameters; }
  static void setUseJson(bool);
  static LogTimeFormats::TimeFormat timeFormat() { return _timeFormat; }

  static void reopen();
  static void addAppender(LogGroup const& group, std::string const& definition);
  static void addGlobalAppender(LogGroup const& group,
                                std::shared_ptr<LogAppender> appender);
  static bool haveAppenders(LogGroup const& group, size_t topicId);
  static void log(LogGroup const& group, LogMessage const& message);

  static bool allowStdLogging() { return _allowStdLogging; }
  static void allowStdLogging(bool value) { _allowStdLogging = value; }

  // can be called after fork()
  static void clearCachedPid() {
    _cachedPid.store(0, std::memory_order_relaxed);
  }

  static bool translateLogLevel(std::string const& l, bool isGeneral,
                                LogLevel& level) noexcept;

  static std::string_view translateLogLevel(LogLevel) noexcept;

  static void log(std::string_view logid, std::string_view function,
                  std::string_view file, int line, LogLevel level,
                  size_t topicId, std::string_view message);

  static void append(
      LogGroup&, std::unique_ptr<LogMessage> msg, bool forceDirect,
      std::function<void(LogMessage const&)> const& inactive =
          [](LogMessage const&) -> void {});

  static bool isEnabled(LogLevel level) {
    return (int)level <= (int)_level.load(std::memory_order_relaxed);
  }
  static bool isEnabled(LogLevel level, LogTopic const& topic) {
    return (int)level <= (int)((topic.level() == LogLevel::DEFAULT)
                                   ? _level.load(std::memory_order_relaxed)
                                   : topic.level());
  }

  static void initialize(bool threaded, uint32_t maxQueuedLogMessages);
  static void shutdown();
  static void flush() noexcept;

  static void setOnDroppedMessage(std::function<void()> cb);
  static void onDroppedMessage() noexcept;

 private:
  static void doSetAllLevels(LogLevel);
  static void doSetGlobalLevel(LogTopic&, LogLevel);

  static void calculateEffectiveLogLevels();
  static void buildJsonLogMessage(std::string& out, std::string_view logid,
                                  std::string_view function,
                                  std::string_view file, int line,
                                  LogLevel level, size_t topicId,
                                  std::string_view message, bool& shrunk);

  static void buildTextLogMessage(std::string& out, std::string_view logid,
                                  std::string_view function,
                                  std::string_view file, int line,
                                  LogLevel level, size_t topicId,
                                  std::string_view message, uint32_t& offset,
                                  bool& shrunk);

  // these variables might be changed asynchronously
  static std::atomic<bool> _active;
  static std::atomic<LogLevel> _level;

  static std::mutex _appenderModificationMutex;
  static logger::Appenders _appenders;
  static bool _allowStdLogging;

  // these variables must be set before calling initialized
  static std::unordered_set<std::string>
      _structuredLogParams;  // if in set, means value is true, else, means it's
                             // false
  static arangodb::basics::ReadWriteLock _structuredParamsLock;
  static LogTimeFormats::TimeFormat _timeFormat;
  static bool _showLineNumber;
  static bool _shortenFilenames;
  static bool _showProcessIdentifier;
  static bool _showThreadIdentifier;
  static bool _showThreadName;
  static bool _showRole;
  static bool _useColor;
  static bool _useControlEscaped;
  static bool _useUnicodeEscaped;
  static bool _keepLogRotate;
  static bool _logRequestParameters;
  static bool _showIds;
  static bool _useJson;
  static char _role;  // current server role to log
  static std::atomic<TRI_pid_t> _cachedPid;
  static std::string _outputPrefix;
  static std::string _hostname;
  static void (*_writerFn)(std::string_view, std::string&);

  struct ThreadRef {
    ThreadRef();
    ~ThreadRef();

    ThreadRef(ThreadRef const&) = delete;
    ThreadRef(ThreadRef&&) = delete;
    ThreadRef& operator=(ThreadRef const&) = delete;
    ThreadRef& operator=(ThreadRef&&) = delete;

    LogThread* operator->() const noexcept { return _thread; }
    operator bool() const noexcept { return _thread != nullptr; }

   private:
    LogThread* _thread;
  };

  // logger thread. only populated when threaded logging is selected.
  // the pointer must only be used with atomic accessors after the ref counter
  // has been increased. Best to use the ThreadRef class for this!
  static std::atomic<std::size_t> _loggingThreadRefs;
  static std::atomic<LogThread*> _loggingThread;

  static std::function<void()> _onDroppedMessage;
};
}  // namespace arangodb
