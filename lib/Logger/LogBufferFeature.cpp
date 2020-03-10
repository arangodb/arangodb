////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/tri-strings.h"
#include "Logger/LogAppender.h"
#include "Logger/LoggerFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include <cstring>
#include <utility>

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

LogBuffer::LogBuffer()
    : _id(0), 
      _level(LogLevel::DEFAULT), 
      _topicId(0), 
      _timestamp(0) {
  memset(&_message[0], 0, sizeof(_message));
}

/// @brief logs to a fixed size ring buffer in memory
class LogAppenderRingBuffer final : public LogAppender {
 public:
  LogAppenderRingBuffer() 
      : LogAppender(),
        _id(0) {
    MUTEX_LOCKER(guard, _lock);
    _buffer.resize(LogBufferFeature::BufferSize);
  }

 public:
  void logMessage(LogMessage const& message) override {
    if (message._level == LogLevel::FATAL) {
      // no need to track FATAL messages here, as the process will go down
      // anyway
      return;
    }

    auto timestamp = time(nullptr);

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

  std::string details() const override {
    return std::string();
  }

  /// @brief return all buffered log entries
  std::vector<LogBuffer> entries(LogLevel level, uint64_t start, bool upToLevel) {
    std::vector<LogBuffer> result;
    
    uint64_t s = 0;
    uint64_t n;

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

      ++i;

      if (i >= LogBufferFeature::BufferSize) {
        i = 0;
      }
    }

    return result;
  }

 private:
  Mutex _lock;
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

LogBufferFeature::LogBufferFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "LogBuffer") {
  setOptional(true);
  startsAfter<LoggerFeature>();
  
#ifdef _WIN32
  LogAppender::addGlobalAppender(std::make_shared<LogAppenderDebugOutput>());
  LogAppender::addGlobalAppender(std::make_shared<LogAppenderEventLog>());
#endif
}

void LogBufferFeature::prepare() {
  // only create the in-memory appender when we really need it. if we created it
  // in the ctor, we would waste a lot of memory in case we don't need the in-memory
  // appender. this is the case for simple command such as `--help` etc.
  _inMemoryAppender = std::make_shared<LogAppenderRingBuffer>();
  LogAppender::addGlobalAppender(_inMemoryAppender);
}

std::vector<LogBuffer> LogBufferFeature::entries(LogLevel level, uint64_t start, bool upToLevel) {
  if (_inMemoryAppender == nullptr) {
    return std::vector<LogBuffer>();
  }
  return static_cast<LogAppenderRingBuffer*>(_inMemoryAppender.get())->entries(level, start, upToLevel);
}

}  // namespace arangodb
