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

#ifndef TRIAGENS_PHILADELPHIA_BASICS_LOGGING_H
#define TRIAGENS_PHILADELPHIA_BASICS_LOGGING_H 1

#include <Basics/Common.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                           LOGGING
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief log levels
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_LOG_FATAL = 1,
  TRI_LOG_ERROR = 2,
  TRI_LOG_WARNING = 3,
  TRI_LOG_INFO = 4,
  TRI_LOG_DEBUG = 5,
  TRI_LOG_TRACE = 6
}
TRI_log_level_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief log severities
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_LOG_SEVERITY_EXCEPTION = 1,
  TRI_LOG_SEVERITY_TECHNICAL = 2,
  TRI_LOG_SEVERITY_FUNCTIONAL = 3,
  TRI_LOG_SEVERITY_DEVELOPMENT = 4,
  TRI_LOG_SEVERITY_HUMAN = 5,
  TRI_LOG_SEVERITY_UNKNOWN = 6
}
TRI_log_severity_e;

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

char const* TRI_GetLogLevelLogging (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the log level
////////////////////////////////////////////////////////////////////////////////

void TRI_SetLogLevelLogging (char const* level);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if human logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsHumanLogging (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if fatal logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsFatalLogging (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if error logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsErrorLogging (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if warning logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsWarningLogging (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if info logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsInfoLogging (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if debug logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsDebugLogging (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if trace logging is enabled
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsTraceLogging (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a new message
////////////////////////////////////////////////////////////////////////////////

void TRI_Log (char const* func, char const* file, int line, TRI_log_level_e, TRI_log_severity_e, char const* fmt, ...);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief logs fatal errors
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOG_FATAL(...)                                                                               \
  do {                                                                                               \
    if (TRI_IsHumanLogging() && TRI_IsFatalLogging()) {                                                            \
      TRI_Log(__FUNCTION__, __FILE__, __LINE__, TRI_LOG_FATAL, TRI_LOG_SEVERITY_HUMAN, __VA_ARGS__); \
    }                                                                                                \
  } while (0)

#else

#define LOG_FATAL(...) /* logging disabled */

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs errors
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOG_ERROR(...)                                                                               \
  do {                                                                                               \
    if (TRI_IsHumanLogging() && TRI_IsErrorLogging()) {                                                            \
      TRI_Log(__FUNCTION__, __FILE__, __LINE__, TRI_LOG_ERROR, TRI_LOG_SEVERITY_HUMAN, __VA_ARGS__); \
    }                                                                                                \
  } while (0)

#else

#define LOG_ERROR(...) /* logging disabled */

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs warnings
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOG_WARNING(...)                                                                               \
  do {                                                                                                 \
    if (TRI_IsHumanLogging() && TRI_IsWarningLogging()) {                                                            \
      TRI_Log(__FUNCTION__, __FILE__, __LINE__, TRI_LOG_WARNING, TRI_LOG_SEVERITY_HUMAN, __VA_ARGS__); \
    }                                                                                                  \
  } while (0)

#else

#define LOG_WARNING(...) /* logging disabled */

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs info messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOG_INFO(...)                                                                               \
  do {                                                                                              \
    if (TRI_IsHumanLogging() && TRI_IsInfoLogging()) {                                                            \
      TRI_Log(__FUNCTION__, __FILE__, __LINE__, TRI_LOG_INFO, TRI_LOG_SEVERITY_HUMAN, __VA_ARGS__); \
    }                                                                                               \
  } while (0)

#else

#define LOG_INFO(...) /* logging disabled */

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs debug messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOG_DEBUG(...)                                                                               \
  do {                                                                                               \
    if (TRI_IsHumanLogging() && TRI_IsDebugLogging()) {                                                            \
      TRI_Log(__FUNCTION__, __FILE__, __LINE__, TRI_LOG_DEBUG, TRI_LOG_SEVERITY_HUMAN, __VA_ARGS__); \
    }                                                                                                \
  } while (0)

#else

#define LOG_DEBUG(...) /* logging disabled */

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs trace messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOG_TRACE(...)                                                                               \
  do {                                                                                               \
    if (TRI_IsHumanLogging() && TRI_IsTraceLogging()) {                                                            \
      TRI_Log(__FUNCTION__, __FILE__, __LINE__, TRI_LOG_TRACE, TRI_LOG_SEVERITY_HUMAN, __VA_ARGS__); \
    }                                                                                                \
  } while (0)

#else

#define LOG_TRACE(...) /* logging disabled */

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      LOG APPENDER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief base structure for log appenders
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_LogAppender_s {
  void (*log) (struct TRI_LogAppender_s*, TRI_log_level_e, TRI_log_severity_e, char const* msg, size_t length);
  void (*close) (struct TRI_LogAppender_s*);
}
TRI_LogAppender_t;

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
/// @brief creates a log append for file output
////////////////////////////////////////////////////////////////////////////////

TRI_LogAppender_t* TRI_LogAppenderFile (char const* filename);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a log append for syslog
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG
TRI_LogAppender_t* TRI_LogAppenderSyslog (char const* name, char const* facility);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a log append for buffering
////////////////////////////////////////////////////////////////////////////////

TRI_LogAppender_t* TRI_LogAppenderBuffer (size_t queueSize, size_t messageSize);

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

void TRI_InitialiseLogging (bool threaded);

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the logging components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownLogging (void);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
