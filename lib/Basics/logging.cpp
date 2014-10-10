////////////////////////////////////////////////////////////////////////////////
/// @brief logging macros and functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "Basics/logging.h"
#include "Basics/shell-colors.h"

#ifdef TRI_ENABLE_SYSLOG
#define SYSLOG_NAMES
#define prioritynames TRI_prioritynames
#define facilitynames TRI_facilitynames
#include <syslog.h>
#endif

#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/locks.h"
#include "Basics/tri-strings.h"
#include "Basics/threads.h"
#include "Basics/vector.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                           LOGGING
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief log appenders type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  APPENDER_TYPE_FILE,
  APPENDER_TYPE_SYSLOG
}
TRI_log_appender_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief base structure for log appenders
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_log_appender_s {
  void (*log) (struct TRI_log_appender_s*, TRI_log_level_e, TRI_log_severity_e, char const* msg, size_t length);
  void (*reopen) (struct TRI_log_appender_s*);
  void (*close) (struct TRI_log_appender_s*);
  char* (*details) (struct TRI_log_appender_s*);

  char*                     _contentFilter;   // an optional content filter for log messages
  TRI_log_severity_e        _severityFilter;  // appender will care only about message with a specific severity. set to TRI_LOG_SEVERITY_UNKNOWN to catch all
  TRI_log_appender_type_e   _type;
  bool                      _consume;         // whether or not the appender will consume the message (true) or let it through to other appenders (false)
}
TRI_log_appender_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialised
////////////////////////////////////////////////////////////////////////////////

static volatile int Initialised = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown function already installed
////////////////////////////////////////////////////////////////////////////////

static volatile bool ShutdownInitalised = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief name of first log file
////////////////////////////////////////////////////////////////////////////////

static char* LogfileName = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief log appenders
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_pointer_t Appenders;

////////////////////////////////////////////////////////////////////////////////
/// @brief log appenders
////////////////////////////////////////////////////////////////////////////////

static TRI_spin_t AppendersLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief maximal output length
////////////////////////////////////////////////////////////////////////////////

#define OUTPUT_MAX_LENGTH (256)

////////////////////////////////////////////////////////////////////////////////
/// @brief output buffer size
////////////////////////////////////////////////////////////////////////////////

#define OUTPUT_BUFFER_SIZE (1024)

////////////////////////////////////////////////////////////////////////////////
/// @brief output log levels
////////////////////////////////////////////////////////////////////////////////

#define OUTPUT_LOG_LEVELS (6)

////////////////////////////////////////////////////////////////////////////////
/// @brief current buffer lid
////////////////////////////////////////////////////////////////////////////////

static uint64_t BufferLID = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief current buffer position
////////////////////////////////////////////////////////////////////////////////

static size_t BufferCurrent[OUTPUT_LOG_LEVELS] = { 0, 0, 0, 0, 0, 0 };

////////////////////////////////////////////////////////////////////////////////
/// @brief current buffer output
////////////////////////////////////////////////////////////////////////////////

static TRI_log_buffer_t BufferOutput[OUTPUT_LOG_LEVELS][OUTPUT_BUFFER_SIZE];

////////////////////////////////////////////////////////////////////////////////
/// @brief buffer lock
////////////////////////////////////////////////////////////////////////////////

static TRI_mutex_t BufferLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable for the logger
////////////////////////////////////////////////////////////////////////////////

static TRI_condition_t LogCondition;

////////////////////////////////////////////////////////////////////////////////
/// @brief message queue lock
////////////////////////////////////////////////////////////////////////////////

static TRI_mutex_t LogMessageQueueLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief message queue
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_t LogMessageQueue;

////////////////////////////////////////////////////////////////////////////////
/// @brief thread used for logging
////////////////////////////////////////////////////////////////////////////////

static TRI_thread_t LoggingThread;

////////////////////////////////////////////////////////////////////////////////
/// @brief thread used for logging
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t LoggingThreadActive = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief usage logging
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsUsage = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief human readable logging
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsHuman = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief exception readable logging
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsException = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief technical readable logging
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsTechnical = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief functional readable logging
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsFunctional = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief development readable logging
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsDevelopment = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief log fatal messages
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsFatal = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief log error messages
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsError = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief log warning messages
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsWarning = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief log info messages
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsInfo = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief log debug messages
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsDebug = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief log trace messages
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsTrace = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief use local time for dates & times in log output
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t UseLocalTime = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief show line numbers, debug and trace always show the line numbers
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t ShowLineNumber = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief show function names
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t ShowFunction = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief show thread identifier
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t ShowThreadIdentifier = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief output prefix
////////////////////////////////////////////////////////////////////////////////

static char* OutputPrefix = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief output prefix lock
////////////////////////////////////////////////////////////////////////////////

static TRI_spin_t OutputPrefixLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief logging active
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t LoggingActive = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief logging thread
////////////////////////////////////////////////////////////////////////////////

static bool ThreadedLogging = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief use file based logging
////////////////////////////////////////////////////////////////////////////////

static bool UseFileBasedLogging = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief size of hash
////////////////////////////////////////////////////////////////////////////////

#define FilesToLogSize (1024 * 1024)

////////////////////////////////////////////////////////////////////////////////
/// @brief files to log
////////////////////////////////////////////////////////////////////////////////

static bool FilesToLog [FilesToLogSize];

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief message
////////////////////////////////////////////////////////////////////////////////

typedef struct log_message_s {
  TRI_log_level_e _level;
  TRI_log_severity_e _severity;
  char* _message;
  size_t _length;
}
log_message_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stores output in a buffer
////////////////////////////////////////////////////////////////////////////////

