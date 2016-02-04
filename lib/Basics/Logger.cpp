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

#include <iostream>
#include <iomanip>

#ifdef TRI_ENABLE_SYSLOG
// we need to define SYSLOG_NAMES for linux to get a list of names
#define SYSLOG_NAMES
#define prioritynames TRI_prioritynames
#define facilitynames TRI_facilitynames
#include <syslog.h>
#endif

#include <boost/lockfree/queue.hpp>

#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/files.h"
#include "Basics/shell-colors.h"
#include "Basics/tri-strings.h"

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief output prefix
////////////////////////////////////////////////////////////////////////////////

static std::atomic<char*> OutputPrefix(nullptr);

////////////////////////////////////////////////////////////////////////////////
/// @brief show line numbers, debug and trace always show the line numbers
////////////////////////////////////////////////////////////////////////////////

static std::atomic<bool> ShowLineNumber(false);

////////////////////////////////////////////////////////////////////////////////
/// @brief show thread identifier
////////////////////////////////////////////////////////////////////////////////

static std::atomic<bool> ShowThreadIdentifier(false);

////////////////////////////////////////////////////////////////////////////////
/// @brief use local time for dates & times in log output
////////////////////////////////////////////////////////////////////////////////

static std::atomic<bool> UseLocalTime(false);

////////////////////////////////////////////////////////////////////////////////
/// @brief true, if a separated thread is used
////////////////////////////////////////////////////////////////////////////////

static std::atomic<bool> ThreadedLogging(false);

////////////////////////////////////////////////////////////////////////////////
/// @brief logging active
////////////////////////////////////////////////////////////////////////////////

static std::atomic<bool> LoggingActive(false);

////////////////////////////////////////////////////////////////////////////////
/// @brief log topics
///
/// Note: they need to be created as static variables in Logger
////////////////////////////////////////////////////////////////////////////////

static Mutex LogTopicNamesLock;
static std::map<std::string, LogTopic*> LogTopicNames;

////////////////////////////////////////////////////////////////////////////////
/// @brief message container
////////////////////////////////////////////////////////////////////////////////

struct LogMessage {
  LogMessage(LogMessage const&) = delete;
  LogMessage& operator=(LogMessage const&) = delete;

  LogMessage(LogLevel level, size_t topicId, std::string&& message,
             size_t offset)
      : _level(level),
        _topicId(topicId),
        _message(std::move(message)),
        _offset(offset) {}

  LogLevel _level;
  size_t _topicId;
  std::string const _message;
  size_t _offset;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief LogAppender
////////////////////////////////////////////////////////////////////////////////

class LogAppender {
 public:
  LogAppender(std::string const& filter) : _filter(filter) {}
  virtual ~LogAppender() {}

  virtual void logMessage(LogLevel, std::string const& message,
                          size_t offset) = 0;

  virtual void reopenLog() = 0;
  virtual void closeLog() = 0;

  virtual std::string details() = 0;

  bool checkContent(std::string const& message) {
    return _filter.empty() ||
           TRI_IsContainedString(message.c_str(), _filter.c_str());
  }

 protected:
  std::string const _filter;  // an optional content filter for log messages
};

////////////////////////////////////////////////////////////////////////////////
/// @brief list of active LogAppender
////////////////////////////////////////////////////////////////////////////////

static arangodb::Mutex AppendersLock;
static std::map<size_t, std::vector<std::unique_ptr<LogAppender>>> Appenders;

////////////////////////////////////////////////////////////////////////////////
/// @brief LogAppenderFile
////////////////////////////////////////////////////////////////////////////////

static void WriteStderr(LogLevel level, std::string const& msg) {
  if (level == LogLevel::FATAL || level == LogLevel::ERR) {
    fprintf(stderr, TRI_SHELL_COLOR_RED "%s" TRI_SHELL_COLOR_RESET "\n",
            msg.c_str());
  } else if (level == LogLevel::WARN) {
    fprintf(stderr, TRI_SHELL_COLOR_YELLOW "%s" TRI_SHELL_COLOR_RESET "\n",
            msg.c_str());
  } else {
    fprintf(stderr, "%s\n", msg.c_str());
  }
}

class LogAppenderFile : public LogAppender {
 public:
  LogAppenderFile(std::string const& filename, bool fatal2stderr,
                  std::string const& filter);
  ~LogAppenderFile() { closeLog(); };

