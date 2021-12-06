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

#include "LogBufferFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "Basics/system-functions.h"
#include "Basics/tri-strings.h"
#include "Logger/LogAppender.h"
#include "Logger/LoggerFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/MetricsFeature.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include <cstring>
#include <utility>

using namespace arangodb::basics;
using namespace arangodb::options;

DECLARE_COUNTER(arangodb_logger_warnings_total, "Number of warnings logged.");
DECLARE_COUNTER(arangodb_logger_errors_total, "Number of errors logged.");

namespace arangodb {

LogBuffer::LogBuffer()
    : _id(0), 
      _level(LogLevel::DEFAULT), 
      _topicId(0), 
      _timestamp(0.0) {
  memset(&_message[0], 0, sizeof(_message));
}

/// @brief logs to a fixed size ring buffer in memory
class LogAppenderRingBuffer final : public LogAppender {
 public:
  explicit LogAppenderRingBuffer(LogLevel minLogLevel) 
      : LogAppender(),
        _minLogLevel(minLogLevel),
        _id(0) {
    MUTEX_LOCKER(guard, _lock);
    _buffer.resize(LogBufferFeature::BufferSize);
  }

 public:
  void logMessage(LogMessage const& message) override {
    if (message._level > _minLogLevel) {
      // logger not configured to log these messages
      return;
    }

    double timestamp = TRI_microtime();

    MUTEX_LOCKER(guard, _lock);

    uint64_t n = _id++;
    LogBuffer& ptr = _buffer[n % LogBufferFeature::BufferSize];

    ptr._id = n;
    ptr._level = message._level;
    ptr._topicId = static_cast<uint32_t>(message._topicId);
    ptr._timestamp = timestamp;
    TRI_CopyString(ptr._message, message._message.c_str() + message._offset,
                   sizeof(ptr._message) - 1);
  }

  void clear() {
    MUTEX_LOCKER(guard, _lock);
    _id = 0;
    _buffer.clear();
    _buffer.resize(LogBufferFeature::BufferSize);
  }

  std::string details() const override {
    return std::string();
  }

  /// @brief return all buffered log entries
  std::vector<LogBuffer> entries(LogLevel level, uint64_t start, bool upToLevel,
                                 std::string const& searchString) {
    std::vector<LogBuffer> result;
    result.reserve(16);
    
    uint64_t s = 0;
    uint64_t n;
  
    std::string search;
    if (!searchString.empty()) {
      search = arangodb::basics::StringUtils::tolower(searchString);
    } 

    MUTEX_LOCKER(guard, _lock);

    if (_id >= LogBufferFeature::BufferSize) {
      s = _id % LogBufferFeature::BufferSize;
      n = LogBufferFeature::BufferSize;
    } else {
      n = static_cast<uint64_t>(_id);
    }

    for (uint64_t i = s; 0 < n; --n) {
      LogBuffer const& p = _buffer[i];

      if (p._id >= start) {
        bool matches = (search.empty() ||
                        arangodb::basics::StringUtils::tolower(p._message).find(search) != std::string::npos);

        if (matches) {
          if (upToLevel) {
            if (static_cast<int>(p._level) <= static_cast<int>(level)) {
              result.emplace_back(p);
            }
          } else {
            if (p._level == level) {
              result.emplace_back(p);
            }
          }
        }
      }

      ++i;

      if (i >= LogBufferFeature::BufferSize) {
        i = 0;
      }
    }

    return result;
  }

 private:
  Mutex _lock;
  LogLevel const _minLogLevel;
  uint64_t _id;
  std::vector<LogBuffer> _buffer;
};

#ifdef _WIN32
/// logs to the debug output windows in MSVC
class LogAppenderDebugOutput final : public LogAppender {
 public:
  LogAppenderDebugOutput() : LogAppender() {}

 public:
  void logMessage(LogMessage const& message) {
    // only handle FATAl and ERR log messages
    if (message._level != LogLevel::FATAL && message._level != LogLevel::ERR) {
      return;
    }
    
    // log these errors to the debug output window in MSVC so
    // we can see them during development
    OutputDebugString(message._message.data() + message._offset);
    OutputDebugString("\r\n");
  }

