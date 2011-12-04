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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "logging.h"

#ifdef TRI_ENABLE_SYSLOG
#define SYSLOG_NAMES
#define prioritynames TRI_prioritynames
#define facilitynames TRI_facilitynames
#include <syslog.h>
#endif

#include <Basics/files.h>
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
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialised
////////////////////////////////////////////////////////////////////////////////

static bool Initialised = false;

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

static sig_atomic_t IsInfo = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief log debug messages
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsDebug = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief log trace messages
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t IsTrace = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief show line numbers, debug and trace always show the line numbers
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t ShowLineNumber = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief show function names
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t ShowFunction = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief show thread identifier
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t ShowThreadIdentifier = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief output prefix
////////////////////////////////////////////////////////////////////////////////

static char* OutputPrefix = NULL;

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

static bool ThreadedLogging = 0;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief stores output in a buffer
////////////////////////////////////////////////////////////////////////////////

static void StoreOutput (TRI_log_level_e level, time_t timestamp, char const* text, size_t length) {
  TRI_log_buffer_t* buf;
  size_t pos;
  size_t cur;

  TRI_LockMutex(&BufferLock);

  pos = (size_t) level;

  if (pos >= OUTPUT_LOG_LEVELS) {
    TRI_UnlockMutex(&BufferLock);
    return;
  }

  BufferCurrent[pos] = (BufferCurrent[pos] + 1) % OUTPUT_BUFFER_SIZE;
  cur = BufferCurrent[pos];
  buf = &BufferOutput[pos][cur];

  if (buf->_text) {
    TRI_FreeString(buf->_text);
  }

  buf->_lid = BufferLID++;
  buf->_level = level;
  buf->_timestamp = timestamp;

  if (length > OUTPUT_MAX_LENGTH) {
    buf->_text = TRI_Allocate(OUTPUT_MAX_LENGTH + 1);

    memcpy(buf->_text, text, OUTPUT_MAX_LENGTH - 4);
    memcpy(buf->_text + OUTPUT_MAX_LENGTH - 4, " ...", 4);
  }
  else {
    buf->_text = TRI_DuplicateString2(text, length);
  }

  TRI_UnlockMutex(&BufferLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares the lid
////////////////////////////////////////////////////////////////////////////////

static int LidCompare (void const* l, void const* r) {
  TRI_log_buffer_t const* left = l;
  TRI_log_buffer_t const* right = r;

  return ((int64_t) left->_lid) - ((int64_t) right->_lid);
}

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

  TRI_LockSpin(&OutputPrefixLock);

  if (OutputPrefix) {
    n = snprintf(buffer + m, size - m, "%s %s ", s, OutputPrefix);
  }
  else {
    n = snprintf(buffer + m, size - m, "%s ", s);
  }

  TRI_UnlockSpin(&OutputPrefixLock);

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

  if (ShowThreadIdentifier) {
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
    case TRI_LOG_LEVEL_FATAL: ll = "FATAL"; break;
    case TRI_LOG_LEVEL_ERROR: ll = "ERROR"; break;
    case TRI_LOG_LEVEL_WARNING: ll = "WARNING"; break;
    case TRI_LOG_LEVEL_INFO: ll = "INFO"; break;
    case TRI_LOG_LEVEL_DEBUG: ll = "DEBUG"; break;
    case TRI_LOG_LEVEL_TRACE: ll = "TRACE"; break;
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

  sln = ShowLineNumber;

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
  size_t i;

  if (! LoggingActive) {
    if (! copy) {
      TRI_FreeString(message);
    }

    return;
  }

  StoreOutput(level, time(0), message, length);

  if (ThreadedLogging) {
    log_message_t msg;
    msg._level = level;
    msg._severity = severity;
    msg._message = copy ? TRI_DuplicateString2(message, length) : message;
    msg._length = length;

    TRI_LockMutex(&LogMessageQueueLock);
    TRI_PushBackVector(&LogMessageQueue, (void*) &msg);
    TRI_UnlockMutex(&LogMessageQueueLock);
  }
  else {
    TRI_LockSpin(&AppendersLock);

    for (i = 0;  i < Appenders._length;  ++i) {
      TRI_log_appender_t* appender;

      appender = Appenders._buffer[i];
      appender->log(appender, level, severity, message, length);
    }

    TRI_UnlockSpin(&AppendersLock);

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
  size_t j;

  TRI_vector_t buffer;

  TRI_InitVector(&buffer, sizeof(log_message_t));

  sl = 100;

  while (true) {
    TRI_LockMutex(&LogMessageQueueLock);

    if (TRI_EmptyVector(&LogMessageQueue)) {
      TRI_UnlockMutex(&LogMessageQueueLock);
      sl += 1000;
    }
    else {
      size_t m;
      size_t i;

      // move message from queue into temporary buffer
      m = LogMessageQueue._length;

      for (j = 0;  j < m;  ++j) {
        TRI_PushBackVector(&buffer, TRI_AtVector(&LogMessageQueue, j));
      }

      TRI_ClearVector(&LogMessageQueue);

      TRI_UnlockMutex(&LogMessageQueueLock);

      // output messages using the appenders
      for (j = 0;  j < m;  ++j) {
        log_message_t* msg;

        msg = TRI_AtVector(&buffer, j);

        TRI_LockSpin(&AppendersLock);

        for (i = 0;  i < Appenders._length;  ++i) {
          TRI_log_appender_t* appender;

          appender = Appenders._buffer[i];
          appender->log(appender, msg->_level, msg->_severity, msg->_message, msg->_length);
        }

        TRI_UnlockSpin(&AppendersLock);

        TRI_FreeString(msg->_message);
      }

      TRI_ClearVector(&buffer);

      // sleep a little while
      sl = 100;
    }

    if (LoggingActive) {
      usleep(sl);
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
    log_message_t* msg;

    msg = TRI_AtVector(&LogMessageQueue, j);
    TRI_FreeString(msg->_message);
  }

  TRI_ClearVector(&LogMessageQueue);

  TRI_UnlockMutex(&LogMessageQueueLock);
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
    TRI_Log(func, file, line, TRI_LOG_LEVEL_WARNING, TRI_LOG_SEVERITY_HUMAN, "format string is corrupt");
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
        TRI_Log(func, file, line, TRI_LOG_LEVEL_ERROR, TRI_LOG_SEVERITY_HUMAN, "log message is too large (%d bytes)", n);
        return;
      }
      else {
        va_copy(ap2, ap);
        m = GenerateMessage(p, n, func, file, line, level, processId, threadId, fmt, ap2);
        va_end(ap2);

        if (m == -1) {
          TRI_Free(p);
          TRI_Log(func, file, line, TRI_LOG_LEVEL_WARNING, TRI_LOG_SEVERITY_HUMAN, "format string is corrupt");
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
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

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

    LOG_ERROR("strange log level '%s'", level);
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
    else if (TRI_CaseEqualString(type, "human")) {
      IsHuman = 1;
    }
    else if (TRI_CaseEqualString(type, "all")) {
      IsException = 1;
      IsTechnical = 1;
      IsFunctional = 1;
      IsDevelopment = 1;
      IsHuman = 1;
    }
    else if (TRI_CaseEqualString(type, "non-human")) {
      IsException = 1;
      IsTechnical = 1;
      IsFunctional = 1;
      IsDevelopment = 1;
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
    TRI_FreeString(OutputPrefix);
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

bool TRI_IsDebugLogging () {
  return IsDebug != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if trace logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsTraceLogging () {
  return IsTrace != 0;
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
/// @brief logs a new raw message
////////////////////////////////////////////////////////////////////////////////

void TRI_RawLog (TRI_log_level_e level,
                 TRI_log_severity_e severity,
                 char const* text,
                 size_t length) {
  union { char* v; char const* c; } cnv;
  cnv.c = text;

  OutputMessage(level, severity, cnv.v, length, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last log entries
////////////////////////////////////////////////////////////////////////////////

TRI_vector_t* TRI_BufferLogging (TRI_log_level_e level, uint64_t start) {
  TRI_log_buffer_t buf;
  TRI_vector_t* result;
  size_t i;
  size_t j;
  size_t pos;
  size_t cur;

  result = TRI_Allocate(sizeof(TRI_vector_t));
  TRI_InitVector(result, sizeof(TRI_log_buffer_t));

  pos = (size_t) level;

  if (pos >= OUTPUT_LOG_LEVELS) {
    pos = OUTPUT_LOG_LEVELS - 1;
  }

  TRI_LockMutex(&BufferLock);

  for (i = 0;  i <= pos;  ++i) {
    for (j = 0;  j < OUTPUT_BUFFER_SIZE;  ++j) {
      cur = (BufferCurrent[i] + j) % OUTPUT_BUFFER_SIZE;

      buf = BufferOutput[i][cur];

      if (buf._lid >= start && buf._text != NULL && *buf._text != '\0') {
        buf._text = TRI_DuplicateString(buf._text);

        TRI_PushBackVector(result, &buf);
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
  TRI_log_buffer_t* buf;
  size_t i;

  for (i = 0;  i < buffer->_length;  ++i) {
    buf = TRI_AtVector(buffer, i);

    TRI_FreeString(buf->_text);
  }

  TRI_FreeVector(buffer);
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
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
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

  escaped = TRI_EscapeCString(msg, length, &escapedLength);

  WriteLogFile(fd, escaped, escapedLength);
  WriteLogFile(fd, "\n", 1);

  TRI_FreeString(escaped);
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

  if (self->_filename == NULL) {
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
    TRI_FreeString(backup);
    return;
  }

  TRI_FreeString(backup);

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
  size_t i;

  self = (log_appender_file_t*) appender;

  TRI_LockSpin(&AppendersLock);

  for (i = 0;  i < Appenders._length;  ++i) {
    if (appender == Appenders._buffer[i]) {
      TRI_RemoveVectorPointer(&Appenders, i);
      break;
    }
  }

  TRI_UnlockSpin(&AppendersLock);

  if (self->_fd >= 3) {
    TRI_CLOSE(self->_fd);
  }

  self->_fd = -1;

  if (self->_filename != NULL) {
    TRI_FreeString(self->_filename);
  }

  TRI_DestroySpin(&self->_lock);

  TRI_Free(self);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a log appender for file output
////////////////////////////////////////////////////////////////////////////////

TRI_log_appender_t* TRI_CreateLogAppenderFile (char const* filename) {
  log_appender_file_t* appender;

  // no logging
  if (filename == NULL || *filename == '\0') {
    return NULL;
  }

  // allocate space
  appender = TRI_Allocate(sizeof(log_appender_file_t));

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
    appender->_fd = TRI_CREATE(filename, O_APPEND | O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP);

    if (appender->_fd < 0) {
      TRI_Free(appender);
      return NULL;
    }

    appender->_filename = TRI_DuplicateString(filename);
  }

  // set methods
  appender->base.log = LogAppenderFile_Log;
  appender->base.reopen = LogAppenderFile_Reopen;
  appender->base.close = LogAppenderFile_Close;

  // create lock
  TRI_InitSpin(&appender->_lock);

  // and store it
  TRI_LockSpin(&AppendersLock);
  TRI_PushBackVectorPointer(&Appenders, &appender->base);
  TRI_UnlockSpin(&AppendersLock);

  // and return base structure
  return &appender->base;
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
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief structure for syslog appenders
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

typedef struct log_appender_syslog_s {
  TRI_log_appender_t base;
}
log_appender_syslog_t;

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

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
  size_t i;

  TRI_LockSpin(&AppendersLock);

  for (i = 0;  i < Appenders._length;  ++i) {
    if (appender == Appenders._buffer[i]) {
      TRI_RemoveVectorPointer(&Appenders, i);
      break;
    }
  }

  TRI_UnlockSpin(&AppendersLock);

  self = (log_appender_syslog_t*) appender;
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
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a syslog appender
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

TRI_log_appender_t* TRI_CreateLogAppenderSyslog (char const* name, char const* facility) {
  log_appender_syslog_t* appender;
  int value;

  // no logging
  if (name == NULL || *name == '\0') {
    return NULL;
  }

  // allocate space
  appender = (log_appender_syslog_t*) TRI_Allocate(sizeof(log_appender_syslog_t));

  // set methods
  appender->base.log = LogAppenderSyslog_Log;
  appender->base.reopen = LogAppenderSyslog_Reopen;
  appender->base.close = LogAppenderSyslog_Close;

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

  // and store it
  TRI_LockSpin(&AppendersLock);
  TRI_PushBackVectorPointer(&Appenders, &appender->base);
  TRI_UnlockSpin(&AppendersLock);

  // and return base structure
  return &appender->base;
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
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the logging components
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseLogging (bool threaded) {
  if (Initialised) {
    return;
  }

  TRI_InitVectorPointer(&Appenders);
  TRI_InitMutex(&BufferLock);
  TRI_InitSpin(&OutputPrefixLock);
  TRI_InitSpin(&AppendersLock);

  // generate threaded logging
  ThreadedLogging = threaded;

  if (threaded) {
    TRI_InitMutex(&LogMessageQueueLock);

    TRI_InitVector(&LogMessageQueue, sizeof(log_message_t));

    TRI_InitThread(&LoggingThread);
    TRI_StartThread(&LoggingThread, MessageQueueWorker, 0);
  }

  // logging is now active
  LoggingActive = 1;

  // and initialised
  Initialised = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes all log appenders
////////////////////////////////////////////////////////////////////////////////

void TRI_CloseLogging () {
  TRI_log_appender_t* appender;

  appender = NULL;

  TRI_LockSpin(&AppendersLock);

  if (Appenders._length != 0) {
    appender = Appenders._buffer[0];
  }

  TRI_UnlockSpin(&AppendersLock);

  if (appender != NULL) {
    appender->close(appender);
    TRI_CloseLogging();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reopens all log appenders
////////////////////////////////////////////////////////////////////////////////

void TRI_ReopenLogging () {
  size_t i;

  TRI_LockSpin(&AppendersLock);

  for (i = 0;  i < Appenders._length;  ++i) {
    TRI_log_appender_t* appender = Appenders._buffer[i];

    appender->reopen(appender);
  }

  TRI_UnlockSpin(&AppendersLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the logging components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownLogging () {
  if (! Initialised) {
    return;
  }

  // logging is now inactive
  LoggingActive = 0;

  // join with the logging thread
  if (ThreadedLogging) {
    TRI_JoinThread(&LoggingThread);
    TRI_DestroyMutex(&LogMessageQueueLock);
    TRI_DestroyVector(&LogMessageQueue);
  }

  // cleanup appenders
  TRI_CloseLogging();
  TRI_DestroyVectorPointer(&Appenders);

  // cleanup prefix
  TRI_LockSpin(&OutputPrefixLock);

  if (OutputPrefix) {
    TRI_FreeString(OutputPrefix);
    OutputPrefix = NULL;
  }

  TRI_UnlockSpin(&OutputPrefixLock);

  // cleanup locks
  TRI_DestroySpin(&OutputPrefixLock);
  TRI_DestroySpin(&AppendersLock);
  TRI_DestroyMutex(&BufferLock);

  Initialised = false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
