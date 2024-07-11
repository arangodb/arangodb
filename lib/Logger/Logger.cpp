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

#include "Logger.h"

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/operating-system.h"
#include "Basics/system-functions.h"
#include "Basics/voc-errors.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Logger/Escaper.h"
#include "Logger/LogAppender.h"
#include "Logger/LogAppenderFile.h"
#include "Logger/LogContext.h"
#include "Logger/LogGroup.h"
#include "Logger/LogMacros.h"
#include "Logger/LogStructuredParamsAllowList.h"
#include "Logger/LogThread.h"

#include <velocypack/Dumper.h>
#include <velocypack/Sink.h>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <chrono>
#include <cstring>
#include <iosfwd>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::structuredParams;

namespace {
static std::string const DEFAULT = "DEFAULT";
static std::string const FATAL = "FATAL";
static std::string const ERR = "ERROR";
static std::string const WARN = "WARNING";
static std::string const INFO = "INFO";
static std::string const DEBUG = "DEBUG";
static std::string const TRACE = "TRACE";
static std::string const UNKNOWN = "UNKNOWN";

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
    _message.append("...\n", 4);
    _shrunk = true;
  }
}

std::atomic<bool> Logger::_active(false);
std::atomic<LogLevel> Logger::_level(LogLevel::INFO);
std::function<void()> Logger::_onDroppedMessage;

// default log levels, captured once at startup. these can be used
// to reset the log levels back to defaults.
std::vector<std::pair<std::string, LogLevel>> Logger::_defaultLogLevelTopics{};

std::unordered_set<std::string> Logger::_structuredLogParams({});
arangodb::basics::ReadWriteLock Logger::_structuredParamsLock;
LogTimeFormats::TimeFormat Logger::_timeFormat(
    LogTimeFormats::TimeFormat::UTCDateString);
bool Logger::_showIds(false);
bool Logger::_showLineNumber(false);
bool Logger::_shortenFilenames(true);
bool Logger::_showProcessIdentifier(true);
bool Logger::_showThreadIdentifier(false);
bool Logger::_showThreadName(false);
bool Logger::_useColor(true);
bool Logger::_useControlEscaped(true);
bool Logger::_useUnicodeEscaped(false);
bool Logger::_keepLogRotate(false);
bool Logger::_logRequestParameters(true);
bool Logger::_showRole(false);
bool Logger::_useJson(false);
char Logger::_role('\0');
std::atomic<TRI_pid_t> Logger::_cachedPid(0);
std::string Logger::_outputPrefix;
std::string Logger::_hostname;
void (*Logger::_writerFn)(std::string_view, std::string&) =
    &Escaper<ControlCharsEscaper, UnicodeCharsRetainer>::writeIntoOutputBuffer;

std::atomic<std::size_t> Logger::_loggingThreadRefs(0);
std::atomic<LogThread*> Logger::_loggingThread(nullptr);

Logger::ThreadRef::ThreadRef() {
  // (1) - this acquire-fetch-add synchronizes with the release-fetch-add (5)
  Logger::_loggingThreadRefs.fetch_add(1, std::memory_order_acquire);
  // (2) - this acquire-load synchronizes with the release-store (4)
  _thread = Logger::_loggingThread.load(std::memory_order_acquire);
}

Logger::ThreadRef::~ThreadRef() {
  // (3) - this relaxed-fetch-add is potentially part of a release-sequence
  //       headed by (5)
  Logger::_loggingThreadRefs.fetch_sub(1, std::memory_order_relaxed);
}

LogGroup& Logger::defaultLogGroup() { return ::defaultLogGroupInstance; }

LogLevel Logger::logLevel() { return _level.load(std::memory_order_relaxed); }

std::unordered_set<std::string> Logger::structuredLogParams() {
  READ_LOCKER(guard, _structuredParamsLock);
  return _structuredLogParams;
}

std::vector<std::pair<std::string, LogLevel>> Logger::logLevelTopics() {
  return LogTopic::logLevelTopics();
}

std::vector<std::pair<std::string, LogLevel>> const&
Logger::defaultLogLevelTopics() {
  return _defaultLogLevelTopics;
}