  std::string details() const override {
    return std::string();
  }
};

/// logs to the Windows event log
class LogAppenderEventLog final : public LogAppender {
 public:
  LogAppenderEventLog() : LogAppender() {}

 public:
  void logMessage(LogMessage const& message) {
    // only handle FATAl and ERR log messages
    if (message._level != LogLevel::FATAL && message._level != LogLevel::ERR) {
      return;
    }
    
    TRI_LogWindowsEventlog(message._function, message._file, message._line, message._message);
  }

  std::string details() const override {
    return std::string();
  }
};
#endif

/// @brief log appender that increases counters for warnings/errors
/// in our metrics
class LogAppenderMetricsCounter final : public LogAppender {
 public:
  LogAppenderMetricsCounter(application_features::ApplicationServer& server)
      : LogAppender(),
        _warningsCounter(server.getFeature<metrics::MetricsFeature>().add(arangodb_logger_warnings_total{})),
        _errorsCounter(server.getFeature<metrics::MetricsFeature>().add(arangodb_logger_errors_total{})) {}

  void logMessage(LogMessage const& message) override {
    // only handle WARN and ERR log messages
    if (message._level == LogLevel::WARN) {
      ++_warningsCounter;
    } else if (message._level == LogLevel::ERR) {
      ++_errorsCounter;
    }
  }

  std::string details() const override {
    return std::string();
  }

 private:
  metrics::Counter& _warningsCounter;
  metrics::Counter& _errorsCounter;
};

LogBufferFeature::LogBufferFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "LogBuffer"),
      _minInMemoryLogLevel("info"),
      _useInMemoryAppender(true) {
  setOptional(true);
  startsAfter<LoggerFeature>();
  
#ifdef _WIN32
  LogAppender::addGlobalAppender(Logger::defaultLogGroup(),
                                 std::make_shared<LogAppenderDebugOutput>());
  LogAppender::addGlobalAppender(Logger::defaultLogGroup(),
                                 std::make_shared<LogAppenderEventLog>());
#endif
  LogAppender::addGlobalAppender(Logger::defaultLogGroup(),
                                 std::make_shared<LogAppenderMetricsCounter>(server));
}
  
void LogBufferFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options
      ->addOption("--log.in-memory", "use in-memory log appender, which can be queried via API and web UI",
                  new BooleanParameter(&_useInMemoryAppender),
                  arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
                  .setIntroducedIn(30800);
  
  std::unordered_set<std::string> const logLevels = { "fatal", "error", "err", "warning", "warn", "info", "debug", "trace" };
  options
      ->addOption("--log.in-memory-level", "use in-memory log appender only for this log level and higher",
                  new DiscreteValuesParameter<StringParameter>(
                      &_minInMemoryLogLevel, logLevels),
                  arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
                  .setIntroducedIn(30709);
}

void LogBufferFeature::prepare() {
  TRI_ASSERT(_inMemoryAppender == nullptr);

  if (_useInMemoryAppender) {
    // only create the in-memory appender when we really need it. if we created it
    // in the ctor, we would waste a lot of memory in case we don't need the in-memory
    // appender. this is the case for simple command such as `--help` etc.
    LogLevel level;
    bool isValid = Logger::translateLogLevel(_minInMemoryLogLevel, true, level);
    if (!isValid) {
      level = LogLevel::INFO;
    }

    _inMemoryAppender = std::make_shared<LogAppenderRingBuffer>(level);
    LogAppender::addGlobalAppender(Logger::defaultLogGroup(), _inMemoryAppender);
  }
}

void LogBufferFeature::clear() {
  if (_inMemoryAppender != nullptr) {
    static_cast<LogAppenderRingBuffer*>(_inMemoryAppender.get())->clear();
  }
}

std::vector<LogBuffer> LogBufferFeature::entries(LogLevel level, uint64_t start, bool upToLevel, 
                                                 std::string const& searchString) {
  if (_inMemoryAppender == nullptr) {
    return std::vector<LogBuffer>();
  }
  TRI_ASSERT(_useInMemoryAppender);
  return static_cast<LogAppenderRingBuffer*>(_inMemoryAppender.get())->entries(level, start, upToLevel, searchString);
}

}  // namespace arangodb
