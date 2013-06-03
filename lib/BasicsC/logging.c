////////////////////////////////////////////////////////////////////////////////
/// @brief logging macros and functions
///
/// @file
///
/// DISCLAIMER
///
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "win-utils.h"
#endif

#include "BasicsC/logging.h"

#ifdef TRI_ENABLE_SYSLOG
#define SYSLOG_NAMES
#define prioritynames TRI_prioritynames
#define facilitynames TRI_facilitynames
#include <syslog.h>
#endif

#include "BasicsC/files.h"
#include "BasicsC/hashes.h"
#include "BasicsC/locks.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/threads.h"
#include "BasicsC/vector.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                           LOGGING
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief base structure for log appenders
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_log_appender_s {
  void (*log) (struct TRI_log_appender_s*, TRI_log_level_e, TRI_log_severity_e, char const* msg, size_t length);
  void (*reopen) (struct TRI_log_appender_s*);
  void (*close) (struct TRI_log_appender_s*);
}
TRI_log_appender_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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
/// @brief shutdown function already installed
////////////////////////////////////////////////////////////////////////////////

static bool ShutdownInitalised = false;

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
/// @brief thread used for logging
////////////////////////////////////////////////////////////////////////////////

static sig_atomic_t LoggingThreadActive = 0;

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

  pos = (size_t) level;

  if (pos >= OUTPUT_LOG_LEVELS) {
    return;
  }

  TRI_LockMutex(&BufferLock);
  BufferCurrent[pos] = (BufferCurrent[pos] + 1) % OUTPUT_BUFFER_SIZE;
  cur = BufferCurrent[pos];
  buf = &BufferOutput[pos][cur];

  if (buf->_text) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, buf->_text);
  }

  buf->_lid = BufferLID++;
  buf->_level = level;
  buf->_timestamp = timestamp;

  if (length > OUTPUT_MAX_LENGTH) {
    buf->_text = TRI_Allocate(TRI_CORE_MEM_ZONE, OUTPUT_MAX_LENGTH + 1, false);

    memcpy(buf->_text, text, OUTPUT_MAX_LENGTH - 4);
    memcpy(buf->_text + OUTPUT_MAX_LENGTH - 4, " ...", 4);
    // append the \0 byte, otherwise we have potentially unbounded strings
    buf->_text[OUTPUT_MAX_LENGTH] = '\0';
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

void writeStderr (char const* line, size_t len) {
  ssize_t n;

  while (0 < len) {
    n = TRI_WRITE(STDERR_FILENO, line, len);

    if (n <= 0) {
      return;
    }

    line += n;
    len -= n;
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
  if (! LoggingActive) {
    writeStderr(message, length);
    writeStderr("\n", 1);

    if (! copy) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, message);
    }

    return;
  }

  if (severity == TRI_LOG_SEVERITY_HUMAN) {
    // we start copying the message from the given offset to skip any irrelevant
    // or redundant message parts such as date, info etc. The offset might be 0 though.
    assert(length >= offset);
    StoreOutput(level, time(0), message + offset, (size_t) (length - offset));
  }

  TRI_LockSpin(&AppendersLock);

  if (Appenders._length == 0) {
    writeStderr(message, length);
    writeStderr("\n", 1);

    if (! copy) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, message);
    }

    TRI_UnlockSpin(&AppendersLock);
    return;
  }

  TRI_UnlockSpin(&AppendersLock);

  if (ThreadedLogging) {
    log_message_t msg;
    msg._level = level;
    msg._severity = severity;
    msg._message = copy ? TRI_DuplicateString2(message, length) : message;
    msg._length = length;

    // this will COPY the structure log_message_t into the vector
    TRI_LockMutex(&LogMessageQueueLock);
    TRI_PushBackVector(&LogMessageQueue, (void*) &msg);
    TRI_UnlockMutex(&LogMessageQueueLock);
  }
  else {
    size_t i;

    TRI_LockSpin(&AppendersLock);

    for (i = 0;  i < Appenders._length;  ++i) {
      TRI_log_appender_t* appender;

      appender = Appenders._buffer[i];
      appender->log(appender, level, severity, message, length);
    }

    TRI_UnlockSpin(&AppendersLock);

    if (! copy) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, message);
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

        TRI_FreeString(TRI_CORE_MEM_ZONE, msg->_message);
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
    TRI_FreeString(TRI_CORE_MEM_ZONE, msg->_message);
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
  TRI_gmtime(tt, &tb);
  // write time in buffer
  len = strftime(buffer, 32, "%Y-%m-%dT%H:%M:%SZ ", &tb);


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
    char* p;

    // allocate as much memory as we need
    p = TRI_Allocate(TRI_CORE_MEM_ZONE, n + len + 2, false);

    if (p == NULL) {
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
      TRI_Free(TRI_CORE_MEM_ZONE, p);
      TRI_Log(func, file, line, TRI_LOG_LEVEL_WARNING, TRI_LOG_SEVERITY_HUMAN, "format string is corrupt");
      return;
    }
    else if (m > n) {
      TRI_Free(TRI_CORE_MEM_ZONE, p);
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
    TRI_log_appender_t* appender = Appenders._buffer[0];

    TRI_RemoveVectorPointer(&Appenders, 0);
    appender->close(appender);
  }

  TRI_UnlockSpin(&AppendersLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs fatal error, cleans up, and exists
////////////////////////////////////////////////////////////////////////////////

void CLEANUP_LOGGING_AND_EXIT_ON_FATAL_ERROR () {
  TRI_ShutdownLogging();
  TRI_EXIT_FUNCTION(EXIT_FAILURE, NULL);
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
    if (! IsDebug || file == NULL) {
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
    if (! IsTrace || file == NULL) {
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
  OutputMessage(level, severity, CONST_CAST(text), length, 0, true);
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

  result = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_vector_t), false);
  TRI_InitVector(result, TRI_CORE_MEM_ZONE, sizeof(TRI_log_buffer_t));

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

    TRI_FreeString(TRI_CORE_MEM_ZONE, buf->_text);
  }

  TRI_FreeVector(TRI_CORE_MEM_ZONE, buffer);
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

  escaped = TRI_EscapeControlsCString(msg, length, &escapedLength);

  WriteLogFile(fd, escaped, escapedLength);
  WriteLogFile(fd, "\n", 1);

  TRI_FreeString(TRI_CORE_MEM_ZONE, escaped);
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

  if (self->_filename != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, self->_filename);
  }

  TRI_DestroySpin(&self->_lock);

  TRI_Free(TRI_CORE_MEM_ZONE, self);
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
  appender = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(log_appender_file_t), false);
  if (appender == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return NULL;
  }

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
      TRI_Free(TRI_CORE_MEM_ZONE, appender);
      return NULL;
    }

    TRI_SetCloseOnExitFile(appender->_fd);

    appender->_filename = TRI_DuplicateString(filename);
    if (appender->_filename == NULL) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      TRI_Free(TRI_CORE_MEM_ZONE, appender);

      return NULL;
    }
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
  TRI_mutex_t _mutex;
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
  log_appender_syslog_t* self;

  switch (severity) {
    case TRI_LOG_SEVERITY_EXCEPTION: priority = LOG_CRIT;  break;
    case TRI_LOG_SEVERITY_FUNCTIONAL: priority = LOG_NOTICE;  break;
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

  TRI_Free(TRI_CORE_MEM_ZONE, self);
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

  assert(facility);
  assert(*facility != '\0');

  // no logging
  if (name == NULL || *name == '\0') {
    name = "[arangod]";
  }

  // allocate space
  appender = (log_appender_syslog_t*) TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(log_appender_syslog_t), false);

  // set methods
  appender->base.log = LogAppenderSyslog_Log;
  appender->base.reopen = LogAppenderSyslog_Reopen;
  appender->base.close = LogAppenderSyslog_Close;

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
    TRI_InitMutex(&LogMessageQueueLock);

    TRI_InitVector(&LogMessageQueue, TRI_CORE_MEM_ZONE, sizeof(log_message_t));

    TRI_InitThread(&LoggingThread);
    TRI_StartThread(&LoggingThread, "[logging]", MessageQueueWorker, 0);

    while (LoggingThreadActive == 0) {
      usleep(1000);
    }
  }

  // and initialised
  Initialised = true;

  // always close logging at the end
  if (! ShutdownInitalised) {
    atexit((void (*)(void)) TRI_ShutdownLogging);
    ShutdownInitalised = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the logging components
////////////////////////////////////////////////////////////////////////////////

bool TRI_ShutdownLogging () {
  size_t i, j;

  if (! Initialised) {
    return ThreadedLogging;
  }

  // logging is now inactive (this will terminate the logging thread)
  LoggingActive = 0;

  // join with the logging thread
  if (ThreadedLogging) {
    TRI_JoinThread(&LoggingThread);
    TRI_DestroyMutex(&LogMessageQueueLock);
    TRI_DestroyVector(&LogMessageQueue);
  }

  // cleanup appenders
  CloseLogging();
  TRI_DestroyVectorPointer(&Appenders);

  // cleanup prefix
  TRI_LockSpin(&OutputPrefixLock);

  if (OutputPrefix) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, OutputPrefix);
    OutputPrefix = NULL;
  }

  TRI_UnlockSpin(&OutputPrefixLock);

  // cleanup output buffers
  TRI_LockMutex(&BufferLock);

  for (i = 0; i < OUTPUT_LOG_LEVELS; i++) {
    for (j = 0; j < OUTPUT_BUFFER_SIZE; j++) {
      if (BufferOutput[i][j]._text != NULL) {
        TRI_FreeString(TRI_CORE_MEM_ZONE, BufferOutput[i][j]._text);
        BufferOutput[i][j]._text = NULL;
      }
    }
  }

  TRI_UnlockMutex(&BufferLock);

  // cleanup locks
  TRI_DestroySpin(&OutputPrefixLock);
  TRI_DestroySpin(&AppendersLock);
  TRI_DestroyMutex(&BufferLock);

  Initialised = false;

  return ThreadedLogging;
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
