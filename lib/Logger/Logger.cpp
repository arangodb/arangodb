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

#include <chrono>
#include <cstring>
#include <iosfwd>
#include <thread>
#include <type_traits>

#include "Logger.h"

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/operating-system.h"
#include "Basics/system-functions.h"
#include "Basics/voc-errors.h"
#include "Logger/LogAppender.h"
#include "Logger/LogAppenderFile.h"
#include "Logger/LogGroup.h"
#include "Logger/LogMacros.h"
#include "Logger/LogThread.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <velocypack/Dumper.h>
#include <velocypack/Sink.h>

using namespace arangodb;
using namespace arangodb::basics;

namespace {
static std::string const DEFAULT = "DEFAULT";
static std::string const FATAL = "FATAL";
static std::string const ERR = "ERROR";
static std::string const WARN = "WARNING";
static std::string const INFO = "INFO";
static std::string const DEBUG = "DEBUG";
static std::string const TRACE = "TRACE";
static std::string const UNKNOWN = "UNKNOWN";

std::string const LogThreadName("Logging");

class DefaultLogGroup final : public LogGroup {
  std::size_t id() const override { return 0; }
};
DefaultLogGroup defaultLogGroupInstance;

}  // namespace
  
LogMessage::LogMessage(char const* function, char const* file, int line,
                       LogLevel level, size_t topicId, std::string&& message, 
                       uint32_t offset, bool shrunk) noexcept
    : _function(function),
      _file(file),
      _line(line),
      _level(level),
      _topicId(topicId),
      _message(std::move(message)),
      _offset(offset),
      _shrunk(shrunk) {}
  
void LogMessage::shrink(std::size_t maxLength) {
  // no need to shrink an already shrunk message
  if (!_shrunk && _message.size() > maxLength) {
    _message.resize(maxLength);

    // normally, offset should be around 20 to 30 bytes,
    // whereas the minimum for maxLength should be around 256 bytes.
    TRI_ASSERT(maxLength > _offset);
    if (_offset > _message.size()) {
      // we need to make sure that the offset is not outside of the message
      // after shrinking
      _offset = static_cast<uint32_t>(_message.size());
    }
    _message.append("...", 3);
    _shrunk = true;
  }
}


Mutex Logger::_initializeMutex;

std::atomic<bool> Logger::_active(false);
std::atomic<LogLevel> Logger::_level(LogLevel::INFO);

LogTimeFormats::TimeFormat Logger::_timeFormat(LogTimeFormats::TimeFormat::UTCDateString);
bool Logger::_showIds(false);
bool Logger::_showLineNumber(false);
bool Logger::_shortenFilenames(true);
bool Logger::_showProcessIdentifier(true);
bool Logger::_showThreadIdentifier(false);
bool Logger::_showThreadName(false);
bool Logger::_threaded(false);
bool Logger::_useColor(true);
bool Logger::_useEscaped(true);
bool Logger::_keepLogRotate(false);
bool Logger::_logRequestParameters(true);
bool Logger::_showRole(false);
bool Logger::_useJson(false);
char Logger::_role('\0');
TRI_pid_t Logger::_cachedPid(0);
std::string Logger::_outputPrefix;
std::string Logger::_hostname;

std::unique_ptr<LogThread> Logger::_loggingThread(nullptr);

LogGroup& Logger::defaultLogGroup() { return ::defaultLogGroupInstance; }

LogLevel Logger::logLevel() { return _level.load(std::memory_order_relaxed); }

std::vector<std::pair<std::string, LogLevel>> Logger::logLevelTopics() {
  return LogTopic::logLevelTopics();
}

void Logger::setShowIds(bool show) {
  _showIds = show;
}

void Logger::setLogLevel(LogLevel level) {
  _level.store(level, std::memory_order_relaxed);
}

void Logger::setLogLevel(std::string const& levelName) {
  std::string l = StringUtils::tolower(levelName);
  std::vector<std::string> v = StringUtils::split(l, '=');

  if (v.empty() || v.size() > 2) {
    Logger::setLogLevel(LogLevel::INFO);
    LOG_TOPIC("b83c6", ERR, arangodb::Logger::FIXME)
        << "strange log level '" << levelName << "', using log level 'info'";
    return;
  }

  // if log level is "foo = bar", we better get rid of the whitespace
  StringUtils::trimInPlace(v[0]);
  bool isGeneral = v.size() == 1;

  if (!isGeneral) {
    StringUtils::trimInPlace(v[1]);
    l = v[1];
  }

  LogLevel level;
  bool isValid = translateLogLevel(l, isGeneral, level);

  if (!isValid) {
    if (!isGeneral) {
      LOG_TOPIC("05367", WARN, arangodb::Logger::FIXME) << "strange log level '" << levelName << "'";
      return;
    }
    level = LogLevel::INFO;
    LOG_TOPIC("d880b", WARN, arangodb::Logger::FIXME)
        << "strange log level '" << levelName << "', using log level 'info'";
  }

  if (isGeneral) {
    Logger::setLogLevel(level);
    // setting the log level for topic "general" is required here, too,
    // as "fixme" is the previous general log topic...
    LogTopic::setLogLevel(std::string("general"), level);
  } else {
    LogTopic::setLogLevel(v[0], level);
  }
}