void Logger::setShowIds(bool show) { _showIds = show; }

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
      LOG_TOPIC("05367", WARN, arangodb::Logger::FIXME)
          << "strange log level '" << levelName << "'";
      return;
    }
    level = LogLevel::INFO;
    LOG_TOPIC("d880b", WARN, arangodb::Logger::FIXME)
        << "strange log level '" << levelName << "', using log level 'info'";
  }

  if (isGeneral) {
    // set the log level globally (e.g. `--log.level info`). note that
    // this does not set the log level for all log topics, but only the
    // log level for the "general" log topic.
    Logger::setLogLevel(level);
    // setting the log level for topic "general" is required here, too,
    // as "fixme" is the previous general log topic...
    LogTopic::setLogLevel(std::string("general"), level);
  } else if (v[0] == LogTopic::ALL) {
    // handle pseudo log-topic "all": this will set the log level for
    // all existing topics
    for (auto const& it : logLevelTopics()) {
      LogTopic::setLogLevel(it.first, level);
    }
  } else {
    // handle a topic-specific request (e.g. `--log.level requests=info`).
    LogTopic::setLogLevel(v[0], level);
  }
}

void Logger::setLogStructuredParams(
    std::unordered_map<std::string, bool> const& paramsAndValues) {
  WRITE_LOCKER(guard, Logger::_structuredParamsLock);
  for (const auto& [paramName, value] : paramsAndValues) {
    if (auto it = allowList.find(std::string_view{paramName});
        it == allowList.end()) {
      continue;
    }
    if (value) {
      _structuredLogParams.emplace(paramName);
    } else {
      _structuredLogParams.erase(paramName);
    }
  }
}

void Logger::setLogLevel(std::vector<std::string> const& levels) {
  for (auto const& level : levels) {
    setLogLevel(level);
  }
}

std::unordered_map<std::string, bool> Logger::parseStringParams(
    std::vector<std::string> const& params) {
  std::unordered_map<std::string, bool> validParams;
  for (auto const& param : params) {
    std::string l = StringUtils::tolower(param);
    std::vector<std::string> v = StringUtils::split(l, '=');
    size_t vSize = v.size();
    if (!vSize || vSize > 2) {
      LOG_TOPIC("4d971", WARN, arangodb::Logger::FIXME)
          << "strange log attribute and value set '" + param + "'";
    } else {
      StringUtils::trimInPlace(v[0]);
      if (vSize == 2) {
        StringUtils::trimInPlace(v[1]);
      }
      if (vSize == 1 || v[1] == "true") {
        validParams[v[0]] = true;
      } else {
        if (v[1] == "false") {
          validParams[v[0]] = false;
        } else {
          LOG_TOPIC("5d210", WARN, arangodb::Logger::FIXME)
              << "strange value '" + v[1] + "'";
        }
      }
    }
  }
  return validParams;
}

void Logger::setLogStructuredParamsOnServerStart(
    std::vector<std::string> const& params) {
  setLogStructuredParams(parseStringParams(params));
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
void Logger::setUseControlEscaped(bool value) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }
  _useControlEscaped = value;
}
// NOTE: this function should not be called if the logging is active.
void Logger::setUseUnicodeEscaped(bool value) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }
  _useUnicodeEscaped = value;
}

// alerts that startup parameters for escaping have already been assigned
void Logger::setEscaping() {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot change settings once logging is active");
  }

  if (_useControlEscaped) {
    if (_useUnicodeEscaped) {
      _writerFn = &Escaper<ControlCharsEscaper,
                           UnicodeCharsEscaper>::writeIntoOutputBuffer;
    } else {
      _writerFn = &Escaper<ControlCharsEscaper,
                           UnicodeCharsRetainer>::writeIntoOutputBuffer;
    }
  } else {
    if (_useUnicodeEscaped) {
      _writerFn = &Escaper<ControlCharsSuppressor,
                           UnicodeCharsEscaper>::writeIntoOutputBuffer;
    } else {
      _writerFn = &Escaper<ControlCharsSuppressor,
                           UnicodeCharsRetainer>::writeIntoOutputBuffer;
    }
  }
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