static void StoreOutput (TRI_log_level_e level,
                         time_t timestamp,
                         char const* text,
                         size_t length) {
  TRI_log_buffer_t* buf;
  size_t pos;
  size_t cur;
  size_t oldPos;

  pos = (size_t) level;

  if (pos >= OUTPUT_LOG_LEVELS) {
    return;
  }

  TRI_LockMutex(&BufferLock);
  oldPos = BufferCurrent[pos];
  BufferCurrent[pos] = (oldPos + 1) % OUTPUT_BUFFER_SIZE;
  cur = BufferCurrent[pos];
  buf = &BufferOutput[pos][cur];

  if (buf->_text != nullptr) {
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, buf->_text);
    buf->_text = nullptr;
  }

  buf->_lid = BufferLID++;
  buf->_level = level;
  buf->_timestamp = timestamp;

  if (length > OUTPUT_MAX_LENGTH) {
    // use the UNKNOWN_MEM_ZONE here...
    // if we use CORE_MEM_ZONE and malloc fails, this fact would be logged.
    // but we are in the logging already...
    buf->_text = static_cast<char*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, OUTPUT_MAX_LENGTH + 1, false));

    if (buf->_text == nullptr) {
      // revert...
      BufferLID--;
      BufferCurrent[pos] = oldPos;
    }
    else {
      memcpy(buf->_text, text, OUTPUT_MAX_LENGTH - 4);
      memcpy(buf->_text + OUTPUT_MAX_LENGTH - 4, " ...", 4);
      // append the \0 byte, otherwise we have potentially unbounded strings
      buf->_text[OUTPUT_MAX_LENGTH] = '\0';
    }
  }
  else {
    buf->_text = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, text, length);
  }

  TRI_UnlockMutex(&BufferLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares the lid
////////////////////////////////////////////////////////////////////////////////

static int LidCompare (void const* l, void const* r) {
  TRI_log_buffer_t const* left = static_cast<TRI_log_buffer_t const*>(l);
  TRI_log_buffer_t const* right = static_cast<TRI_log_buffer_t const*>(r);

  return (int) (((int64_t) left->_lid) - ((int64_t) right->_lid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a message string
////////////////////////////////////////////////////////////////////////////////

static int GenerateMessage (char* buffer,
                            size_t size,
                            int* offset,
                            char const* func,
                            char const* file,
                            int line,
                            TRI_log_level_e level,
                            TRI_pid_t currentProcessId,
                            TRI_tid_t currentThreadId,
                            char const* fmt,
                            va_list ap) {
  int m;
  int n;

  char const* ll;
  bool sln;

  // we store the "real" beginning of the message (without any prefixes) here
  *offset = 0;

  // .............................................................................
  // append the time prefix and output prefix
  // .............................................................................

  n = 0;

  // TODO: can this lock be removed? the prefix is only set at the server startup once
  TRI_LockSpin(&OutputPrefixLock);


  if (OutputPrefix && *OutputPrefix) {
    n = snprintf(buffer, size, "%s ", OutputPrefix);
  }

  TRI_UnlockSpin(&OutputPrefixLock);

  if (n < 0) {
    return n;
  }
  else if ((int) size <= n) {
    return n;
  }

  m = n;

  // .............................................................................
  // append the process / thread identifier
  // .............................................................................

  if (ShowThreadIdentifier) {
    n = snprintf(buffer + m,
                 size - m,
                 "[%llu-%llu] ",
                 (unsigned long long) currentProcessId,
                 (unsigned long long) currentThreadId);
  }
  else {
    n = snprintf(buffer + m,
                 size - m,
                 "[%llu] ",
                 (unsigned long long) currentProcessId);
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
    case TRI_LOG_LEVEL_FATAL:   ll = "FATAL"; break;
    case TRI_LOG_LEVEL_ERROR:   ll = "ERROR"; break;
    case TRI_LOG_LEVEL_WARNING: ll = "WARNING"; break;
    case TRI_LOG_LEVEL_INFO:    ll = "INFO"; break;
    case TRI_LOG_LEVEL_DEBUG:   ll = "DEBUG"; break;
    case TRI_LOG_LEVEL_TRACE:   ll = "TRACE"; break;
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
  // check if we must display the line number
  // .............................................................................

  sln = ShowLineNumber > 0;

  switch (level) {
    case TRI_LOG_LEVEL_DEBUG:
    case TRI_LOG_LEVEL_TRACE:
      sln = true;
      break;

    default:
      break;
  }

  // .............................................................................
  // append the file and line
  // .............................................................................

  if (sln) {
    if (ShowFunction) {
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

  // store the "real" beginning of the message (without any prefixes) in offset
  *offset = m;
  n = vsnprintf(buffer + m, size - m, fmt, ap);

  if (n < 0) {
    return n;
  }

  return m + n;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write to stderr
////////////////////////////////////////////////////////////////////////////////

static void WriteStderr (TRI_log_level_e level,
                         char const* msg) {
  if (level == TRI_LOG_LEVEL_FATAL || level == TRI_LOG_LEVEL_ERROR) {
    fprintf(stderr, TRI_SHELL_COLOR_RED "%s" TRI_SHELL_COLOR_RESET "\n", msg);
  }
  else if (level == TRI_LOG_LEVEL_WARNING) {
    fprintf(stderr, TRI_SHELL_COLOR_YELLOW "%s" TRI_SHELL_COLOR_RESET "\n", msg);
  }
  else {
    fprintf(stderr, "%s\n", msg);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs a message string to all appenders
////////////////////////////////////////////////////////////////////////////////

static void OutputMessage (TRI_log_level_e level,
                           TRI_log_severity_e severity,
                           char* message,
                           size_t length,
                           size_t offset,
                           bool copy) {
  TRI_ASSERT(message != nullptr);

  if (! LoggingActive) {
    WriteStderr(level, message);

    if (! copy) {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, message);
    }

    return;
  }

  // copy message to ring buffer of recent log messages
  if (severity == TRI_LOG_SEVERITY_HUMAN) {
    // we start copying the message from the given offset to skip any irrelevant
    // or redundant message parts such as date, info etc. The offset might be 0 though.
    TRI_ASSERT(length >= offset);
    StoreOutput(level, time(0), message + offset, (size_t) (length - offset));
  }

  TRI_LockSpin(&AppendersLock);

  if (Appenders._length == 0) {
    WriteStderr(level, message);

    TRI_UnlockSpin(&AppendersLock);

    if (! copy) {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, message);
    }

    return;
  }

  TRI_UnlockSpin(&AppendersLock);

  if (ThreadedLogging) {
    log_message_t msg;
    msg._level = level;
    msg._severity = severity;
    msg._message = copy ? TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, message, length) : message;
    msg._length = length;

    // check if we had enough memory to copy the message string
    if (msg._message != nullptr) {
      // this will COPY the structure log_message_t into the vector
      TRI_LockMutex(&LogMessageQueueLock);
      TRI_PushBackVector(&LogMessageQueue, (void*) &msg);
      TRI_UnlockMutex(&LogMessageQueueLock);
    }
  }
  else {
    size_t i;

    TRI_LockSpin(&AppendersLock);

    for (i = 0;  i < Appenders._length;  ++i) {
      TRI_log_appender_t* appender = static_cast<TRI_log_appender_t*>(Appenders._buffer[i]);

      TRI_ASSERT(appender != nullptr);

      // apply severity filter
      if (appender->_severityFilter != TRI_LOG_SEVERITY_UNKNOWN &&
          appender->_severityFilter != severity) {
        continue;
      }

      // apply content filter on log message
      if (appender->_contentFilter != nullptr) {
        if (! TRI_IsContainedString(message, appender->_contentFilter)) {
          continue;
        }
      }

      appender->log(appender, level, severity, message, length);

      if (appender->_consume) {
        break;
      }
    }

    TRI_UnlockSpin(&AppendersLock);

    if (! copy) {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, message);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the message queue and sends message to appenders
////////////////////////////////////////////////////////////////////////////////

static void MessageQueueWorker (void* data) {
  int sl;
  size_t j;

  TRI_vector_t buffer;

  TRI_InitVector(&buffer, TRI_CORE_MEM_ZONE, sizeof(log_message_t));

  sl = 100;
  LoggingThreadActive = 1;

  while (true) {
    TRI_LockMutex(&LogMessageQueueLock);

    if (TRI_EmptyVector(&LogMessageQueue)) {
      TRI_UnlockMutex(&LogMessageQueueLock);

      sl += 1000;

      if (1000000 < sl) {
        sl = 1000000;
      }
    }
    else {
      size_t m;
      size_t i;

      TRI_ASSERT(buffer._length == 0);

      // move message from queue into temporary buffer
      m = LogMessageQueue._length;

      for (j = 0;  j < m;  ++j) {
        TRI_PushBackVector(&buffer, TRI_AtVector(&LogMessageQueue, j));
      }

      TRI_ClearVector(&LogMessageQueue);

      TRI_UnlockMutex(&LogMessageQueueLock);

      // output messages using the appenders
      m = buffer._length;

      for (j = 0;  j < m;  ++j) {
        log_message_t* msg = static_cast<log_message_t*>(TRI_AtVector(&buffer, j));
        TRI_ASSERT(msg->_message != nullptr);

        TRI_LockSpin(&AppendersLock);

        for (i = 0;  i < Appenders._length;  ++i) {
          TRI_log_appender_t* appender = static_cast<TRI_log_appender_t*>(Appenders._buffer[i]);

          TRI_ASSERT(appender != nullptr);

          // apply severity filter
          if (appender->_severityFilter != TRI_LOG_SEVERITY_UNKNOWN &&
            appender->_severityFilter != msg->_severity) {
            continue;
          }

          // apply content filter on log message
          if (appender->_contentFilter != nullptr) {
            if (! TRI_IsContainedString(msg->_message, appender->_contentFilter)) {
              continue;
            }
          }

          appender->log(appender, msg->_level, msg->_severity, msg->_message, msg->_length);

          if (appender->_consume) {
            break;
          }
        }

        TRI_UnlockSpin(&AppendersLock);

        TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, msg->_message);
      }

      TRI_ClearVector(&buffer);

      // sleep a little while
      sl = 100;
    }

    if (LoggingActive) {
      TRI_LockCondition(&LogCondition);
      TRI_TimedWaitCondition(&LogCondition, (uint64_t) sl);
      TRI_UnlockCondition(&LogCondition);
    }
    else {
      TRI_LockMutex(&LogMessageQueueLock);

      if (TRI_EmptyVector(&LogMessageQueue)) {
        TRI_UnlockMutex(&LogMessageQueueLock);
        break;
      }

      TRI_UnlockMutex(&LogMessageQueueLock);
    }
  }

  // cleanup
  TRI_LockMutex(&LogMessageQueueLock);
  TRI_DestroyVector(&buffer);

  for (j = 0;  j < LogMessageQueue._length;  ++j) {
    log_message_t* msg = static_cast<log_message_t*>(TRI_AtVector(&LogMessageQueue, j));
    TRI_ASSERT(msg->_message != nullptr);
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, msg->_message);
  }

  TRI_ClearVector(&LogMessageQueue);

  TRI_UnlockMutex(&LogMessageQueueLock);

  LoggingThreadActive = 0;
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
  static const int maxSize = 100 * 1024;
  va_list ap2;
  char buffer[2048];  // try a static buffer first
  time_t tt;
  struct tm tb;
  size_t len;
  int offset;
  int n;

  // .............................................................................
  // generate time prefix
  // .............................................................................

  tt = time(0);
  if (UseLocalTime == 0) {
    // use GMtime
    TRI_gmtime(tt, &tb);
    // write time in buffer
    len = strftime(buffer, 32, "%Y-%m-%dT%H:%M:%SZ ", &tb);
  }
  else {
    // use localtime
    TRI_localtime(tt, &tb);
    len = strftime(buffer, 32, "%Y-%m-%dT%H:%M:%S ", &tb);
  }


  va_copy(ap2, ap);
  n = GenerateMessage(buffer + len, sizeof(buffer) - len, &offset, func, file, line, level, processId, threadId, fmt, ap2);
  va_end(ap2);


  if (n == -1) {
    TRI_Log(func, file, line, TRI_LOG_LEVEL_WARNING, TRI_LOG_SEVERITY_HUMAN, "format string is corrupt");
    return;
  }
  if (n < (int) (sizeof(buffer) - len)) {
    // static buffer was big enough
    OutputMessage(level, severity, buffer, (size_t) (n + len), (size_t) (offset + len), true);
    return;
  }

  // static buffer was not big enough
  while (n < maxSize) {
    int m;

    // allocate as much memory as we need
    char* p = static_cast<char*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, n + len + 2, false));

    if (p == nullptr) {
      TRI_Log(func, file, line, TRI_LOG_LEVEL_ERROR, TRI_LOG_SEVERITY_HUMAN, "log message is too large (%d bytes)", n + len);
      return;
    }

    if (len > 0) {
      // copy still existing and unchanged time prefix into dynamic buffer
      memcpy(p, buffer, len);
    }

    va_copy(ap2, ap);
    m = GenerateMessage(p + len, n + 1, &offset, func, file, line, level, processId, threadId, fmt, ap2);
    va_end(ap2);

    if (m == -1) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, p);
      TRI_Log(func, file, line, TRI_LOG_LEVEL_WARNING, TRI_LOG_SEVERITY_HUMAN, "format string is corrupt");
      return;
    }
    else if (m > n) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, p);
      n = m;
      // again
    }
    else {
      // finally got a buffer big enough. p is freed in OutputMessage
      if (m + len - 1 > 0) {
        OutputMessage(level, severity, p, (size_t) (m + len), (size_t) (offset + len), false);
      }

      return;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes all log appenders
////////////////////////////////////////////////////////////////////////////////

static void CloseLogging (void) {
  TRI_LockSpin(&AppendersLock);

  while (0 < Appenders._length) {
    TRI_log_appender_t* appender = static_cast<TRI_log_appender_t*>(Appenders._buffer[0]);

    TRI_RemoveVectorPointer(&Appenders, 0);
    appender->close(appender);
  }

  TRI_UnlockSpin(&AppendersLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs fatal error, cleans up, and exists
////////////////////////////////////////////////////////////////////////////////

void CLEANUP_LOGGING_AND_EXIT_ON_FATAL_ERROR () {
  TRI_ShutdownLogging(true);
  TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the log level
////////////////////////////////////////////////////////////////////////////////

char const* TRI_LogLevelLogging () {
  if (IsTrace) {
    return "trace";
  }

  if (IsDebug) {
    return "debug";
  }

  if (IsInfo) {
    return "info";
  }

  if (IsWarning) {
    return "warning";
  }

  if (IsError) {
    return "error";
  }

  return "fatal";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the log level
////////////////////////////////////////////////////////////////////////////////

void TRI_SetLogLevelLogging (char const* level) {
  IsFatal = 1;
  IsError = 0;
  IsWarning = 0;
  IsInfo = 0;
  IsDebug = 0;
  IsTrace = 0;

  if (TRI_CaseEqualString(level, "fatal")) {
  }
  else if (TRI_CaseEqualString(level, "error")) {
    IsError = 1;
  }
  else if (TRI_CaseEqualString(level, "warning")) {
    IsError = 1;
    IsWarning = 1;
  }
  else if (TRI_CaseEqualString(level, "info")) {
    IsError = 1;
    IsWarning = 1;
    IsInfo = 1;
  }
  else if (TRI_CaseEqualString(level, "debug")) {
    IsError = 1;
    IsWarning = 1;
    IsInfo = 1;
    IsDebug = 1;
  }
  else if (TRI_CaseEqualString(level, "trace")) {
    IsError = 1;
    IsWarning = 1;
    IsInfo = 1;
    IsDebug = 1;
    IsTrace = 1;
  }
  else {
    IsError = 1;
    IsWarning = 1;
    IsInfo = 1;

    LOG_ERROR("strange log level '%s'. using log level 'info'", level);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the log severity
////////////////////////////////////////////////////////////////////////////////

void TRI_SetLogSeverityLogging (char const* severities) {
  TRI_vector_string_t split;
  size_t i;
  size_t n;

  split = TRI_SplitString(severities, ',');

  IsException = 0;
  IsTechnical = 0;
  IsFunctional = 0;
  IsDevelopment = 0;
  IsUsage = 0;
  IsHuman = 0;

  n = split._length;

  for (i = 0;  i < n;  ++i) {
    char const* type = split._buffer[i];

    if (TRI_CaseEqualString(type, "exception")) {
      IsException = 1;
    }
    else if (TRI_CaseEqualString(type, "technical")) {
      IsTechnical = 1;
    }
    else if (TRI_CaseEqualString(type, "functional")) {
      IsFunctional = 1;
    }
    else if (TRI_CaseEqualString(type, "development")) {
      IsDevelopment = 1;
    }
    else if (TRI_CaseEqualString(type, "usage")) {
      IsUsage = 1;
    }
    else if (TRI_CaseEqualString(type, "human")) {
      IsHuman = 1;
    }
    else if (TRI_CaseEqualString(type, "all")) {
      IsException = 1;
      IsTechnical = 1;
      IsFunctional = 1;
      IsDevelopment = 1;
      IsUsage = 1;
      IsHuman = 1;
    }
    else if (TRI_CaseEqualString(type, "non-human")) {
      IsException = 1;
      IsTechnical = 1;
      IsFunctional = 1;
      IsDevelopment = 1;
      IsUsage = 1;
    }
  }

  TRI_DestroyVectorString(&split);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the output prefix
////////////////////////////////////////////////////////////////////////////////

void TRI_SetPrefixLogging (char const* prefix) {
  TRI_LockSpin(&OutputPrefixLock);

  if (OutputPrefix) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, OutputPrefix);
  }

  OutputPrefix = TRI_DuplicateString(prefix);

  TRI_UnlockSpin(&OutputPrefixLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the thread identifier visibility
////////////////////////////////////////////////////////////////////////////////

void TRI_SetThreadIdentifierLogging (bool show) {
  ShowThreadIdentifier = show ? 1 : 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief use local time?
////////////////////////////////////////////////////////////////////////////////

void TRI_SetUseLocalTimeLogging (bool value) {
  UseLocalTime = value ? 1 : 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the line number visibility
////////////////////////////////////////////////////////////////////////////////

void TRI_SetLineNumberLogging (bool show) {
  ShowLineNumber = show ? 1 : 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the function names visibility
////////////////////////////////////////////////////////////////////////////////

void TRI_SetFunctionLogging (bool show) {
  ShowFunction = show ? 1 : 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the file to log for debug and trace
////////////////////////////////////////////////////////////////////////////////

void TRI_SetFileToLog (char const* file) {
  UseFileBasedLogging = true;
  FilesToLog[TRI_FnvHashString(file) % FilesToLogSize] = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if usage logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsUsageLogging () {
  return IsUsage != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if human logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsHumanLogging () {
  return IsHuman != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if exception logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsExceptionLogging () {
  return IsException != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if technical logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsTechnicalLogging () {
  return IsTechnical != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if functional logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsFunctionalLogging () {
  return IsFunctional != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if development logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsDevelopmentLogging () {
  return IsDevelopment != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if fatal logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsFatalLogging () {
  return IsFatal != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if error logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsErrorLogging () {
  return IsError != 0;

}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if warning logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsWarningLogging () {
  return IsWarning != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if info logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsInfoLogging () {
  return IsInfo != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if debug logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsDebugLogging (char const* file) {
  if (UseFileBasedLogging) {
    if (! IsDebug || file == nullptr) {
      return false;
    }

    while (file[0] == '.' && file[1] == '.' && file[2] == '/') {
      file += 3;
    }

    return FilesToLog[TRI_FnvHashString(file) % FilesToLogSize];
  }
  else {
    return IsDebug != 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if trace logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsTraceLogging (char const* file) {
  if (UseFileBasedLogging) {
    if (! IsTrace || file == nullptr) {
      return false;
    }

    while (file[0] == '.' && file[1] == '.' && file[2] == '/') {
      file += 3;
    }

    return FilesToLog[TRI_FnvHashString(file) % FilesToLogSize];
  }
  else {
    return IsTrace != 0;
  }
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
  TRI_pid_t processId;
  TRI_tid_t threadId;
  va_list ap;

  if (! LoggingActive) {
    return;
  }

  processId = TRI_CurrentProcessId();
  threadId = TRI_CurrentThreadId();

  va_start(ap, fmt);
  LogThread(func, file, line, level, severity, processId, threadId, fmt, ap);
  va_end(ap);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a new raw message
////////////////////////////////////////////////////////////////////////////////

void TRI_RawLog (TRI_log_level_e level,
                 TRI_log_severity_e severity,
                 char const* text,
                 size_t length) {
  OutputMessage(level, severity, const_cast<char*>(text), length, 0, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last log entries
////////////////////////////////////////////////////////////////////////////////

TRI_vector_t* TRI_BufferLogging (TRI_log_level_e level, uint64_t start, bool useUpto) {
  TRI_log_buffer_t buf;
  TRI_vector_t* result;
  size_t i;
  size_t j;
  size_t begin;
  size_t pos;
  size_t cur;

  result = static_cast<TRI_vector_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vector_t), false));

  if (result == nullptr) {
    return nullptr;
  }

  TRI_InitVector(result, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_log_buffer_t));

  begin = 0;
  pos = (size_t) level;

  if (pos >= OUTPUT_LOG_LEVELS) {
    pos = OUTPUT_LOG_LEVELS - 1;
  }

  if (! useUpto) {
    begin = pos;
  }

  TRI_LockMutex(&BufferLock);

  for (i = begin;  i <= pos;  ++i) {
    for (j = 0;  j < OUTPUT_BUFFER_SIZE;  ++j) {
      cur = (BufferCurrent[i] + j) % OUTPUT_BUFFER_SIZE;

      buf = BufferOutput[i][cur];

      if (buf._lid >= start &&
          buf._text != nullptr &&
          *buf._text != '\0') {
        buf._text = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, buf._text);

        if (buf._text != nullptr) {
          TRI_PushBackVector(result, &buf);
        }
      }
    }
  }

  TRI_UnlockMutex(&BufferLock);

  qsort(TRI_BeginVector(result), result->_length, sizeof(TRI_log_buffer_t), LidCompare);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the log buffer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBufferLogging (TRI_vector_t* buffer) {
  for (size_t i = 0;  i < buffer->_length;  ++i) {
    TRI_log_buffer_t* buf = static_cast<TRI_log_buffer_t*>(TRI_AtVector(buffer, i));

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, buf->_text);
  }

  TRI_FreeVector(TRI_UNKNOWN_MEM_ZONE, buffer);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 LOG FILE APPENDER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief structure for file log appenders
////////////////////////////////////////////////////////////////////////////////

typedef struct log_appender_file_s {
  TRI_log_appender_t base;

  char* _filename;
  int _fd;

  TRI_spin_t _lock;
}
log_appender_file_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief writes to a log file
////////////////////////////////////////////////////////////////////////////////

static void WriteLogFile (int fd, char const* buffer, ssize_t len) {
  bool giveUp = false;

  while (len > 0) {
    ssize_t n;

    n = TRI_WRITE(fd, buffer, len);

    if (n < 0) {
      fprintf(stderr, "cannot log data: %s\n", TRI_LAST_ERROR_STR);
      return; // give up, but do not try to log failure
    }
    else if (n == 0) {
      if (! giveUp) {
        giveUp = true;
        continue;
      }
    }

    buffer += n;
    len -= n;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message to a log file appender
////////////////////////////////////////////////////////////////////////////////

static void LogAppenderFile_Log (TRI_log_appender_t* appender,
                                 TRI_log_level_e level,
                                 TRI_log_severity_e severity,
                                 char const* msg,
                                 size_t length) {
  log_appender_file_t* self;
  char* escaped;
  size_t escapedLength;
  int fd;

  self = (log_appender_file_t*) appender;

  TRI_LockSpin(&self->_lock);
  fd = self->_fd;
  TRI_UnlockSpin(&self->_lock);

  if (fd < 0) {
    return;
  }

  if (level == TRI_LOG_LEVEL_FATAL) {
    // a fatal error. always print this on stderr, too.
    size_t i;

    WriteStderr(level, msg);

    // this function is already called when the appenders lock is held
    // no need to lock it again
    for (i = 0;  i < Appenders._length;  ++i) {
      char* details;

      TRI_log_appender_t* a = static_cast<TRI_log_appender_t*>(Appenders._buffer[i]);

      details = a->details(appender);

      if (details != nullptr) {
        WriteStderr(TRI_LOG_LEVEL_INFO, details);
        TRI_Free(TRI_CORE_MEM_ZONE, details);
      }
    }

    if (self->_filename == nullptr &&
        (fd == STDOUT_FILENO || fd == STDERR_FILENO)) {
      // the logfile is either stdout or stderr. no need to print the message again
      return;
    }
  }

  escaped = TRI_EscapeControlsCString(TRI_UNKNOWN_MEM_ZONE, msg, length, &escapedLength, true);

  if (escaped != nullptr) {
    WriteLogFile(fd, escaped, (ssize_t) escapedLength);

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, escaped);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reopens the log files appender
////////////////////////////////////////////////////////////////////////////////

static void LogAppenderFile_Reopen (TRI_log_appender_t* appender) {
  log_appender_file_t* self;
  char* backup;
  int fd;
  int old;

  self = (log_appender_file_t*) appender;

  if (self->_fd < 3) {
    return;
  }

  if (self->_filename == nullptr) {
    return;
  }

  // rename log file
  backup = TRI_Concatenate2String(self->_filename, ".old");

  TRI_UnlinkFile(backup);
  TRI_RenameFile(self->_filename, backup);

  // open new log file
  fd = TRI_CREATE(self->_filename, O_APPEND | O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP);

  if (fd < 0) {
    TRI_RenameFile(backup, self->_filename);
    TRI_FreeString(TRI_CORE_MEM_ZONE, backup);
    return;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, backup);

  TRI_SetCloseOnExitFile(fd);

  TRI_LockSpin(&self->_lock);
  old = self->_fd;
  self->_fd = fd;
  TRI_UnlockSpin(&self->_lock);

  TRI_CLOSE(old);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a log file appender
////////////////////////////////////////////////////////////////////////////////

static void LogAppenderFile_Close (TRI_log_appender_t* appender) {
  log_appender_file_t* self;

  self = (log_appender_file_t*) appender;

  if (self->_fd >= 3) {
    TRI_CLOSE(self->_fd);
  }

  self->_fd = -1;

  if (self->_filename != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, self->_filename);
  }

  if (self->base._contentFilter != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, self->base._contentFilter);
  }

  TRI_DestroySpin(&self->_lock);

  TRI_Free(TRI_CORE_MEM_ZONE, self);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief provide details about the logfile appender
////////////////////////////////////////////////////////////////////////////////

static char* LogAppenderFile_Details (TRI_log_appender_t* appender) {
  log_appender_file_t* self;

  self = (log_appender_file_t*) appender;

  if (self->_filename != nullptr &&
      self->_fd != STDOUT_FILENO &&
      self->_fd != STDERR_FILENO) {
    char buffer[1024];

    snprintf(buffer, sizeof(buffer), "More error details may be provided in the logfile '%s'", self->_filename);

    return TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, buffer);
  }

  return nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a log appender for file output
////////////////////////////////////////////////////////////////////////////////

TRI_log_appender_t* TRI_CreateLogAppenderFile (char const* filename,
                                               char const* contentFilter,
                                               TRI_log_severity_e severityFilter,
                                               bool consume) {
  // no logging
  if (filename == nullptr || *filename == '\0') {
    return nullptr;
  }

  // allocate space
  log_appender_file_t* appender = static_cast<log_appender_file_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(log_appender_file_t), false));

  if (appender == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return nullptr;
  }

  appender->base._type           = APPENDER_TYPE_FILE;
  appender->base._contentFilter  = nullptr;
  appender->base._severityFilter = severityFilter;
  appender->base._consume        = consume;

  if (contentFilter != nullptr) {
    if (nullptr == (appender->base._contentFilter = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, contentFilter))) {
      TRI_Free(TRI_CORE_MEM_ZONE, appender);
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

      return nullptr;
    }
  }

  // logging to stdout
  if (TRI_EqualString(filename, "+")) {
    appender->_filename = nullptr;
    appender->_fd = STDOUT_FILENO;
  }

  // logging to stderr
  else if (TRI_EqualString(filename, "-")) {
    appender->_filename = nullptr;
    appender->_fd = STDERR_FILENO;
  }

  // logging to file
  else {
    appender->_fd = TRI_CREATE(filename, O_APPEND | O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP);

    if (appender->_fd < 0) {
      TRI_Free(TRI_CORE_MEM_ZONE, appender);
      return nullptr;
    }

    TRI_SetCloseOnExitFile(appender->_fd);

    appender->_filename = TRI_DuplicateString(filename);

    if (appender->_filename == nullptr) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      TRI_Free(TRI_CORE_MEM_ZONE, appender);

      return nullptr;
    }
  }

  // set methods
  appender->base.log     = LogAppenderFile_Log;
  appender->base.reopen  = LogAppenderFile_Reopen;
  appender->base.close   = LogAppenderFile_Close;
  appender->base.details = LogAppenderFile_Details;

  // create lock
  TRI_InitSpin(&appender->_lock);

  // and store it
  TRI_LockSpin(&AppendersLock);
  TRI_PushBackVectorPointer(&Appenders, &appender->base);
  TRI_UnlockSpin(&AppendersLock);

  // register the name of the first logfile
  if (LogfileName == nullptr) {
    LogfileName = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, filename);
  }

  // and return base structure
  return &appender->base;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   SYSLOG APPENDER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief structure for syslog appenders
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

typedef struct log_appender_syslog_s {
  TRI_log_appender_t base;
  TRI_mutex_t _mutex;
}
log_appender_syslog_t;

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message to a syslog appender
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

static void LogAppenderSyslog_Log (TRI_log_appender_t* appender,
                                   TRI_log_level_e level,
                                   TRI_log_severity_e severity,
                                   char const* msg,
                                   size_t length) {
  int priority;
  log_appender_syslog_t* self;

  switch (severity) {
    case TRI_LOG_SEVERITY_EXCEPTION: priority = LOG_CRIT;  break;
    case TRI_LOG_SEVERITY_FUNCTIONAL: priority = LOG_NOTICE;  break;
    case TRI_LOG_SEVERITY_USAGE: priority = LOG_INFO;  break;
    case TRI_LOG_SEVERITY_TECHNICAL: priority = LOG_INFO;  break;
    case TRI_LOG_SEVERITY_DEVELOPMENT: priority = LOG_DEBUG;  break;
    default: priority = LOG_DEBUG;  break;
  }

  if (severity == TRI_LOG_SEVERITY_HUMAN) {
    switch (level) {
      case TRI_LOG_LEVEL_FATAL: priority = LOG_CRIT; break;
      case TRI_LOG_LEVEL_ERROR: priority = LOG_ERR; break;
      case TRI_LOG_LEVEL_WARNING: priority = LOG_WARNING; break;
      case TRI_LOG_LEVEL_INFO: priority = LOG_NOTICE; break;
      case TRI_LOG_LEVEL_DEBUG: priority = LOG_INFO; break;
      case TRI_LOG_LEVEL_TRACE: priority = LOG_DEBUG; break;
    }
  }

  self = (log_appender_syslog_t*) appender;

  TRI_LockMutex(&self->_mutex);
  syslog(priority, "%s", msg);
  TRI_UnlockMutex(&self->_mutex);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief reopens a syslog appender
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

static void LogAppenderSyslog_Reopen (TRI_log_appender_t* appender) {
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a syslog appender
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

static void LogAppenderSyslog_Close (TRI_log_appender_t* appender) {
  log_appender_syslog_t* self;

  self = (log_appender_syslog_t*) appender;

  TRI_LockMutex(&self->_mutex);
  closelog();
  TRI_UnlockMutex(&self->_mutex);

  TRI_DestroyMutex(&self->_mutex);

  if (self->base._contentFilter != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, self->base._contentFilter);
  }

  TRI_Free(TRI_CORE_MEM_ZONE, self);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief provide details about the logfile appender
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

static char* LogAppenderSyslog_Details (TRI_log_appender_t* appender) {
  return TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, "More error details may be provided in the syslog");
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a syslog appender
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

TRI_log_appender_t* TRI_CreateLogAppenderSyslog (char const* name,
                                                 char const* facility,
                                                 char const* contentFilter,
                                                 TRI_log_severity_e severityFilter,
                                                 bool consume) {
  log_appender_syslog_t* appender;
  int value;

  TRI_ASSERT(facility != nullptr);
  TRI_ASSERT(*facility != '\0');

  // no logging
  if (name == nullptr || *name == '\0') {
    name = "[arangod]";
  }

  // allocate space
  appender = (log_appender_syslog_t*) TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(log_appender_syslog_t), false);

  if (appender == nullptr) {
    return nullptr;
  }

  appender->base._type           = APPENDER_TYPE_SYSLOG;
  appender->base._contentFilter  = nullptr;
  appender->base._severityFilter = severityFilter;
  appender->base._consume        = consume;

  // set methods
  appender->base.log             = LogAppenderSyslog_Log;
  appender->base.reopen          = LogAppenderSyslog_Reopen;
  appender->base.close           = LogAppenderSyslog_Close;
  appender->base.details         = LogAppenderSyslog_Details;

  if (contentFilter != nullptr) {
    if (nullptr == (appender->base._contentFilter = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, contentFilter))) {
      TRI_Free(TRI_CORE_MEM_ZONE, appender);
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

      return nullptr;
    }
  }

  TRI_InitMutex(&appender->_mutex);

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

  // and open logging, openlog does not have a return value...
  TRI_LockMutex(&appender->_mutex);
  openlog(name, LOG_CONS | LOG_PID, value);
  TRI_UnlockMutex(&appender->_mutex);

  // and store it
  TRI_LockSpin(&AppendersLock);
  TRI_PushBackVectorPointer(&Appenders, &appender->base);
  TRI_UnlockSpin(&AppendersLock);

  // and return base structure
  return &appender->base;
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return global log file name
////////////////////////////////////////////////////////////////////////////////

char const* TRI_GetFilenameLogging () {
  return LogfileName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the logging components
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseLogging (bool threaded) {
  if (Initialised > 0) {
    return;
  }

  Initialised = 1;

  UseFileBasedLogging = false;
  memset(FilesToLog, 0, sizeof(FilesToLog));

  TRI_InitVectorPointer(&Appenders, TRI_CORE_MEM_ZONE);
  TRI_InitMutex(&BufferLock);
  TRI_InitSpin(&OutputPrefixLock);
  TRI_InitSpin(&AppendersLock);

  // logging is now active
  LoggingActive = 1;

  // generate threaded logging?
  ThreadedLogging = threaded;

  if (threaded) {
    TRI_InitCondition(&LogCondition);
    TRI_InitMutex(&LogMessageQueueLock);

    TRI_InitVector(&LogMessageQueue, TRI_CORE_MEM_ZONE, sizeof(log_message_t));

    TRI_InitThread(&LoggingThread);
    TRI_StartThread(&LoggingThread, nullptr, "[logging]", MessageQueueWorker, 0);

    while (LoggingThreadActive == 0) {
      usleep(1000);
    }
  }

  // always close logging at the end
  if (! ShutdownInitalised) {
    atexit((void (*)(void)) TRI_ShutdownLogging);
    ShutdownInitalised = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the logging components
////////////////////////////////////////////////////////////////////////////////

bool TRI_ShutdownLogging (bool clearBuffers) {
  if (Initialised != 1) {
    if (Initialised == 0) {
      return ThreadedLogging;
    }
    
    WriteStderr(TRI_LOG_LEVEL_ERROR, "race condition detected in logger");
    return false;
  }

  Initialised = 1;

  // logging is now inactive (this will terminate the logging thread)
  LoggingActive = 0;

  if (LogfileName != nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, LogfileName);
    LogfileName = 0;
  }

  // join with the logging thread
  if (ThreadedLogging) {
    TRI_LockCondition(&LogCondition);
    TRI_SignalCondition(&LogCondition);
    TRI_UnlockCondition(&LogCondition);

    if (TRI_JoinThread(&LoggingThread) != TRI_ERROR_NO_ERROR) {
      // ignore all errors for now as we cannot log them anywhere...
      // TODO: find some means to signal errors on shutdown
    }

    TRI_DestroyMutex(&LogMessageQueueLock);
    TRI_DestroyVector(&LogMessageQueue);
    TRI_DestroyCondition(&LogCondition);
  }

  // cleanup appenders
  CloseLogging();
  TRI_DestroyVectorPointer(&Appenders);

  // cleanup prefix
  TRI_LockSpin(&OutputPrefixLock);

  if (OutputPrefix != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, OutputPrefix);
    OutputPrefix = nullptr;
  }

  TRI_UnlockSpin(&OutputPrefixLock);

  if (clearBuffers) {
    size_t i, j;

    // cleanup output buffers
    TRI_LockMutex(&BufferLock);

    for (i = 0; i < OUTPUT_LOG_LEVELS; i++) {
      for (j = 0; j < OUTPUT_BUFFER_SIZE; j++) {
        if (BufferOutput[i][j]._text != nullptr) {
          TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, BufferOutput[i][j]._text);
          BufferOutput[i][j]._text = nullptr;
        }
      }
    }

    TRI_UnlockMutex(&BufferLock);
  }


  // cleanup locks
  TRI_DestroySpin(&OutputPrefixLock);
  TRI_DestroySpin(&AppendersLock);
  TRI_DestroyMutex(&BufferLock);

  Initialised = 0;

  return ThreadedLogging;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reopens all log appenders
////////////////////////////////////////////////////////////////////////////////

void TRI_ReopenLogging () {
  TRI_LockSpin(&AppendersLock);

  for (size_t i = 0;  i < Appenders._length;  ++i) {
    TRI_log_appender_t* appender = static_cast<TRI_log_appender_t*>(Appenders._buffer[i]);

    appender->reopen(appender);
  }

  TRI_UnlockSpin(&AppendersLock);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
