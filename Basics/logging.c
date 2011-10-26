////////////////////////////////////////////////////////////////////////////////
/// @brief logging macros and functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2010, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "logging.h"

#ifdef TRI_ENABLE_SYSLOG
#define SYSLOG_NAMES
#define prioritynames TRI_prioritynames
#define facilitynames TRI_facilitynames
#include <syslog.h>
#endif

#include <Basics/locks.h>
#include <Basics/strings.h>
#include <Basics/threads.h>
#include <Basics/vector.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                           LOGGING
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup LoggingPrivate Logging (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief log appenders
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_t _Appenders;

////////////////////////////////////////////////////////////////////////////////
/// @brief message queue lock
////////////////////////////////////////////////////////////////////////////////

static TRI_mutex_t _LogMessageQueueLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief message queue
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_t _LogMessageQueue;

////////////////////////////////////////////////////////////////////////////////
/// @brief thread used for logging
////////////////////////////////////////////////////////////////////////////////

static TRI_thread_t _LoggingThread;

////////////////////////////////////////////////////////////////////////////////
/// @brief human readable logging
////////////////////////////////////////////////////////////////////////////////

static bool _IsHuman = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief log fatal messages
////////////////////////////////////////////////////////////////////////////////

static bool _IsFatal = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief log error messages
////////////////////////////////////////////////////////////////////////////////

static bool _IsError = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief log warning messages
////////////////////////////////////////////////////////////////////////////////

static bool _IsWarning = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief log info messages
////////////////////////////////////////////////////////////////////////////////

static bool _IsInfo = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief log debug messages
////////////////////////////////////////////////////////////////////////////////

static bool _IsDebug = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief log trace messages
////////////////////////////////////////////////////////////////////////////////

static bool _IsTrace = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief show line numbers, debug and trace always show the line numbers
////////////////////////////////////////////////////////////////////////////////

static bool _ShowLineNumber = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief show function names
////////////////////////////////////////////////////////////////////////////////

static bool _ShowFunction = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief show thread identifier
////////////////////////////////////////////////////////////////////////////////

static bool _ShowThreadIdentifier = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief output prefix
////////////////////////////////////////////////////////////////////////////////

static char const * _OutputPrefix = NULL;

////////////////////////////////////////////////////////////////////////////////
/// @brief logging active
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t _LoggingActive = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief logging thread
////////////////////////////////////////////////////////////////////////////////

static bool _ThreadedLogging = false;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup LoggingPrivate Logging (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief message
////////////////////////////////////////////////////////////////////////////////

typedef struct LogMessage_s {
  TRI_log_level_e _level;
  TRI_log_severity_e _severity;
  char* _message;
  size_t _length;
}
LogMessage_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup LoggingPrivate Logging (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a message string
////////////////////////////////////////////////////////////////////////////////

static size_t GenerateMessage (char* buffer,
                               size_t size,
                               char const* func,
                               char const* file,
                               int line,
                               TRI_log_level_e level,
                               TRI_pid_t currentProcessId,
                               TRI_tid_t currentThreadId,
                               char const* fmt,
                               va_list ap) {
  char s[32];
  int m;
  int n;

  char const* ll;
  bool sln;

  time_t tt;
  struct tm tb;

  // .............................................................................
  // generate time prefix
  // .............................................................................

  tt = time(0);
  TRI_gmtime(tt, &tb);

  strftime(s,  sizeof(s) - 1, "%Y-%m-%dT%H:%M:%SZ", &tb);

  // .............................................................................
  // append the time prefix and output prefix
  // .............................................................................

  m = 0;

  if (_OutputPrefix) {
    n = snprintf(buffer + m, size - m, "%s %s ", s, _OutputPrefix);
  }
  else {
    n = snprintf(buffer + m, size - m, "%s ", s);
  }

  if (n < 0) {
    return n;
  }
  else if ((int) size <= m + n) {
    return m + n;
  }

  m += n;

  // .............................................................................
  // append the process / thread identifier
  // .............................................................................

  if (_ShowThreadIdentifier) {
    n = snprintf(buffer + m, size - m, "[%llu-%llu] ", (unsigned long long) currentProcessId, (unsigned long long) currentThreadId);
  }
  else {
    n = snprintf(buffer + m, size - m, "[%llu] ", (unsigned long long) currentProcessId);
  }

  if (n < 0) {
    return n;
  }
  else if ((int) size <= m + n) {
    return m + n;
  }

  m += n;

  // .............................................................................
  // append the log level
  // .............................................................................

  ll = "UNKNOWN";

  switch (level) {
    case TRI_LOG_FATAL: ll = "FATAL"; break;
    case TRI_LOG_ERROR: ll = "ERROR"; break;
    case TRI_LOG_WARNING: ll = "WARNING"; break;
    case TRI_LOG_INFO: ll = "INFO"; break;
    case TRI_LOG_DEBUG: ll = "DEBUG"; break;
    case TRI_LOG_TRACE: ll = "TRACE"; break;
  }

  n = snprintf(buffer + m, size - m, "%s ", ll);

  if (n < 0) {
    return n;
  }
  else if ((int) size <= m + n) {
    return m + n;
  }

  m += n;

  // .............................................................................
  // check if we must disply the line number
  // .............................................................................

  sln = _ShowLineNumber;

  switch (level) {
    case TRI_LOG_DEBUG:
    case TRI_LOG_TRACE:
      sln = true;
      break;

    default:
      break;
  }

  // .............................................................................
  // append the file and line
  // .............................................................................

  if (sln) {
    if (_ShowFunction) {
      n = snprintf(buffer + m, size - m, "[%s@%s:%d] ", func, file, line);
    }
    else {
      n = snprintf(buffer + m, size - m, "[%s:%d] ", file, line);
    }

    if (n < 0) {
      return n;
    }
    else if ((int) size <= m + n) {
      return m + n;
    }

    m += n;
  }

  // .............................................................................
  // append the message
  // .............................................................................

  n = vsnprintf(buffer + m, size - n, fmt, ap);

  if (n < 0) {
    return n;
  }

  return m + n;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs a message string to all appenders
////////////////////////////////////////////////////////////////////////////////

static void OutputMessage (TRI_log_level_e level,
                           TRI_log_severity_e severity,
                           char* message,
                           size_t length,
                           bool copy) {
  if (! _LoggingActive) {
    if (! copy) {
      TRI_FreeString(message);
    }

    return;
  }

  if (_ThreadedLogging) {
    LogMessage_t msg;
    msg._level = level;
    msg._severity = severity;
    msg._message = copy ? TRI_DuplicateString2(message, length) : message;
    msg._length = length;

    TRI_LockMutex(&_LogMessageQueueLock);
    TRI_PushBackVector(&_LogMessageQueue, (void*) &msg);
    TRI_UnlockMutex(&_LogMessageQueueLock);
  }
  else {
    size_t n = TRI_SizeVector(&_Appenders);
    size_t i;

    for (i = 0;  i < n;  ++i) {
      TRI_LogAppender_t* appender = * (TRI_LogAppender_t**) TRI_AtVector(&_Appenders, i);

      appender->log(appender, level, severity, message, length);
    }

    if (! copy) {
      TRI_FreeString(message);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the message queue and sends message to appenders
////////////////////////////////////////////////////////////////////////////////

static void MessageQueueWorker (void* data) {
  int sl;
  size_t m;
  size_t j;

  TRI_vector_t buffer;

  TRI_InitVector(&buffer, sizeof(LogMessage_t));

  sl = 100;

  while (true) {
    TRI_LockMutex(&_LogMessageQueueLock);

    if (TRI_EmptyVector(&_LogMessageQueue)) {
      TRI_UnlockMutex(&_LogMessageQueueLock);
      sl += 1000;
    }
    else {
      size_t n;
      size_t i;

      // move message from queue into temporary buffer
      n = TRI_SizeVector(&_Appenders);
      m = TRI_SizeVector(&_LogMessageQueue);

      for (j = 0;  j < m;  ++j) {
        TRI_PushBackVector(&buffer, TRI_AtVector(&_LogMessageQueue, j));
      }

      TRI_ClearVector(&_LogMessageQueue);

      TRI_UnlockMutex(&_LogMessageQueueLock);

      // output messages using the appenders

      for (j = 0;  j < m;  ++j) {
        LogMessage_t* msg;

        msg = TRI_AtVector(&buffer, j);

        for (i = 0;  i < n;  ++i) {
          TRI_LogAppender_t* appender = * (TRI_LogAppender_t**) TRI_AtVector(&_Appenders, i);

          appender->log(appender, msg->_level, msg->_severity, msg->_message, msg->_length);
        }

        TRI_FreeString(msg->_message);
      }

      TRI_ClearVector(&buffer);

      // sleep a little while
      sl = 100;
    }

    if (_LoggingActive) {
      usleep(sl);
    }
    else {
      TRI_LockMutex(&_LogMessageQueueLock);

      if (TRI_EmptyVector(&_LogMessageQueue)) {
        TRI_UnlockMutex(&_LogMessageQueueLock);
        break;
      }

      TRI_UnlockMutex(&_LogMessageQueueLock);
    }
  }

  // cleanup
  TRI_LockMutex(&_LogMessageQueueLock);
  TRI_DestroyVector(&buffer);

  m = TRI_SizeVector(&_LogMessageQueue);

  for (j = 0;  j < m;  ++j) {
    LogMessage_t* msg;

    msg = TRI_AtVector(&_LogMessageQueue, j);
    TRI_FreeString(msg->_message);
  }

  TRI_ClearVector(&_LogMessageQueue);

  TRI_UnlockMutex(&_LogMessageQueueLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a new message with given thread information
////////////////////////////////////////////////////////////////////////////////

static void LogThread (char const* func,
                       char const* file,
                       int line,
                       TRI_log_level_e level,
                       TRI_log_severity_e severity,
                       TRI_pid_t processId,
                       TRI_tid_t threadId,
                       char const* fmt,
                       va_list ap) {
  static int maxSize = 100 * 1024;

  va_list ap2;
  char buffer[4096];   // try a static buffer first
  int n;

  va_copy(ap2, ap);
  n = GenerateMessage(buffer, sizeof(buffer), func, file, line, level, processId, threadId, fmt, ap2);
  va_end(ap2);

  if (n == -1) {
    TRI_Log(func, file, line, TRI_LOG_WARNING, TRI_LOG_SEVERITY_HUMAN, "format string is corrupt");
  }
  else if (n < (int) sizeof(buffer)) {
    OutputMessage(level, severity, buffer, n, true);
  }
  else {
    while (n < maxSize) {
      int m;
      char* p;

      p = TRI_Allocate(n);

      if (p == NULL) {
        TRI_Log(func, file, line, TRI_LOG_ERROR, TRI_LOG_SEVERITY_HUMAN, "log message is too large (%d bytes)", n);
        return;
      }
      else {
        va_copy(ap2, ap);
        m = GenerateMessage(p, n, func, file, line, level, processId, threadId, fmt, ap2);
        va_end(ap2);

        if (m == -1) {
          TRI_Free(p);
          TRI_Log(func, file, line, TRI_LOG_WARNING, TRI_LOG_SEVERITY_HUMAN, "format string is corrupt");
          return;
        }
        else if (m > n) {
          free(p);
          n = m;
        }
        else {
          OutputMessage(level, severity, p, n, false);
          return;
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the log level
////////////////////////////////////////////////////////////////////////////////

char const* TRI_GetLogLevelLogging () {
  if (_IsTrace) {
    return "trace";
  }

  if (_IsDebug) {
    return "debug";
  }

  if (_IsInfo) {
    return "info";
  }

  if (_IsWarning) {
    return "warning";
  }

  if (_IsError) {
    return "error";
  }

  return "fatal";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the log level
////////////////////////////////////////////////////////////////////////////////

void TRI_SetLogLevelLogging (char const* level) {
  _IsFatal = true;
  _IsError = false;
  _IsWarning = false;
  _IsInfo = false;
  _IsDebug = false;
  _IsTrace = false;

  if (TRI_CaseEqualString(level, "fatal")) {
  }
  else if (TRI_CaseEqualString(level, "error")) {
    _IsError = true;
  }
  else if (TRI_CaseEqualString(level, "warning")) {
    _IsError = true;
    _IsWarning = true;
  }
  else if (TRI_CaseEqualString(level, "info")) {
    _IsError = true;
    _IsWarning = true;
    _IsInfo = true;
  }
  else if (TRI_CaseEqualString(level, "debug")) {
    _IsError = true;
    _IsWarning = true;
    _IsInfo = true;
    _IsDebug = true;
  }
  else if (TRI_CaseEqualString(level, "trace")) {
    _IsError = true;
    _IsWarning = true;
    _IsInfo = true;
    _IsDebug = true;
    _IsTrace = true;
  }
  else {
    _IsError = true;
    _IsWarning = true;

    LOG_ERROR("strange log level '%s'", level);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if human logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsHumanLogging () {
  return _IsHuman;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if fatal logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsFatalLogging () {
  return _IsFatal;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if error logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsErrorLogging () {
  return _IsError;

}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if warning logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsWarningLogging () {
  return _IsWarning;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if info logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsInfoLogging () {
  return _IsInfo;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if debug logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsDebugLogging () {
  return _IsDebug;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if trace logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsTraceLogging () {
  return _IsTrace;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a new message
////////////////////////////////////////////////////////////////////////////////

void TRI_Log (char const* func,
              char const* file,
              int line,
              TRI_log_level_e level,
              TRI_log_severity_e severity,
              char const* fmt,
              ...) {
  va_list ap;

  va_start(ap, fmt);
  LogThread(func, file, line, level, severity, TRI_CurrentProcessId(), TRI_CurrentThreadId(), fmt, ap);
  va_end(ap);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 LOG FILE APPENDER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup LoggingPrivate Logging (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief structure for file log appenders
////////////////////////////////////////////////////////////////////////////////

typedef struct LogAppenderFile_s {
  TRI_LogAppender_t _base;

  char* _filename;
  int _fd;
}
LogAppenderFile_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup LoggingPrivate Logging (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief writes to a log file
////////////////////////////////////////////////////////////////////////////////

static void WriteLogFile (int fd, char const* buffer, ssize_t len) {
  while (len > 0) {
    ssize_t n;

    n = TRI_WRITE(fd, buffer, len);

    if (n <= 0) {
      fprintf(stderr, "cannot log data: %s", TRI_LAST_ERROR_STR);
      return; // give up, but do not try to log failure
    }

    buffer += n;
    len -= n;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message to a log file appender
////////////////////////////////////////////////////////////////////////////////

static void LogAppenderFile_Log (TRI_LogAppender_t* appender,
                                 TRI_log_level_e level,
                                 TRI_log_severity_e severity,
                                 char const* msg,
                                 size_t length) {
  LogAppenderFile_t* self;
  char* escaped;
  size_t escapedLength;

  self = (LogAppenderFile_t*) appender;

  if (self->_fd < 0) {
    return;
  }

  escaped = TRI_EscapeCString(msg, length, &escapedLength);

  WriteLogFile(self->_fd, escaped, escapedLength);
  WriteLogFile(self->_fd, "\n", 1);

  TRI_FreeString(escaped);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a log file appender
////////////////////////////////////////////////////////////////////////////////

static void LogAppenderFile_Close (TRI_LogAppender_t* appender) {
  LogAppenderFile_t* self;

  self = (LogAppenderFile_t*) appender;

 if (self->_fd >= 3) {
    TRI_CLOSE(self->_fd);
  }

  self->_fd = -1;
  TRI_FreeString(self->_filename);

  TRI_Free(self);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a log appender for file output
////////////////////////////////////////////////////////////////////////////////

TRI_LogAppender_t* TRI_LogAppenderFile (char const* filename) {
  LogAppenderFile_t* appender;

  // no logging
  if (filename == NULL) {
    return NULL;
  }

  // allocate space
  appender = TRI_Allocate(sizeof(LogAppenderFile_t));

  // logging to stdout
  if (TRI_EqualString(filename, "+")) {
    appender->_filename = NULL;
    appender->_fd = 1;
  }

  // logging to stderr
  else if (TRI_EqualString(filename, "-")) {
    appender->_filename = NULL;
    appender->_fd = 2;
  }

  // logging to file
  else {
    appender->_fd = TRI_CREATE(filename, O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);

    if (appender->_fd < 0) {
      TRI_Free(appender);
      return NULL;
    }

    appender->_filename = TRI_DuplicateString(filename);
  }

  // set methods
  appender->_base.log = LogAppenderFile_Log;
  appender->_base.close = LogAppenderFile_Close;

  // and return base structure
  return &appender->_base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   SYSLOG APPENDER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup LoggingPrivate Logging (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief structure for syslog appenders
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

typedef struct TRI_LogAppenderSyslog_s {
  TRI_LogAppender_t _base;
}
TRI_LogAppenderSyslog_t;

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup LoggingPrivate Logging (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message to a log file appender
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

static void LogAppenderSyslog_Log (TRI_LogAppender_t* appender,
                                   TRI_log_level_e level,
                                   TRI_log_severity_e severity,
                                   char const* msg,
                                   size_t length) {
  int priority;

  switch (severity) {
    case TRI_LOG_SEVERITY_EXCEPTION: priority = LOG_CRIT;  break;
    case TRI_LOG_SEVERITY_FUNCTIONAL: priority = LOG_NOTICE;  break;
    case TRI_LOG_SEVERITY_TECHNICAL: priority = LOG_INFO;  break;
    case TRI_LOG_SEVERITY_DEVELOPMENT: priority = LOG_DEBUG;  break;
    default: priority = LOG_DEBUG;  break;
  }

  syslog(priority, "%s", msg);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a log file appender
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

static void LogAppenderSyslog_Close (TRI_LogAppender_t* appender) {
  TRI_LogAppenderSyslog_t* self;

  self = (TRI_LogAppenderSyslog_t*) appender;
  closelog();
  TRI_Free(self);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a log appender for file output
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

TRI_LogAppender_t* TRI_LogAppenderSyslog (char const* name, char const* facility) {
  TRI_LogAppenderSyslog_t* appender;
  int value;

  // no logging
  if (name == NULL) {
    return NULL;
  }

  // allocate space
  appender = (TRI_LogAppenderSyslog_t*) TRI_Allocate(sizeof(TRI_LogAppenderSyslog_t));

  // set methods
  appender->_base.log = LogAppenderSyslog_Log;
  appender->_base.close = LogAppenderSyslog_Close;

  // find facility
  value = LOG_LOCAL0;

  if ('0' <= facility[0] && facility[0] <= '9') {
    value = atoi(facility);
  }
  else {
    CODE * ptr = TRI_facilitynames;

    while (ptr->c_name != 0) {
      if (strcmp(ptr->c_name, facility) == 0) {
        value = ptr->c_val;
        break;
      }

      ++ptr;
    }
  }

  // and open logging
  openlog(name, LOG_CONS | LOG_PID, value);

  // and return base structure
  return &appender->_base;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the logging components
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseLogging (bool threaded) {
  TRI_LogAppender_t* a;

  TRI_InitVector(&_Appenders, sizeof(TRI_LogAppender_t*));

  // TODO
  a = TRI_LogAppenderFile("+");
  TRI_PushBackVector(&_Appenders, &a);

#ifdef TRI_ENABLE_SYSLOG
  a = TRI_LogAppenderSyslog("test", "local0");
  TRI_PushBackVector(&_Appenders, &a);
#endif

  // generate threaded logging
  _ThreadedLogging = threaded;

  if (threaded) {
    TRI_InitMutex(&_LogMessageQueueLock);

    TRI_InitVector(&_LogMessageQueue, sizeof(LogMessage_t));

    TRI_InitThread(&_LoggingThread);
    TRI_StartThread(&_LoggingThread, MessageQueueWorker, 0);
  }

  // logging is now active
  _LoggingActive = 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the logging components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownLogging () {
  size_t n;
  size_t i;

  // logging is now inactive
  _LoggingActive = 0;

  if (_ThreadedLogging) {
    TRI_JoinThread(&_LoggingThread);

    TRI_DestroyMutex(&_LogMessageQueueLock);

    TRI_DestroyVector(&_LogMessageQueue);
  }

  // cleanup appenders
  n = TRI_SizeVector(&_Appenders);

  for (i = 0;  i < n;  ++i) {
    TRI_LogAppender_t* appender = * (TRI_LogAppender_t**) TRI_AtVector(&_Appenders, i);

    appender->close(appender);
  }

  TRI_DestroyVector(&_Appenders);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|// --SECTION--\\)"
// End:
