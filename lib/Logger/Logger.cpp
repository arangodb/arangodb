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
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/files.h"
#include "Logger/LogAppender.h"

using namespace arangodb;
using namespace arangodb::basics;

Mutex Logger::_initializeMutex;

std::atomic<bool> Logger::_active(false);
std::atomic<LogLevel> Logger::_level(LogLevel::INFO);

bool Logger::_showLineNumber(false);
bool Logger::_shortenFilenames(true);
bool Logger::_showThreadIdentifier(false);
bool Logger::_threaded(false);
bool Logger::_useLocalTime(false);
bool Logger::_keepLogRotate(false);
bool Logger::_useMicrotime(false);
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

  bool isGeneral = v.size() == 1;

  if (!isGeneral) {
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
void Logger::setUseLocalTime(bool show) {
  if (_active) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot change use local time if logging is active");
  }

  _useLocalTime = show;
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

std::string const& Logger::translateLogLevel(LogLevel level) {
  static std::string DEFAULT = "DEFAULT";
  static std::string FATAL = "FATAL";
  static std::string ERR = "ERROR";
  static std::string WARN = "WARNING";
  static std::string INFO = "INFO";
  static std::string DEBUG = "DEBUG";
  static std::string TRACE = "TRACE";
  static std::string UNKNOWN = "UNKNOWN";

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

void Logger::log(char const* function, char const* file, long int line,
                 LogLevel level, size_t topicId,
                 std::string const& message) {
#ifdef _WIN32
  if (level == LogLevel::FATAL || level == LogLevel::ERR) {
    if (ArangoGlobalContext::CONTEXT != nullptr && ArangoGlobalContext::CONTEXT->useEventLog()) {
      TRI_LogWindowsEventlog(function, file, line, message);
    }

    // additionally log these errors to the debug output window in MSVC so
    // we can see them during development
    OutputDebugString(message.c_str());
    OutputDebugString("\r\n");
  }
#endif

  if (!_active.load(std::memory_order_relaxed)) {
    LogAppender::writeStderr(level, message);
    return;
  }

  std::stringstream out;

  // time prefix
  if (_useMicrotime) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%.6f ", TRI_microtime());
    out << buf;
  } else {
    char timePrefix[32];
    time_t tt = time(0);
    struct tm tb;

    if (!_useLocalTime) {
      // use GMtime
      TRI_gmtime(tt, &tb);
      strftime(timePrefix, sizeof(timePrefix), "%Y-%m-%dT%H:%M:%SZ ", &tb);
    } else {
      // use localtime
      TRI_localtime(tt, &tb);
      strftime(timePrefix, sizeof(timePrefix), "%Y-%m-%dT%H:%M:%S ", &tb);
    }

    out << timePrefix;
  }

  // output prefix
  if (!_outputPrefix.empty()) {
    out << _outputPrefix << " ";
  }

  // append the process / thread identifier
  {
    char processPrefix[48];

    TRI_pid_t processId = Thread::currentProcessId();

    if (_showThreadIdentifier) {
      uint64_t threadNumber = Thread::currentThreadNumber();
      snprintf(processPrefix, sizeof(processPrefix), "[%llu-%llu] ",
               (unsigned long long)processId, (unsigned long long)threadNumber);
    } else {
      snprintf(processPrefix, sizeof(processPrefix), "[%llu] ",
               (unsigned long long)processId);
    }

    out << processPrefix;
  }

  // log level
  out << Logger::translateLogLevel(level) << ' ';

  // check if we must display the line number
  if (_showLineNumber) {
    char const* filename = file;

    if (_shortenFilenames) {
      // shorten file names from `/home/.../file.cpp` to just `file.cpp`
      char const* shortened = strrchr(filename, TRI_DIR_SEPARATOR_CHAR);
      if (shortened != nullptr) {
        filename = shortened + 1;
      }
    }
    out << '[' << filename << ':' << line << "] ";
  }

  // generate the complete message
  out << message;
  size_t offset = out.str().size() - message.size();
  auto msg = std::make_unique<LogMessage>(level, topicId, out.str(), offset);

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
      usleep(10000);
    }
    _loggingThread->beginShutdown();
    _loggingThread.reset();
  }

  // cleanup appenders
  LogAppender::shutdown();
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
