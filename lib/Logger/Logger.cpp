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

#include "Logger.h"

#include "Basics/ArangoGlobalContext.h"
#include "Basics/Common.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/files.h"
#include "Logger/LogAppender.h"
#include "Logger/LogAppenderFile.h"
#include "Logger/LogThread.h"

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
}

Mutex Logger::_initializeMutex;

std::atomic<bool> Logger::_active(false);
std::atomic<LogLevel> Logger::_level(LogLevel::INFO);

bool Logger::_showLineNumber(false);
bool Logger::_shortenFilenames(true);
bool Logger::_showThreadIdentifier(false);
bool Logger::_showThreadName(false);
bool Logger::_threaded(false);
bool Logger::_useColor(true);
bool Logger::_useEscaped(true);
bool Logger::_useLocalTime(false);
bool Logger::_keepLogRotate(false);
bool Logger::_useMicrotime(false);
bool Logger::_logRequestParameters(true);
bool Logger::_showRole(false);
char Logger::_role('\0');
TRI_pid_t Logger::_cachedPid(0);
std::string Logger::_outputPrefix("");

std::unique_ptr<LogThread> Logger::_loggingThread(nullptr);

LogLevel Logger::logLevel() { return _level.load(std::memory_order_relaxed); }

std::vector<std::pair<std::string, LogLevel>> Logger::logLevelTopics() {
  return LogTopic::logLevelTopics();
}

void Logger::setLogLevel(LogLevel level) {
  _level.store(level, std::memory_order_relaxed);
}

void Logger::setLogLevel(std::string const& levelName) {
  std::string l = StringUtils::tolower(levelName);
  std::vector<std::string> v = StringUtils::split(l, '=');

  if (v.empty() || v.size() > 2) {
    Logger::setLogLevel(LogLevel::INFO);
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "strange log level '" << levelName
             << "', using log level 'info'";
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
    if (isGeneral) {
      Logger::setLogLevel(LogLevel::INFO);
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "strange log level '" << levelName
               << "', using log level 'info'";
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "strange log level '" << levelName << "'";
    }

    return;
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
  for (auto level : levels) {
    setLogLevel(level);
  }
}

void Logger::setRole(char role) {
  _role = role;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setOutputPrefix(std::string const& prefix) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change output prefix if logging is active");
  }

  _outputPrefix = prefix;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setShowLineNumber(bool show) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change show line number if logging is active");
  }

  _showLineNumber = show;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setShortenFilenames(bool shorten) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change shorten filenames if logging is active");
  }

  _shortenFilenames = shorten;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setShowThreadIdentifier(bool show) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change show thread identifier if logging is active");
  }

  _showThreadIdentifier = show;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setShowThreadName(bool show) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change show thread name if logging is active");
  }

  _showThreadName = show;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setUseColor(bool value) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change color if logging is active");
  }

  _useColor = value;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setUseEscaped(bool value) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change escaping if logging is active");
  }

  _useEscaped = value;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setUseLocalTime(bool show) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change use local time if logging is active");
  }

  _useLocalTime = show;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setShowRole(bool show) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change show role if logging is active");
  }

  _showRole = show;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setUseMicrotime(bool show) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change use microtime if logging is active");
  }

  _useMicrotime = show;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setKeepLogrotate(bool keep) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change keep log rotate if logging is active");
  }

  _keepLogRotate = keep;
}

// NOTE: this function should not be called if the logging is active.
void Logger::setLogRequestParameters(bool log) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change logging of request parameters if logging is active");
  }

  _logRequestParameters = log;
}

std::string const& Logger::translateLogLevel(LogLevel level) {
  switch (level) {
    case LogLevel::DEFAULT:
      return DEFAULT;
    case LogLevel::FATAL:
      return FATAL;
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
  }

  return UNKNOWN;
}

void Logger::log(char const* function, char const* file, int line,
                 LogLevel level, size_t topicId,
                 std::string const& message) {
#ifdef _WIN32
  if (level == LogLevel::FATAL || level == LogLevel::ERR) {
    if (ArangoGlobalContext::CONTEXT != nullptr && ArangoGlobalContext::CONTEXT->useEventLog()) {
      TRI_LogWindowsEventlog(function, file, line, message);
    }

    // additionally log these errors to the debug output window in MSVC so
    // we can see them during development
    OutputDebugString(message.data());
    OutputDebugString("\r\n");
  }
#endif

  if (!_active.load(std::memory_order_relaxed)) {
    LogAppenderStdStream::writeLogMessage(STDERR_FILENO, (isatty(STDERR_FILENO) == 1), level, message.data(), message.size(), true);
    return;
  }

  std::stringstream out;
  char buf[64];

  // time prefix
  if (_useMicrotime) {
    snprintf(buf, sizeof(buf), "%.6f ", TRI_microtime());
  } else {
    time_t tt = time(nullptr);
    struct tm tb;

    if (!_useLocalTime) {
      // use GMtime
      TRI_gmtime(tt, &tb);
      strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ ", &tb);
    } else {
      // use localtime
      TRI_localtime(tt, &tb);
      strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S ", &tb);
    }
  }
  out << buf;

  // output prefix
  if (!_outputPrefix.empty()) {
    out << _outputPrefix << ' ';
  }

  // append the process / thread identifier

  // we only determine our pid once, as currentProcessId() will
  // likely do a syscall.
  // this read-check-update sequence is not thread-safe, but this
  // should not matter, as the pid value is only changed from 0 to the
  // actual pid and never changes afterwards
  if (_cachedPid == 0) {
    _cachedPid = Thread::currentProcessId();
  }
  TRI_ASSERT(_cachedPid != 0);
  out << '[' << _cachedPid;

  if (_showThreadIdentifier) {
    out << '-' << Thread::currentThreadNumber();
  }

  // log thread name
  if (_showThreadName) {
    char const* threadName =  Thread::currentThreadName();
    if (threadName == nullptr) {
      threadName = "main";
    }
   
    out << '-' << threadName;
  }

  out << "] ";
  
  if (_showRole && _role != '\0') {
    out << _role << ' ';
  }

  // log level
  out << Logger::translateLogLevel(level) << ' ';

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
    out << '[' << function << "@" << filename << ':' << line << "] ";
  }

  // generate the complete message
  out << message;
  std::string ostreamContent = out.str();
  size_t offset = ostreamContent.size() - message.size();
  auto msg = std::make_unique<LogMessage>(level, topicId, std::move(ostreamContent), offset);

  // now either queue or output the message
  if (_threaded) {
    try {
      _loggingThread->log(msg);
      bool const isDirectLogLevel = (level == LogLevel::FATAL || level == LogLevel::ERR || level == LogLevel::WARN);
      if (isDirectLogLevel) {
        _loggingThread->flush();
      }
      return;
    } catch (...) {
      // fall-through to non-threaded logging
    }
  }
   
  LogAppender::log(msg.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the logging component
////////////////////////////////////////////////////////////////////////////////

void Logger::initialize(bool threaded) {
  MUTEX_LOCKER(locker, _initializeMutex);

  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "Logger already initialized");
  }

  // logging is now active
  _active.store(true);

  // generate threaded logging?
  _threaded = threaded;

  if (threaded) {
    _loggingThread = std::make_unique<LogThread>("Logging");
    _loggingThread->start();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the logging components
////////////////////////////////////////////////////////////////////////////////

void Logger::shutdown() {
  MUTEX_LOCKER(locker, _initializeMutex);

  if (!_active) {
    // if logging not activated or already shutdown, then we can abort here
    return;
  }

  _active = false;

  // logging is now inactive (this will terminate the logging thread)
  // join with the logging thread
  if (_threaded) {
    // ignore all errors for now as we cannot log them anywhere...
    int tries = 0;
    while (_loggingThread->hasMessages() && ++tries < 1000) {
      _loggingThread->wakeup();
      std::this_thread::sleep_for(std::chrono::microseconds(10000));
    }
    _loggingThread->beginShutdown();
    _loggingThread.reset();
  }

  // cleanup appenders
  LogAppender::shutdown();
  
  _cachedPid = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to flush the logging
////////////////////////////////////////////////////////////////////////////////

void Logger::flush() {
  MUTEX_LOCKER(locker, _initializeMutex);

  if (!_active) {
    // logging not (or not yet) initialized
    return;
  }

  if (_threaded) {
    LogThread::flush();
  }
}