void Logger::setLogLevel(std::vector<std::string> const& levels) {
  for (auto const& level : levels) {
    setLogLevel(level);
  }
}

void Logger::setRole(char role) { _role = role; }

// NOTE: this function should not be called if the logging is active.
void Logger::setOutputPrefix(std::string const& prefix) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _outputPrefix = prefix;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setHostname(std::string const& hostname) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  if (hostname == "auto") {
    _hostname = utilities::hostname();
  } else {
    _hostname = hostname;
  }
}

// NOTE: this function should not be called if the logging is active.
void Logger::setShowLineNumber(bool show) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _showLineNumber = show;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setShortenFilenames(bool shorten) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _shortenFilenames = shorten;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setShowProcessIdentifier(bool show) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _showProcessIdentifier = show;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setShowThreadIdentifier(bool show) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _showThreadIdentifier = show;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setShowThreadName(bool show) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _showThreadName = show;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setUseColor(bool value) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _useColor = value;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setUseEscaped(bool value) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _useEscaped = value;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setShowRole(bool show) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _showRole = show;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setTimeFormat(LogTimeFormats::TimeFormat format) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _timeFormat = format;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setKeepLogrotate(bool keep) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _keepLogRotate = keep;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setLogRequestParameters(bool log) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _logRequestParameters = log;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setUseJson(bool value) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  _useJson = value;
}

bool Logger::translateLogLevel(std::string const& l, bool isGeneral, LogLevel& level) noexcept {
  if (l == "fatal") {
    level = LogLevel::FATAL;
  } else if (l == "error" || l == "err") {
    level = LogLevel::ERR;
  } else if (l == "warning" || l == "warn") {
    level = LogLevel::WARN;
  } else if (l == "info") {
    level = LogLevel::INFO;
  } else if (l == "debug") {
    level = LogLevel::DEBUG;
  } else if (l == "trace") {
    level = LogLevel::TRACE;
  } else if (!isGeneral && (l.empty() || l == "default")) {
    level = LogLevel::DEFAULT;
  } else {
    return false;
  }

  return true;
}

std::string const& Logger::translateLogLevel(LogLevel level) noexcept {
  switch (level) {
    case LogLevel::ERR:
      return ERR;
    case LogLevel::WARN:
      return WARN;
    case LogLevel::INFO:
      return INFO;
    case LogLevel::DEBUG:
      return DEBUG;
    case LogLevel::TRACE:
      return TRACE;
    case LogLevel::FATAL:
      return FATAL;
    case LogLevel::DEFAULT:
      return DEFAULT;
  }

  return UNKNOWN;
}