bool Logger::translateLogLevel(std::string const& l, bool isGeneral,
                               LogLevel& level) noexcept {
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

void Logger::log(char const* logid, char const* function, char const* file,
                 int line, LogLevel level, size_t topicId,
                 std::string_view message) try {
  TRI_ASSERT(logid != nullptr);
  LogContext& logContext = LogContext::current();

  // we only determine our pid once, as currentProcessId() will
  // likely do a syscall.
  // this read-check-update sequence is not thread-safe, but this
  // should not matter, as the pid value is only changed from 0 to the
  // actual pid and never changes afterwards
  if (_cachedPid.load(std::memory_order_relaxed) == 0) {
    _cachedPid.store(Thread::currentProcessId(), std::memory_order_relaxed);
  }

  std::string out;
  out.reserve(256 + message.size());

  uint32_t offset = 0;
  bool shrunk = false;

  if (Logger::_useJson) {
    // construct JSON output
    arangodb::velocypack::StringSink sink(&out);
    arangodb::velocypack::Options options;
    options.escapeControl = _useControlEscaped;
    options.escapeUnicode = _useUnicodeEscaped;
    arangodb::velocypack::Dumper dumper(&sink, &options);

    out.push_back('{');

    // current date/time
    {
      out.append("\"time\":");
      if (LogTimeFormats::isStringFormat(_timeFormat)) {
        out.push_back('"');
      }
      // value of date/time is always safe to print
      LogTimeFormats::writeTime(out, _timeFormat,
                                std::chrono::system_clock::now());
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
      StringUtils::itoa(uint64_t(_cachedPid.load(std::memory_order_relaxed)),
                        out);
    }

    // tid
    if (_showThreadIdentifier) {
      out.append(",\"tid\":");
      StringUtils::itoa(uint64_t(Thread::currentThreadNumber()), out);
    }

    // thread name
    if (_showThreadName) {
      ThreadNameFetcher nameFetcher;
      std::string_view threadName = nameFetcher.get();
      out.append(",\"thread\":");
      dumper.appendString(threadName.data(), threadName.size());
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
      if (_shortenFilenames) {
        char const* shortened = strrchr(filename, TRI_DIR_SEPARATOR_CHAR);
        if (shortened != nullptr) {
          filename = shortened + 1;
        }
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

    {
      READ_LOCKER(guard, _structuredParamsLock);
      // meta data from log context
      LogContext::OverloadVisitor visitor([&out, &dumper](
                                              std::string_view const& key,
                                              auto&& value) {
        if (!_structuredLogParams.contains({key.data(), key.size()})) {
          return;
        }
        out.push_back(',');
        dumper.appendString(key.data(), key.size());
        out.push_back(':');
        if constexpr (std::is_same_v<std::string_view,
                                     std::remove_cv_t<std::remove_reference_t<
                                         decltype(value)>>>) {
          dumper.appendString(value.data(), value.size());
        } else {
          out.append(std::to_string(value));
        }
      });
      logContext.visit(visitor);
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
      dumper.appendString(message.data(), maxMessageLength);

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
    LogTimeFormats::writeTime(out, _timeFormat,
                              std::chrono::system_clock::now());
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
      TRI_ASSERT(_cachedPid.load(std::memory_order_relaxed) != 0);
      out.push_back('[');
      StringUtils::itoa(uint64_t(_cachedPid.load(std::memory_order_relaxed)),
                        out);
      haveProcessOutput = true;
    }

    if (_showThreadIdentifier) {
      out.push_back(haveProcessOutput ? '-' : '[');
      StringUtils::itoa(uint64_t(Thread::currentThreadNumber()), out);
      haveProcessOutput = true;
    }

    // log thread name
    if (_showThreadName) {
      ThreadNameFetcher nameFetcher;
      std::string_view threadName = nameFetcher.get();

      out.push_back(haveProcessOutput ? '-' : '[');
      out.append(threadName.data(), threadName.size());
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

    // the offset is used by the in-memory logger, and it cuts off everything
    // from the start of the concatenated log string until the offset. only
    // what's after the offset gets displayed in the web interface
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

    {
      READ_LOCKER(guard, _structuredParamsLock);
      //  meta data from log
      LogContext::OverloadVisitor visitor([&out, writerFn = _writerFn](
                                              std::string_view const& key,
                                              auto&& value) {
        if (!_structuredLogParams.contains({key.data(), key.size()})) {
          return;
        }
        out.push_back('[');
        out.append(key).append(": ", 2);

        if constexpr (std::is_same_v<std::string_view,
                                     std::remove_cv_t<std::remove_reference_t<
                                         decltype(value)>>>) {
          writerFn(value, out);
        } else {
          std::string number = std::to_string(value);
          writerFn(number, out);
        }

        out.append("] ", 2);
      });
      logContext.visit(visitor);
    }

    _writerFn(message, out);
  }

  TRI_ASSERT(offset == 0 || !_useJson);

  // append final newline
  out.push_back('\n');

  auto msg = std::make_unique<LogMessage>(function, file, line, level, topicId,
                                          std::move(out), offset, shrunk);

  append(defaultLogGroup(), std::move(msg), false,
         [level, topicId](LogMessage const& msg) -> void {
           LogAppenderStdStream::writeLogMessage(STDERR_FILENO,
                                                 (isatty(STDERR_FILENO) == 1),
                                                 level, topicId, msg._message);
         });
} catch (...) {
  // logging itself must never cause an exeption to escape
}

void Logger::append(LogGroup& group, std::unique_ptr<LogMessage> msg,
                    bool forceDirect,
                    std::function<void(LogMessage const&)> const& inactive) {
  TRI_ASSERT(msg != nullptr);

  // check if we need to shrink the message here
  if (!msg->shrunk()) {
    msg->shrink(group.maxLogEntryLength());
  }

  // first log to all "global" appenders, which are the in-memory ring buffer
  // logger plus some Windows-specifc appenders for the debug output window
  // and the Windows event log. note that these loggers do not require any
  // configuration so we can always and safely invoke them.
  LogAppender::logGlobal(group, *msg);

  if (!_active.load(std::memory_order_acquire)) {
    // logging is still turned off. now use hard-coded to-stderr logging
    inactive(*msg);
  } else {
    // now either queue or output the message
    bool handled = false;
    if (!forceDirect) {
      // check if we have a logging thread
      ThreadRef loggingThread;

      if (loggingThread) {
        handled = loggingThread->log(group, msg);
      }
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

void Logger::initialize(bool threaded, uint32_t maxQueuedLogMessages) {
  if (_active.exchange(true, std::memory_order_acquire)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "Logger already initialized");
  }

  _defaultLogLevelTopics = logLevelTopics();

  // logging is now active
  if (threaded) {
    auto loggingThread = std::make_unique<LogThread>(std::string(logThreadName),
                                                     maxQueuedLogMessages);
    if (!loggingThread->start()) {
      LOG_TOPIC("28bd9", FATAL, arangodb::Logger::FIXME)
          << "could not start logging thread";
      FATAL_ERROR_EXIT();
    }

    // (4) - this release-store synchronizes with the acquire-load (2)
    _loggingThread.store(loggingThread.release(), std::memory_order_release);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the logging components
////////////////////////////////////////////////////////////////////////////////

void Logger::shutdown() {
  if (!_active.exchange(false, std::memory_order_acquire)) {
    // if logging not activated or already shut down, then we can abort here
    return;
  }
  // logging is now inactive

  // reset the instance variable in Logger, so that others won't see it
  // anymore
  std::unique_ptr<LogThread> loggingThread(
      _loggingThread.exchange(nullptr, std::memory_order_relaxed));

  // logging is now inactive (this will terminate the logging thread)
  // join with the logging thread
  if (loggingThread != nullptr) {
    // (5) - this release-fetch-add synchronizes with the acquire-fetch-add
    // (1) Even though a fetch-add with 0 is essentially a noop, this is
    // necessary to ensure that threads which try to get a reference to the
    // _loggingThread actually see the new nullptr value.
    _loggingThreadRefs.fetch_add(0, std::memory_order_release);

    // wait until all threads have dropped their reference to the logging
    // thread
    while (_loggingThreadRefs.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    ThreadNameFetcher nameFetcher;
    std::string_view currentThreadName = nameFetcher.get();
    if (logThreadName == currentThreadName) {
      // oops, the LogThread itself crashed...
      // so we need to flush the log messages here ourselves - if we waited
      // for the LogThread to flush them, we would wait forever.
      loggingThread->processPendingMessages();
      loggingThread->beginShutdown();
    } else {
      int tries = 0;
      while (loggingThread->hasMessages() && ++tries < 10) {
        loggingThread->wakeup();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      loggingThread->beginShutdown();
      // wait until logging thread has logged all active messages
      while (loggingThread->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  }

  // cleanup appenders
  LogAppender::shutdown();

  _cachedPid.store(0, std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to flush the logging
////////////////////////////////////////////////////////////////////////////////

void Logger::flush() noexcept {
  if (!_active.load(std::memory_order_acquire)) {
    // logging not (or not yet) initialized
    return;
  }

  ThreadRef loggingThread;
  if (loggingThread) {
    loggingThread->flush();
  }
}

void Logger::setOnDroppedMessage(std::function<void()> cb) {
  _onDroppedMessage = std::move(cb);
}

void Logger::onDroppedMessage() noexcept {
  if (_onDroppedMessage) {
    _onDroppedMessage();
  }
}
