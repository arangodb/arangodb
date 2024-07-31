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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "LogBufferFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "Basics/system-functions.h"
#include "Basics/tri-strings.h"
#include "Logger/LogAppender.h"
#include "Logger/LogMessage.h"
#include "Logger/LoggerFeature.h"
#include "Logger/Logger.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#include <cstring>
#include <utility>

using namespace arangodb::basics;
using namespace arangodb::options;

DECLARE_COUNTER(arangodb_logger_warnings_total, "Number of warnings logged.");
DECLARE_COUNTER(arangodb_logger_errors_total, "Number of errors logged.");
DECLARE_COUNTER(arangodb_logger_messages_dropped_total,
                "Number of log messages dropped.");

namespace arangodb {

LogBuffer::LogBuffer()
    : _id(0), _level(LogLevel::DEFAULT), _topicId(0), _timestamp(0.0) {
  memset(&_message[0], 0, sizeof(_message));
}

/// @brief logs to a fixed size ring buffer in memory
class LogAppenderRingBuffer final : public LogAppender {
 public:
  explicit LogAppenderRingBuffer(LogLevel minLogLevel)
      : LogAppender(), _minLogLevel(minLogLevel), _id(0) {
    std::lock_guard guard{_lock};
    _buffer.resize(LogBufferFeature::BufferSize);
  }

 public:
  void logMessage(LogMessage const& message) override {
    if (message._level > _minLogLevel) {
      // logger not configured to log these messages
      return;
    }

    double timestamp = TRI_microtime();

    std::lock_guard guard{_lock};

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
    std::lock_guard guard{_lock};
    _id = 0;
    _buffer.clear();
    _buffer.resize(LogBufferFeature::BufferSize);
  }

  std::string details() const override { return std::string(); }

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

    std::lock_guard guard{_lock};

    if (_id >= LogBufferFeature::BufferSize) {
      s = _id % LogBufferFeature::BufferSize;
      n = LogBufferFeature::BufferSize;
    } else {
      n = static_cast<uint64_t>(_id);
    }

    for (uint64_t i = s; 0 < n; --n) {
      LogBuffer const& p = _buffer[i];

      if (p._id >= start) {
        bool matches =
            (search.empty() ||
             arangodb::basics::StringUtils::tolower(p._message).find(search) !=
                 std::string::npos);

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
  std::mutex _lock;
  LogLevel const _minLogLevel;
  uint64_t _id;
  std::vector<LogBuffer> _buffer;
};

/// @brief log appender that increases counters for warnings/errors
/// in our metrics
class LogAppenderMetricsCounter final : public LogAppender {
 public:
  LogAppenderMetricsCounter(metrics::MetricsFeature& metrics)
      : LogAppender(),
        _warningsCounter(metrics.add(arangodb_logger_warnings_total{})),
        _errorsCounter(metrics.add(arangodb_logger_errors_total{})),
        _droppedMessagesCounter(
            metrics.add(arangodb_logger_messages_dropped_total{})) {}

  void logMessage(LogMessage const& message) override {
    // only handle WARN and ERR log messages
    if (message._level == LogLevel::WARN) {
      ++_warningsCounter;
    } else if (message._level == LogLevel::ERR) {
      ++_errorsCounter;
    }
  }

  void trackDroppedMessage() noexcept { ++_droppedMessagesCounter; }

  std::string details() const override { return std::string(); }

 private:
  metrics::Counter& _warningsCounter;
  metrics::Counter& _errorsCounter;
  metrics::Counter& _droppedMessagesCounter;
};

LogBufferFeature::LogBufferFeature(Server& server)
    : ArangodFeature{server, *this},
      _minInMemoryLogLevel("info"),
      _useInMemoryAppender(true) {
  setOptional(true);
  startsAfter<LoggerFeature>();

  _metricsCounter = std::make_shared<LogAppenderMetricsCounter>(
      server.getFeature<metrics::MetricsFeature>());

  Logger::addGlobalAppender(Logger::defaultLogGroup(), _metricsCounter);

  Logger::setOnDroppedMessage([mc = _metricsCounter]() noexcept {
    std::static_pointer_cast<LogAppenderMetricsCounter>(mc)
        ->trackDroppedMessage();
  });
}

void LogBufferFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options
      ->addOption("--log.in-memory",
                  "Use an in-memory log appender which can be queried via the "
                  "API and web interface.",
                  new BooleanParameter(&_useInMemoryAppender),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30800)
      .setLongDescription(R"(You can use this option to toggle storing log
messages in memory, from which they can be consumed via the `/_admin/log`
HTTP API and via the web interface.

By default, this option is turned on, so log messages are consumable via the API
and web interface. Turning this option off disables that functionality, saves a
bit of memory for the in-memory log buffers, and prevents potential log
information leakage via these means.)");

  std::unordered_set<std::string> const logLevels = {
      "fatal", "error", "err", "warning", "warn", "info", "debug", "trace"};
  options
      ->addOption(
          "--log.in-memory-level",
          "Use an in-memory log appender only for this log level and higher.",
          new DiscreteValuesParameter<StringParameter>(&_minInMemoryLogLevel,
                                                       logLevels),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(You can use this option to control which log
messages are preserved in memory (in case `--log.in-memory` is enabled).

The default value is `info`, meaning all log messages of types `info`,
`warning`, `error`, and `fatal` are stored in-memory by an instance. By setting
this option to `warning`, only `warning`, `error` and `fatal` log messages are 
preserved in memory, and by setting the option to `error`, only `error` and
`fatal` messages are kept.

This option is useful because the number of in-memory log messages is limited 
to the latest 2048 messages, and these slots are shared between informational,
warning, and error messages by default.)");
}

void LogBufferFeature::prepare() {
  TRI_ASSERT(_inMemoryAppender == nullptr);

  if (_useInMemoryAppender) {
    // only create the in-memory appender when we really need it. if we created
    // it in the ctor, we would waste a lot of memory in case we don't need the
    // in-memory appender. this is the case for simple command such as `--help`
    // etc.
    LogLevel level;
    bool isValid = Logger::translateLogLevel(_minInMemoryLogLevel, true, level);
    if (!isValid) {
      level = LogLevel::INFO;
    }

    _inMemoryAppender = std::make_shared<LogAppenderRingBuffer>(level);
    Logger::addGlobalAppender(Logger::defaultLogGroup(), _inMemoryAppender);
  }
}

void LogBufferFeature::clear() {
  if (_inMemoryAppender != nullptr) {
    static_cast<LogAppenderRingBuffer*>(_inMemoryAppender.get())->clear();
  }
}

std::vector<LogBuffer> LogBufferFeature::entries(
    LogLevel level, uint64_t start, bool upToLevel,
    std::string const& searchString) {
  if (_inMemoryAppender == nullptr) {
    return std::vector<LogBuffer>();
  }
  TRI_ASSERT(_useInMemoryAppender);
  return static_cast<LogAppenderRingBuffer*>(_inMemoryAppender.get())
      ->entries(level, start, upToLevel, searchString);
}

}  // namespace arangodb