void Logger::log(char const* logid, char const* function, char const* file, int line,
                 LogLevel level, size_t topicId, std::string const& message) try {
  TRI_ASSERT(logid != nullptr);

  // we only determine our pid once, as currentProcessId() will
  // likely do a syscall.
  // this read-check-update sequence is not thread-safe, but this
  // should not matter, as the pid value is only changed from 0 to the
  // actual pid and never changes afterwards
  if (_cachedPid == 0) {
    _cachedPid = Thread::currentProcessId();
  }

  std::string out;
  out.reserve(256 + message.size());

  uint32_t offset = 0;
  bool shrunk = false;

  if (Logger::_useJson) {
    // construct JSON output
    arangodb::velocypack::StringSink sink(&out);
    arangodb::velocypack::Dumper dumper(&sink);

    out.push_back('{');

    // current date/time
    {
      out.append("\"time\":");
      if (LogTimeFormats::isStringFormat(_timeFormat)) {
        out.push_back('"');
      }
      // value of date/time is always safe to print
      LogTimeFormats::writeTime(out, _timeFormat, std::chrono::system_clock::now());
      if (LogTimeFormats::isStringFormat(_timeFormat)) {
        out.push_back('"');
      }
    }

    // prefix
    if (!_outputPrefix.empty()) {
      out.append(",\"prefix\":");
      dumper.appendString(_outputPrefix.data(), _outputPrefix.size());
    }

    // pid
    if (_showProcessIdentifier) {
      out.append(",\"pid\":");
      StringUtils::itoa(uint64_t(_cachedPid), out);
    }

    // tid
    if (_showThreadIdentifier) {
      out.append(",\"tid\":");
      StringUtils::itoa(uint64_t(Thread::currentThreadNumber()), out);
    }

    // thread name
    if (_showThreadName) {
      char const* threadName = Thread::currentThreadName();
      if (threadName == nullptr) {
        threadName = "main";
      }

      out.append(",\"thread\":");
      dumper.appendString(threadName, strlen(threadName));
    }

    // role
    if (_showRole) {
      out.append(",\"role\":\"");
      if (_role != '\0') {
        // value of _role is always safe to print
        out.push_back(_role);
      }
      out.push_back('"');
    }

    // log level
    {
      out.append(",\"level\":");
      auto const& ll = Logger::translateLogLevel(level);
      // value of level is always safe to print
      dumper.appendString(ll.data(), ll.size());
    }

    // file and line
    if (_showLineNumber && file != nullptr) {
      char const* filename = file;
      char const* shortened = strrchr(filename, TRI_DIR_SEPARATOR_CHAR);
      if (shortened != nullptr) {
        filename = shortened + 1;
      }

      out.append(",\"file\":");
      dumper.appendString(filename, strlen(filename));
    }

    if (_showLineNumber) {
      out.append(",\"line\":");
      StringUtils::itoa(uint64_t(line), out);
    }

    if (_showLineNumber && function != nullptr) {
      out.append(",\"function\":");
      dumper.appendString(function, strlen(function));
    }

    // the topic
    {
      out.append(",\"topic\":");
      std::string topic = LogTopic::lookup(topicId);
      // value of topic is always safe to print
      dumper.appendString(topic.data(), topic.size());
    }

    // the logid
    if (::arangodb::Logger::getShowIds()) {
      out.append(",\"id\":");
      // value of id is always safe to print
      dumper.appendString(logid, strlen(logid));
    }

    // hostname
    if (!_hostname.empty()) {
      out.append(",\"hostname\":");
      dumper.appendString(_hostname.data(), _hostname.size());
    }

    // the message itself
    {
      out.append(",\"message\":");
  
      // the log message can be really large, and it can lead to 
      // truncation of the log message further down the road.
      // however, as we are supposed to produce valid JSON log
      // entries even with the truncation in place, we need to make
      // sure that the dynamic text part is truncated and not the
      // entries JSON thing
      size_t maxMessageLength = defaultLogGroup().maxLogEntryLength();
      // cut of prologue, the quotes ('"' --- ' '") and the final '}'
      if (maxMessageLength >= out.size() + 3) {
        maxMessageLength -= out.size() + 3;
      }
      if (maxMessageLength > message.size()) {
        maxMessageLength = message.size();
      }
      dumper.appendString(message.c_str(), maxMessageLength);
  
      // this tells the logger to not shrink our (potentially already
      // shrunk) message once more - if it would shrink the message again,
      // it may produce invalid JSON
      shrunk = true;
    }

    out.push_back('}');

    TRI_ASSERT(offset == 0);
  } else {
    // hostname
    if (!_hostname.empty()) {
      out.append(_hostname);
      out.push_back(' ');
    }

    // human readable format
    LogTimeFormats::writeTime(out, _timeFormat, std::chrono::system_clock::now());
    out.push_back(' ');

    // output prefix
    if (!_outputPrefix.empty()) {
      out.append(_outputPrefix);
      out.push_back(' ');
    }

    // [pid-tid-threadname], all components are optional
    bool haveProcessOutput = false;
    if (_showProcessIdentifier) {
      // append the process / thread identifier
      TRI_ASSERT(_cachedPid != 0);
      out.push_back('[');
      StringUtils::itoa(uint64_t(_cachedPid), out);
      haveProcessOutput = true;
    }

    if (_showThreadIdentifier) {
      out.push_back(haveProcessOutput ? '-' : '[');
      StringUtils::itoa(uint64_t(Thread::currentThreadNumber()), out);
      haveProcessOutput = true;
    }

    // log thread name
    if (_showThreadName) {
      char const* threadName = Thread::currentThreadName();
      if (threadName == nullptr) {
        threadName = "main";
      }

      out.push_back(haveProcessOutput ? '-' : '[');
      out.append(threadName);
      haveProcessOutput = true;
    }

    if (haveProcessOutput) {
      out.append("] ", 2);
    }

    if (_showRole && _role != '\0') {
      out.push_back(_role);
      out.push_back(' ');
    }

    // log level
    out.append(Logger::translateLogLevel(level));
    out.push_back(' ');

    // check if we must display the line number
    if (_showLineNumber && file != nullptr && function != nullptr) {
      char const* filename = file;

      if (_shortenFilenames) {
        // shorten file names from `/home/.../file.cpp` to just `file.cpp`
        char const* shortened = strrchr(filename, TRI_DIR_SEPARATOR_CHAR);
        if (shortened != nullptr) {
          filename = shortened + 1;
        }
      }
      out.push_back('[');
      out.append(function);
      out.push_back('@');
      out.append(filename);
      out.push_back(':');
      StringUtils::itoa(uint64_t(line), out);
      out.append("] ", 2);
    }

    // the offset is used by the in-memory logger, and it cuts off everything from the start
    // of the concatenated log string until the offset. only what's after the offset gets
    // displayed in the web UI
    TRI_ASSERT(out.size() < static_cast<size_t>(UINT32_MAX));
    offset = static_cast<uint32_t>(out.size());

    if (::arangodb::Logger::getShowIds()) {
      out.push_back('[');
      out.append(logid);
      out.append("] ", 2);
    }

    {
      out.push_back('{');
      out.append(LogTopic::lookup(topicId));
      out.append("} ", 2);
    }

    // generate the complete message
    out.append(message);
  }

  TRI_ASSERT(offset == 0 || !_useJson);

  auto msg = std::make_unique<LogMessage>(function, file, line, level, topicId, std::move(out), offset, shrunk);

  append(defaultLogGroup(), msg, false, [level, topicId](std::unique_ptr<LogMessage>& msg) -> void {
    LogAppenderStdStream::writeLogMessage(STDERR_FILENO, (isatty(STDERR_FILENO) == 1),
                                          level, topicId, msg->_message.data(),
                                          msg->_message.size(), true);
  });
} catch (...) {
  // logging itself must never cause an exeption to escape
}