  void logMessage(LogLevel, std::string const& message,
                  size_t offset) override final;

  void reopenLog() override final;
  void closeLog() override final;

  std::string details() override final;

 private:
  void writeLogFile(int, char const*, ssize_t);

 private:
  std::string _filename;
  bool _fatal2stderr;
  std::atomic<int> _fd;
};

LogAppenderFile::LogAppenderFile(std::string const& filename, bool fatal2stderr,
                                 std::string const& filter)
    : LogAppender(filter),
      _filename(filename),
      _fatal2stderr(fatal2stderr),
      _fd(-1) {
  // logging to stdout
  if (_filename == "+") {
    _fd.store(STDOUT_FILENO);
  }

  // logging to stderr
  else if (_filename == "-") {
    _fd.store(STDERR_FILENO);
  }

  // logging to file
  else {
    int fd = TRI_CREATE(filename.c_str(),
                        O_APPEND | O_CREAT | O_WRONLY | TRI_O_CLOEXEC,
                        S_IRUSR | S_IWUSR | S_IRGRP);

    if (fd < 0) {
      std::cerr << "cannot write to file '" << filename << "'" << std::endl;
      _filename = "+";
      _fd.store(STDERR_FILENO);
    } else {
      _fd.store(fd);
    }
  }
}

void LogAppenderFile::logMessage(LogLevel level, std::string const& message,
                                 size_t offset) {
  int fd = _fd.load();

  if (fd < 0) {
    return;
  }

  if (level == LogLevel::FATAL && _fatal2stderr) {
    MUTEX_LOCKER(guard, AppendersLock);

    // a fatal error. always print this on stderr, too.
    WriteStderr(level, message);

    // this function is already called when the appenders lock is held
    // no need to lock it again
    for (auto const& it : Appenders[MAX_LOG_TOPICS]) {
      std::string details = it->details();

      if (!details.empty()) {
        WriteStderr(LogLevel::INFO, details);
      }
    }

    if (_filename.empty() && (fd == STDOUT_FILENO || fd == STDERR_FILENO)) {
      // the logfile is either stdout or stderr. no need to print the message
      // again
      return;
    }
  }

  size_t escapedLength;
  char* escaped =
      TRI_EscapeControlsCString(TRI_UNKNOWN_MEM_ZONE, message.c_str(),
                                message.size(), &escapedLength, true);

  if (escaped != nullptr) {
    writeLogFile(fd, escaped, (ssize_t)escapedLength);
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, escaped);
  }
}

void LogAppenderFile::reopenLog() {
  if (_filename.empty()) {
    return;
  }

  if (_fd <= STDERR_FILENO) {
    return;
  }

  // rename log file
  std::string backup(_filename);
  backup.append(".old");

  TRI_UnlinkFile(backup.c_str());
  TRI_RenameFile(_filename.c_str(), backup.c_str());

  // open new log file
  int fd = TRI_CREATE(_filename.c_str(),
                      O_APPEND | O_CREAT | O_WRONLY | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR | S_IRGRP);

  if (fd < 0) {
    TRI_RenameFile(backup.c_str(), _filename.c_str());
    return;
  }

  int old = std::atomic_exchange(&_fd, fd);

  TRI_CLOSE(old);
}

void LogAppenderFile::closeLog() {
  int fd = _fd.exchange(-1);

  if (fd > STDERR_FILENO) {
    TRI_CLOSE(fd);
  }
}

std::string LogAppenderFile::details() {
  if (_filename.empty()) {
    return "";
  }

  int fd = _fd.load();

  if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
    std::string buffer("More error details may be provided in the logfile '");
    buffer.append(_filename);
    buffer.append("'");

    return buffer;
  }

  return "";
}

void LogAppenderFile::writeLogFile(int fd, char const* buffer, ssize_t len) {
  bool giveUp = false;

  while (len > 0) {
    ssize_t n;

    n = TRI_WRITE(fd, buffer, len);

    if (n < 0) {
      fprintf(stderr, "cannot log data: %s\n", TRI_LAST_ERROR_STR);
      return;  // give up, but do not try to log failure
    } else if (n == 0) {
      if (!giveUp) {
        giveUp = true;
        continue;
      }
    }

    buffer += n;
    len -= n;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief LogAppenderSyslog
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

class LogAppenderSyslog : public LogAppender {
 public:
  LogAppenderSyslog(std::string const& facility, std::string const& name,
                    std::string const& filter);
  ~LogAppenderSyslog() { closeLog(); };

  void logMessage(LogLevel, std::string const& message,
                  size_t offset) override final;

  void reopenLog() override final;
  void closeLog() override final;

  std::string details() override final;

 private:
  arangodb::Mutex _lock;
  bool _opened;
};

LogAppenderSyslog::LogAppenderSyslog(std::string const& facility,
                                     std::string const& name,
                                     std::string const& filter)
    : LogAppender(filter), _opened(false) {
  // no logging
  std::string sysname = name.empty() ? "[arangod]" : name;

  // find facility
  int value = LOG_LOCAL0;

  if ('0' <= facility[0] && facility[0] <= '9') {
    value = StringUtils::int32(facility);
  } else {
    CODE* ptr = reinterpret_cast<CODE*>(TRI_facilitynames);

    while (ptr->c_name != 0) {
      if (strcmp(ptr->c_name, facility.c_str()) == 0) {
        value = ptr->c_val;
        break;
      }

      ++ptr;
    }
  }

  // and open logging, openlog does not have a return value...
  {
    MUTEX_LOCKER(guard, _lock);
    ::openlog(sysname.c_str(), LOG_CONS | LOG_PID, value);
    _opened = true;
  }
}

void LogAppenderSyslog::logMessage(LogLevel level, std::string const& message,
                                   size_t offset) {
  int priority = LOG_ERR;

  switch (level) {
    case LogLevel::FATAL:
      priority = LOG_CRIT;
      break;
    case LogLevel::ERR:
      priority = LOG_ERR;
      break;
    case LogLevel::WARN:
      priority = LOG_WARNING;
      break;
    case LogLevel::DEFAULT:
    case LogLevel::INFO:
      priority = LOG_NOTICE;
      break;
    case LogLevel::DEBUG:
      priority = LOG_INFO;
      break;
    case LogLevel::TRACE:
      priority = LOG_DEBUG;
      break;
  }

  {
    MUTEX_LOCKER(mutexLocker, _lock);
    if (_opened) {
      ::syslog(priority, "%s", message.c_str() + offset);
    }
  }
}

void LogAppenderSyslog::reopenLog() {}

void LogAppenderSyslog::closeLog() {
  MUTEX_LOCKER(mutexLocker, _lock);

  if (_opened) {
    _opened = false;
    ::closelog();
  }
}

std::string LogAppenderSyslog::details() {
  return "More error details may be provided in the syslog";
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief RingBuffer
////////////////////////////////////////////////////////////////////////////////

#define RING_BUFFER_SIZE (10240)

static Mutex RingBufferLock;
static uint64_t RingBufferId = 0;
static LogBuffer RingBuffer[RING_BUFFER_SIZE];

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a message in a ring buffer
///
/// We ignore any race conditions here.
////////////////////////////////////////////////////////////////////////////////

static void StoreMessage(LogLevel level, std::string const& message,
                         size_t offset) {
  MUTEX_LOCKER(guard, RingBufferLock);

  uint64_t n = RingBufferId++;
  LogBuffer* ptr = &RingBuffer[n % RING_BUFFER_SIZE];

  ptr->_id = n;
  ptr->_level = level;
  ptr->_timestamp = time(0);
  TRI_CopyString(ptr->_message, message.c_str() + offset,
                 sizeof(ptr->_message) - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief OutputMessage
////////////////////////////////////////////////////////////////////////////////

static void OutputMessage(LogLevel level, size_t topicId,
                          std::string const& message, size_t offset) {
  MUTEX_LOCKER(guard, AppendersLock);

  // store message for frontend
  if (message.size() > offset) {
    StoreMessage(level, message, offset);
  }

  // output to appender
  auto output = [&level, &message, &offset](size_t n) -> bool {
    auto const& it = Appenders.find(n);
    bool shown = false;

    if (it != Appenders.end()) {
      auto const& appenders = it->second;

      for (auto const& appender : appenders) {
        if (appender->checkContent(message)) {
          appender->logMessage(level, message, offset);
        }

        shown = true;
      }
    }

    return shown;
  };

  bool shown = false;

  // try to find a specific topic
  if (topicId < MAX_LOG_TOPICS) {
    shown = output(topicId);
  }

  // otherwise use the general topic
  if (!shown) {
    shown = output(MAX_LOG_TOPICS);
  }

  if (!shown) {
    WriteStderr(level, message);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief MessageQueue
////////////////////////////////////////////////////////////////////////////////

static boost::lockfree::queue<LogMessage*> MessageQueue(0);

////////////////////////////////////////////////////////////////////////////////
/// @brief QueueMessage
////////////////////////////////////////////////////////////////////////////////

static void QueueMessage(char const* function, char const* file, long int line,
                         LogLevel level, size_t topicId,
                         std::string const& message) {
#ifdef _WIN32
  if (level == LogLevel::FATAL || level == LogLevel::ERROR) {
    va_list wva;
    va_copy(wva, ap);
    TRI_LogWindowsEventlog(func, file, line, "%s", message.c_str());
    va_end(wva);
  }
#endif

  if (!LoggingActive.load(std::memory_order_relaxed)) {
    WriteStderr(level, message);
    return;
  }

  std::stringstream out;

  TRI_pid_t processId = TRI_CurrentProcessId();
  uint64_t threadNumber = Thread::currentThreadNumber();

  // time prefix
  {
    char timePrefix[32];
    time_t tt = time(0);
    struct tm tb;

    if (!UseLocalTime.load(std::memory_order_relaxed)) {
      // use GMtime
      TRI_gmtime(tt, &tb);
      // write time in buffer
      strftime(timePrefix, sizeof(timePrefix), "%Y-%m-%dT%H:%M:%SZ ", &tb);
    } else {
      // use localtime
      TRI_localtime(tt, &tb);
      strftime(timePrefix, sizeof(timePrefix), "%Y-%m-%dT%H:%M:%S ", &tb);
    }

    out << timePrefix;
  }

  // output prefix
  char const* outputPrefix = OutputPrefix.load(std::memory_order_relaxed);

  if (outputPrefix != nullptr && *outputPrefix) {
    out << outputPrefix << " ";
  }

  // append the process / thread identifier
  {
    char processPrefix[128];

    if (ShowThreadIdentifier.load(std::memory_order_relaxed)) {
      snprintf(processPrefix, sizeof(processPrefix), "[%llu-%llu] ",
               (unsigned long long)processId, (unsigned long long)threadNumber);
    } else {
      snprintf(processPrefix, sizeof(processPrefix), "[%llu] ",
               (unsigned long long)processId);
    }

    out << processPrefix;
  }

  // log level
  out << Logger::translateLogLevel(level) << " ";

  // check if we must display the line number
  bool sln = ShowLineNumber.load(std::memory_order_relaxed);

  // append the file and line
  if (sln) {
    out << "[" << file << ":" << line << "] ";
  }

  // generate the complete message
  size_t offset = out.str().size();
  out << message;
  std::string const& m = out.str();

  // now either queue or output the message
  if (ThreadedLogging.load(std::memory_order_relaxed)) {
    auto msg = std::make_unique<LogMessage>(level, topicId,
                                            std::move(out.str()), offset);

    try {
      MessageQueue.push(msg.get());
      msg.release();
    } catch (...) {
      OutputMessage(level, topicId, m, offset);
    }
  } else {
    OutputMessage(level, topicId, m, offset);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief LoggerThread
////////////////////////////////////////////////////////////////////////////////

class LogThread : public Thread {
 public:
  LogThread(std::string const& name) : Thread(name) {}

 public:
  void run();
};

void LogThread::run() {
  LogMessage* msg;

  while (LoggingActive.load()) {
    while (MessageQueue.pop(msg)) {
      try {
        OutputMessage(msg->_level, msg->_topicId, msg->_message, msg->_offset);
      } catch (...) {
      }

      delete msg;
    }

    usleep(100 * 1000);
  }

  while (MessageQueue.pop(msg)) {
    delete msg;
  }
}
////////////////////////////////////////////////////////////////////////////////
/// @brief LogTopic
////////////////////////////////////////////////////////////////////////////////

namespace {
std::atomic_uint_fast16_t NEXT_TOPIC_ID(0);
}

LogTopic::LogTopic(std::string const& name)
    : LogTopic(name, LogLevel::DEFAULT) {}

LogTopic::LogTopic(std::string const& name, LogLevel level)
    : _id(NEXT_TOPIC_ID.fetch_add(1, std::memory_order_seq_cst)),
      _name(name),
      _level(level) {
  try {
    MUTEX_LOCKER(guard, LogTopicNamesLock);
    LogTopicNames[name] = this;
  } catch (...) {
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief LogStream
////////////////////////////////////////////////////////////////////////////////

LoggerStream::~LoggerStream() {
  try {
    QueueMessage(_function, _file, _line, _level, _topicId, _out.str());
  } catch (...) {
    std::cerr << "failed to log: " << _out.str() << std::endl;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief predefined topics
////////////////////////////////////////////////////////////////////////////////

LogTopic Logger::COLLECTOR("collector");
LogTopic Logger::COMPACTOR("compactor");
LogTopic Logger::MMAP("mmap");
LogTopic Logger::PERFORMANCE("performance",
                             LogLevel::FATAL);  // suppress by default
LogTopic Logger::QUERIES("queries", LogLevel::INFO);
LogTopic Logger::REQUESTS("requests", LogLevel::FATAL);  // suppress by default

////////////////////////////////////////////////////////////////////////////////
/// @brief current log level
////////////////////////////////////////////////////////////////////////////////

std::atomic<LogLevel> Logger::_level(LogLevel::INFO);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new appender
///
/// fatal2stderr and filter will only be used for general (non-topic) log
/// messages.
////////////////////////////////////////////////////////////////////////////////

void Logger::addAppender(std::string const& definition, bool fatal2stderr,
                         std::string const& filter) {
  std::vector<std::string> v = StringUtils::split(definition, '=');
  std::string topicName;
  std::string output;
  std::string contentFilter;
  bool f2s = false;

  if (v.size() == 1) {
    output = v[0];
    contentFilter = filter;
    f2s = fatal2stderr;
  } else if (v.size() == 2) {
    topicName = StringUtils::tolower(v[0]);

    if (topicName.empty()) {
      output = v[0];
      contentFilter = filter;
      f2s = fatal2stderr;
    } else {
      output = v[1];
    }
  } else {
    LOG(ERR) << "strange output definition '" << definition << "' ignored";
    return;
  }

  LogTopic* topic = nullptr;

  if (!topicName.empty()) {
    MUTEX_LOCKER(guard, LogTopicNamesLock);

    auto it = LogTopicNames.find(topicName);

    if (it == LogTopicNames.end()) {
      LOG(ERR) << "strange topic '" << topicName
               << "', ignoring whole defintion";
      return;
    }

    topic = it->second;
  }

  std::unique_ptr<LogAppender> appender;

  if (output == "-") {
    appender.reset(new LogAppenderFile("-", f2s, contentFilter));
  } else if (output == "+") {
    appender.reset(new LogAppenderFile("+", f2s, contentFilter));
  } else if (StringUtils::isPrefix(output, "file://")) {
    auto filename = output.substr(7);

    appender.reset(new LogAppenderFile(filename, f2s, contentFilter));
#ifdef TRI_ENABLE_SYSLOG
  } else if (StringUtils::isPrefix(output, "syslog://")) {
    auto s = StringUtils::split(output.substr(9), '/');

    if (s.size() < 1 || s.size() > 2) {
      LOG(ERR) << "unknown syslog definition '" << output << "', expecting "
               << "'syslog://facility/identifier'";
      return;
    }

    if (s.size() == 1) {
      appender.reset(new LogAppenderSyslog(s[0], "", contentFilter));
    } else {
      appender.reset(new LogAppenderSyslog(s[0], s[1], contentFilter));
    }
#endif
  } else {
    LOG(ERR) << "unknown output '" << output << "'";
    return;
  }

  size_t n = topic == nullptr ? MAX_LOG_TOPICS : topic->id();

  MUTEX_LOCKER(guard, AppendersLock);
  Appenders[n].emplace_back(appender.release());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines the global log level
////////////////////////////////////////////////////////////////////////////////

LogLevel Logger::logLevel() { return _level.load(std::memory_order_relaxed); }

////////////////////////////////////////////////////////////////////////////////
/// @brief determines the global log level
////////////////////////////////////////////////////////////////////////////////

std::vector<std::pair<std::string, LogLevel>> Logger::logLevelTopics() {
  MUTEX_LOCKER(guard, LogTopicNamesLock);

  std::vector<std::pair<std::string, LogLevel>> levels;

  for (auto topic : LogTopicNames) {
    levels.emplace_back(make_pair(topic.first, topic.second->level()));
  }

  return levels;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the global log level
////////////////////////////////////////////////////////////////////////////////

void Logger::setLogLevel(LogLevel level) {
  _level.store(level, std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the log level from a string
////////////////////////////////////////////////////////////////////////////////

void Logger::setLogLevel(std::string const& levelName) {
  std::string l = StringUtils::tolower(levelName);
  std::vector<std::string> v = StringUtils::split(l, '=');

  if (v.empty() || v.size() > 2) {
    Logger::setLogLevel(LogLevel::INFO);
    LOG(ERR) << "strange log level '" << levelName
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
  } else if (l == "error") {
    level = LogLevel::ERR;
  } else if (l == "warning") {
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
      LOG(ERR) << "strange log level '" << levelName
               << "', using log level 'info'";
    } else {
      LOG(ERR) << "strange log level '" << levelName << "'";
    }

    return;
  }

  if (isGeneral) {
    Logger::setLogLevel(level);
  } else {
    MUTEX_LOCKER(guard, LogTopicNamesLock);

    auto const& name = v[0];
    auto it = LogTopicNames.find(name);

    if (it == LogTopicNames.end()) {
      LOG(ERR) << "strange topic '" << name << "'";
      return;
    }

    auto topic = it->second;

    if (topic != nullptr) {
      topic->setLogLevel(level);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the log level from a vector of string
////////////////////////////////////////////////////////////////////////////////

void Logger::setLogLevel(std::vector<std::string> const& levels) {
  for (auto level : levels) {
    setLogLevel(level);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the output prefix
///
/// Note that: this function should not be called if the logging is active.
////////////////////////////////////////////////////////////////////////////////

void Logger::setOutputPrefix(std::string const& prefix) {
  char* outputPrefix =
      TRI_DuplicateString(TRI_UNKNOWN_MEM_ZONE, prefix.c_str());

  if (outputPrefix == nullptr) {
    return;
  }

  auto old = OutputPrefix.exchange(outputPrefix);

  if (old != nullptr) {
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, old);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the line number mode
////////////////////////////////////////////////////////////////////////////////

void Logger::setShowLineNumber(bool show) {
  ShowLineNumber.store(show, std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the thread identifier mode
////////////////////////////////////////////////////////////////////////////////

void Logger::setShowThreadIdentifier(bool show) {
  ShowThreadIdentifier.store(show, std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the local time mode
////////////////////////////////////////////////////////////////////////////////

void Logger::setUseLocalTime(bool show) {
  UseLocalTime.store(show, std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string description for the log level
////////////////////////////////////////////////////////////////////////////////

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

std::ostream& operator<<(std::ostream& stream, LogLevel level) {
  stream << Logger::translateLogLevel(level);
  return stream;
}

LoggerStream& LoggerStream::operator<<(Logger::RANGE range) {
  std::ostringstream tmp;
  tmp << range.baseAddress << " - "
      << static_cast<void const*>(static_cast<char const*>(range.baseAddress) +
                                  range.size)
      << " (" << range.size << " bytes)";
  _out << tmp.str();
  return *this;
}

LoggerStream& LoggerStream::operator<<(Logger::DURATION duration) {
  std::ostringstream tmp;
  tmp << std::setprecision(duration._precision) << std::fixed
      << duration._duration;
  _out << tmp.str();
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last log entries
////////////////////////////////////////////////////////////////////////////////

std::vector<LogBuffer> Logger::bufferedEntries(LogLevel level, uint64_t start,
                                               bool upToLevel) {
  std::vector<LogBuffer> result;

  MUTEX_LOCKER(guard, RingBufferLock);

  size_t s = 0;
  size_t e;

  if (RingBufferId >= RING_BUFFER_SIZE) {
    s = e = (RingBufferId + 1) % RING_BUFFER_SIZE;
  } else {
    e = RingBufferId;
  }

  for (size_t i = s; i != e;) {
    LogBuffer& p = RingBuffer[i];

    if (p._id >= start) {
      if (upToLevel) {
        if ((int)p._level <= (int)level) {
          result.emplace_back(p);
        }
      } else {
        if (p._level == level) {
          result.emplace_back(p);
        }
      }
    }

    ++i;

    if (i >= RING_BUFFER_SIZE) {
      i = 0;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the logging component
////////////////////////////////////////////////////////////////////////////////

static std::atomic<bool> IsInitialized(false);
static std::atomic<bool> ShutdownInitalized(false);
static std::unique_ptr<LogThread> LoggingThread;

static void ShutdownLogging() { Logger::shutdown(true); }

void Logger::initialize(bool threaded) {
  if (!IsInitialized) {
    IsInitialized = true;
  }

  // logging is now active
  LoggingActive.store(true);

  // generate threaded logging?
  ThreadedLogging.store(threaded, std::memory_order_relaxed);

  if (threaded) {
    LoggingThread = std::make_unique<LogThread>("Logging");

    LoggingThread->start();
  }

  // always close logging at the end
  if (!ShutdownInitalized) {
    atexit(ShutdownLogging);
    ShutdownInitalized = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the logging components
////////////////////////////////////////////////////////////////////////////////

void Logger::shutdown(bool clearBuffers) {
  if (!IsInitialized) {
    return;
  }

  // logging is now inactive (this will terminate the logging thread)
  LoggingActive.store(false, std::memory_order_relaxed);

  // join with the logging thread
  if (ThreadedLogging) {
    LoggingActive.store(false, std::memory_order_relaxed);

    // ignore all errors for now as we cannot log them anywhere...
    LoggingThread->join();
  }

  // cleanup appenders
  {
    MUTEX_LOCKER(guard, AppendersLock);
    Appenders.clear();
  }

  // cleanup prefix
  {
    auto prefix = OutputPrefix.exchange(nullptr);

    if (prefix != nullptr) {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, prefix);
    }
  }

#if 0
  // and clear all logging buffers
  if (clearBuffers) {
    // cleanup output buffers
    MUTEX_LOCKER(mutexLocker, BufferLock);

    for (size_t i = 0; i < OUTPUT_LOG_LEVELS; i++) {
      for (size_t j = 0; j < OUTPUT_BUFFER_SIZE; j++) {
        if (BufferOutput[i][j]._text != nullptr) {
          TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, BufferOutput[i][j]._text);
          BufferOutput[i][j]._text = nullptr;
        }
      }
    }
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reopens all log appenders
////////////////////////////////////////////////////////////////////////////////

void Logger::reopen() {
  MUTEX_LOCKER(guard, AppendersLock);

  for (auto& it : Appenders) {
    for (std::unique_ptr<LogAppender>& appender : it.second) {
      try {
        appender->reopenLog();
      } catch (...) {
        // silently catch this error (we shouldn't try to log an error about a
        // logging error as this will get us into trouble with mutexes etc.)
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to flush the logging
////////////////////////////////////////////////////////////////////////////////

void Logger::flush() {
  if (!IsInitialized) {
    return;
  }

  if (ThreadedLogging) {
    int tries = 0;

    while (++tries < 500) {
      if (MessageQueue.empty()) {
        break;
      }

      usleep(10000);
    }
  }
}