void Logger::append(LogGroup& group, 
                    std::unique_ptr<LogMessage>& msg,
                    bool forceDirect,
                    std::function<void(std::unique_ptr<LogMessage>&)> const& inactive) {
  // check if we need to shrink the message here
  if (!msg->shrunk()) {
    msg->shrink(group.maxLogEntryLength());
  }

  // first log to all "global" appenders, which are the in-memory ring buffer logger plus
  // some Windows-specifc appenders for the debug output window and the Windows event log.
  // note that these loggers do not require any configuration so we can always and safely invoke them.
  LogAppender::logGlobal(group, *msg);

  if (!_active.load(std::memory_order_relaxed)) {
    // logging is still turned off. now use hard-coded to-stderr logging
    inactive(msg);
  } else {
    // now either queue or output the message
    bool handled = false;
    if (_threaded && !forceDirect) {
      handled = _loggingThread->log(group, msg);
    }

    if (!handled) {
      TRI_ASSERT(msg != nullptr);

      TRI_IF_FAILURE("Logger::append") {
        // cut off all logging
        return;
      }

      LogAppender::log(group, *msg);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the logging component
////////////////////////////////////////////////////////////////////////////////

void Logger::initialize(application_features::ApplicationServer& server, bool threaded) {
  MUTEX_LOCKER(locker, _initializeMutex);

  if (_active.exchange(true)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "Logger already initialized");
  }

  // logging is now active
  TRI_ASSERT(_active);
  
  if (threaded) {
    _loggingThread = std::make_unique<LogThread>(server, ::LogThreadName);
    if (!_loggingThread->start()) {
      LOG_TOPIC("28bd9", FATAL, arangodb::Logger::FIXME)
          << "could not start logging thread";
      FATAL_ERROR_EXIT();
    }
  }
  
  // generate threaded logging?
  _threaded = threaded;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the logging components
////////////////////////////////////////////////////////////////////////////////

void Logger::shutdown() {
  MUTEX_LOCKER(locker, _initializeMutex);

  if (!_active.exchange(false)) {
    // if logging not activated or already shutdown, then we can abort here
    return;
  }

  TRI_ASSERT(!_active);

  // logging is now inactive (this will terminate the logging thread)
  // join with the logging thread
  if (_threaded) {
    _threaded = false;

    char const* currentThreadName = Thread::currentThreadName();
    if (currentThreadName != nullptr && ::LogThreadName == currentThreadName) {
      // oops, the LogThread itself crashed...
      // so we need to flush the log messages here ourselves - if we waited for
      // the LogThread to flush them, we would wait forever.
      _loggingThread->processPendingMessages();
      _loggingThread->beginShutdown();
    } else {
      int tries = 0;
      while (_loggingThread->hasMessages() && ++tries < 10) {
        _loggingThread->wakeup();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      _loggingThread->beginShutdown();
      // wait until logging thread has logged all active messages
      while (_loggingThread->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  }

  // cleanup appenders
  LogAppender::shutdown();

  _cachedPid = 0;
}

void Logger::shutdownLogThread() {
  _loggingThread.reset();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to flush the logging
////////////////////////////////////////////////////////////////////////////////

void Logger::flush() noexcept {
  MUTEX_LOCKER(locker, _initializeMutex);

  if (!_active) {
    // logging not (or not yet) initialized
    return;
  }

  if (_threaded && _loggingThread != nullptr) {
    _loggingThread->flush();
  }
}
