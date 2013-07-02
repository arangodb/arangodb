////////////////////////////////////////////////////////////////////////////////
/// @brief logger wrapper for C logging
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
/// @author Achim Brandt
/// @author Copyright 2007-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_LOGGER_LOGGER_H
#define TRIAGENS_LOGGER_LOGGER_H 1

#include "Basics/Common.h"

#include "Basics/Timing.h"
#include "BasicsC/logging.h"
#include "Logger/LoggerInfo.h"
#include "Logger/LoggerStream.h"
#include "Logger/LoggerTiming.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief logs fatal errors
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_FATAL_AND_EXIT(a)                                                  \
  do {                                                                            \
    if (TRI_IsHumanLogging() && TRI_IsFatalLogging()) {                           \
      triagens::basics::Logger::_singleton                                        \
      << TRI_LOG_LEVEL_FATAL                                                      \
      << TRI_LOG_SEVERITY_HUMAN                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
    CLEANUP_LOGGING_AND_EXIT_ON_FATAL_ERROR();                                    \
  }                                                                               \
  while (0)

#else

#define LOGGER_FATAL_AND_EXIT(a)                                                  \
  do {                                                                            \
    fprintf(stderr, "fatal error. exiting.\n");                                   \
    CLEANUP_LOGGING_AND_EXIT_ON_FATAL_ERROR();                                    \
  }                                                                               \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs fatal errors
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_FATAL_AND_EXIT_I(i,a)                                              \
  do {                                                                            \
    if (TRI_IsHumanLogging() && TRI_IsFatalLogging()) {                           \
      triagens::basics::Logger::_singleton                                        \
      << (i)                                                                      \
      << TRI_LOG_LEVEL_FATAL                                                      \
      << TRI_LOG_SEVERITY_HUMAN                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
    CLEANUP_LOGGING_AND_EXIT_ON_FATAL_ERROR();                                    \
  }                                                                               \
  while (0)

#else

#define LOGGER_FATAL_AND_EXIT_I(i,a)                                              \
  CLEANUP_LOGGING_AND_EXIT_ON_FATAL_ERROR()

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs errors
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_ERROR(a)                                                           \
  do {                                                                            \
    if (TRI_IsHumanLogging() && TRI_IsErrorLogging()) {                           \
      triagens::basics::Logger::_singleton                                        \
      << TRI_LOG_LEVEL_ERROR                                                      \
      << TRI_LOG_SEVERITY_HUMAN                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define LOGGER_ERROR(a)                                                           \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs errors
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_ERROR_I(i,a)                                                       \
  do {                                                                            \
    if (TRI_IsHumanLogging() && TRI_IsErrorLogging()) {                           \
      triagens::basics::Logger::_singleton                                        \
      << (i)                                                                      \
      << TRI_LOG_LEVEL_ERROR                                                      \
      << TRI_LOG_SEVERITY_HUMAN                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define LOGGER_ERROR_I(i,a)                                                       \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs warnings
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_WARNING(a)                                                         \
  do {                                                                            \
    if (TRI_IsHumanLogging() && TRI_IsWarningLogging()) {                         \
      triagens::basics::Logger::_singleton                                        \
      << TRI_LOG_LEVEL_WARNING                                                    \
      << TRI_LOG_SEVERITY_HUMAN                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define LOGGER_WARNING(a)                                                         \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs warnings
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_WARNING_I(i,a)                                                     \
  do {                                                                            \
    if (TRI_IsHumanLogging() && TRI_IsWarningLogging()) {                         \
      triagens::basics::Logger::_singleton                                        \
      << (i)                                                                      \
      << TRI_LOG_LEVEL_WARNING                                                    \
      << TRI_LOG_SEVERITY_HUMAN                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define LOGGER_WARNING_I(i,a)                                                     \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs info messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_INFO(a)                                                            \
  do {                                                                            \
    if (TRI_IsHumanLogging() && TRI_IsInfoLogging()) {                            \
      triagens::basics::Logger::_singleton                                        \
      << TRI_LOG_LEVEL_INFO                                                       \
      << TRI_LOG_SEVERITY_HUMAN                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define LOGGER_INFO(a)                                                            \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs info messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_INFO_I(i,a)                                                        \
  do {                                                                            \
    if (TRI_IsHumanLogging() && TRI_IsInfoLogging()) {                            \
      triagens::basics::Logger::_singleton                                        \
      << (i)                                                                      \
      << TRI_LOG_LEVEL_INFO                                                       \
      << TRI_LOG_SEVERITY_HUMAN                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define LOGGER_INFO_I(i,a)                                                        \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs debug messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_DEBUG(a)                                                           \
  do {                                                                            \
    if (TRI_IsHumanLogging() && TRI_IsDebugLogging(__FILE__)) {                   \
      triagens::basics::Logger::_singleton                                        \
      << TRI_LOG_LEVEL_DEBUG                                                      \
      << TRI_LOG_SEVERITY_HUMAN                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define LOGGER_DEBUG(a)                                                           \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs debug messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_DEBUG_I(i,a)                                                       \
  do {                                                                            \
    if (TRI_IsHumanLogging() && TRI_IsDebugLogging(__FILE__)) {                   \
      triagens::basics::Logger::_singleton                                        \
      << (i)                                                                      \
      << TRI_LOG_LEVEL_DEBUG                                                      \
      << TRI_LOG_SEVERITY_HUMAN                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define LOGGER_DEBUG_I(i,a)                                                       \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs trace messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_TRACE(a)                                                           \
  do {                                                                            \
    if (TRI_IsHumanLogging() && TRI_IsTraceLogging(__FILE__)) {                   \
      triagens::basics::Logger::_singleton                                        \
      << TRI_LOG_LEVEL_TRACE                                                      \
      << TRI_LOG_SEVERITY_HUMAN                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define LOGGER_TRACE(a)                                                           \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs trace messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_TRACE_I(i,a)                                                       \
  do {                                                                            \
    if (TRI_IsHumanLogging() && TRI_IsTraceLogging(__FILE__)) {                   \
      triagens::basics::Logger::_singleton                                        \
      << (i)                                                                      \
      << TRI_LOG_LEVEL_TRACE                                                      \
      << TRI_LOG_SEVERITY_HUMAN                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define LOGGER_TRACE_I(i,a)                                                       \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs usage messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_USAGE(a)                                                           \
  do {                                                                            \
    if (TRI_IsUsageLogging()) {                                                   \
      triagens::basics::Logger::_singleton                                        \
      << TRI_LOG_LEVEL_INFO                                                       \
      << TRI_LOG_SEVERITY_USAGE                                                   \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define LOGGER_USAGE(a)                                                           \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs non-human messsages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define VOC_LOGGER(severity,a)                                                    \
  do {                                                                            \
    if (TRI_Is ## severity ## Logging ()) {                                       \
      triagens::basics::Logger::_singleton                                        \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define VOC_LOGGER(severity,a)                                                    \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs non-human messsages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define VOC_LOGGER_I(severity,i,a)                                                \
  do {                                                                            \
    if (TRI_Is ## severity ## Logging ()) {                                       \
      triagens::basics::Logger::_singleton                                        \
      << (i)                                                                      \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a;                                                                       \
    }                                                                             \
  }                                                                               \
  while (0)

#else

#define VOC_LOGGER_I(severity,i,a)                                                \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-in start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_IN_START(a)                                                \
  do {                                                                            \
    VOC_LOGGER(Technical, TRI_LOG_CATEGORY_REQUEST_IN_START << a);                \
  }                                                                               \
  while (0)

#else

#define LOGGER_REQUEST_IN_START(a)                                                \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-in start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_IN_START_I(i,a)                                            \
  do {                                                                            \
    VOC_LOGGER_I(Technical, i, TRI_LOG_CATEGORY_REQUEST_IN_START << a);           \
  }                                                                               \
  while (0)

#else

#define LOGGER_REQUEST_IN_START_I(i,a)                                            \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-in end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_IN_END(a)                                                  \
  do {                                                                            \
    VOC_LOGGER(Technical, TRI_LOG_CATEGORY_REQUEST_IN_END << a);                  \
  }                                                                               \
  while (0)

#else

#define LOGGER_REQUEST_IN_END(a)                                                  \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-in end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_IN_END_I(i,a)                                              \
  do {                                                                            \
    VOC_LOGGER_I(Technical, i, TRI_LOG_CATEGORY_REQUEST_IN_END << a);             \
  }                                                                               \
  while (0)

#else

#define LOGGER_REQUEST_IN_END_I(i,a)                                              \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-out start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_OUT_START(a)                                               \
  do {                                                                            \
    VOC_LOGGER(Technical, TRI_LOG_CATEGORY_REQUEST_OUT_START << a);               \
  }                                                                               \
  while (0)

#else

#define LOGGER_REQUEST_OUT_START(a)                                               \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-out start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_OUT_START_I(i,a)                                           \
  do {                                                                            \
    VOC_LOGGER_I(Technical, i, TRI_LOG_CATEGORY_REQUEST_OUT_START << a);          \
  }                                                                               \
  while (0)

#else

#define LOGGER_REQUEST_OUT_START_I(i,a)                                           \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-out end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_OUT_END(a)                                                 \
  do {                                                                            \
    VOC_LOGGER(Technical, TRI_LOG_CATEGORY_REQUEST_OUT_END << a);                 \
  }                                                                               \
  while (0)

#else

#define LOGGER_REQUEST_OUT_END(a)                                                 \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-out end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_OUT_END_I(i,a)                                             \
  do {                                                                            \
    VOC_LOGGER_I(Technical, i, TRI_LOG_CATEGORY_REQUEST_OUT_END << a);            \
  }                                                                               \
  while (0)

#else

#define LOGGER_REQUEST_OUT_END_I(i,a)                                             \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief module in start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_MODULE_IN_START(a)                                                 \
  do {                                                                            \
    VOC_LOGGER(Technical, TRI_LOG_CATEGORY_MODULE_IN_START << a);                 \
  }                                                                               \
  while (0)

#else

#define LOGGER_MODULE_IN_START(a)                                                 \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief module in start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_MODULE_IN_START_I(i,a)                                             \
  do {                                                                            \
    VOC_LOGGER_I(Technical, i, TRI_LOG_CATEGORY_MODULE_IN_START << a);            \
  }                                                                               \
  while (0)

#else

#define LOGGER_MODULE_IN_START_I(i,a)                                             \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief module in end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_MODULE_IN_END(a)                                                   \
  do {                                                                            \
    VOC_LOGGER(Technical, TRI_LOG_CATEGORY_MODULE_IN_END  << a);                  \
  }                                                                               \
  while (0)

#else

#define LOGGER_MODULE_IN_END(a)                                                   \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief module in end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_MODULE_IN_END_I(i,a)                                               \
  do {                                                                            \
    VOC_LOGGER_I(Technical, i, TRI_LOG_CATEGORY_MODULE_IN_END << a);              \
  }                                                                               \
  while (0)

#else

#define LOGGER_MODULE_IN_END_I(i,a)                                               \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function in start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_FUNCTION_IN_START(a)                                               \
  do {                                                                            \
    VOC_LOGGER(Technical, TRI_LOG_CATEGORY_FUNCTION_IN_START << a);               \
  }                                                                               \
  while (0)

#else

#define LOGGER_FUNCTION_IN_START(a)                                               \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function in start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_FUNCTION_IN_START_I(i,a)                                           \
  do {                                                                            \
    VOC_LOGGER_I(Technical, i, TRI_LOG_CATEGORY_FUNCTION_IN_START << a);          \
  }                                                                               \
  while (0)

#else

#define LOGGER_FUNCTION_IN_START_I(i,a)                                           \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function in end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_FUNCTION_IN_END(a)                                                 \
  do {                                                                            \
    VOC_LOGGER(Technical, TRI_LOG_CATEGORY_FUNCTION_IN_END << a);                 \
  }                                                                               \
  while (0)

#else

#define LOGGER_FUNCTION_IN_END(a)                                                 \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function in end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_FUNCTION_IN_END_I(i,a)                                             \
  do {                                                                            \
    VOC_LOGGER_I(Technical, i, TRI_LOG_CATEGORY_FUNCTION_IN_END << a);            \
  }                                                                               \
  while (0)

#else

#define LOGGER_FUNCTION_IN_END_I(i,a)                                             \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function step
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_STEP(a)                                                            \
  do {                                                                            \
    VOC_LOGGER(Development, TRI_LOG_CATEGORY_STEP                                 \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)   \
    << a);                                                                        \
  }                                                                               \
  while (0)

#else

#define LOGGER_STEP(a)                                                            \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function step
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_STEP_I(i,a)                                                        \
  do {                                                                            \
    VOC_LOGGER_I(Development, i, TRI_LOG_CATEGORY_STEP                            \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a);                                                                      \
  }                                                                               \
  while (0)

#else

#define LOGGER_STEP_I(i,a)                                                        \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function loop
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_LOOP(a)                                                            \
  do {                                                                            \
    VOC_LOGGER(Development, TRI_LOG_CATEGORY_LOOP                                 \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a);                                                                      \
  }                                                                               \
  while (0)

#else

#define LOGGER_LOOP(a)                                                            \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function loop
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_LOOP_I(i,a)                                                        \
  do {                                                                            \
    VOC_LOGGER_I(Development, i, TRI_LOG_CATEGORY_LOOP                            \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a);                                                                      \
  }                                                                               \
  while (0)

#else

#define LOGGER_LOOP_I(i,a)                                                        \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_HEARTBEAT(a)                                                       \
  do {                                                                            \
    VOC_LOGGER(Technical, TRI_LOG_CATEGORY_HEARTBEAT                              \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a);                                                                      \
  }                                                                               \
  while (0)

#else

#define LOGGER_HEARTBEAT(a)                                                       \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_HEARTBEAT_I(i,a)                                                   \
  do {                                                                            \
    VOC_LOGGER_I(Technical, i, TRI_LOG_CATEGORY_HEARTBEAT                         \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a);                                                                      \
  }                                                                               \
  while (0)

#else

#define LOGGER_HEARTBEAT_I(i,a)                                                   \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief heartpulse
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_HEARTPULSE(a)                                                      \
  do {                                                                            \
    VOC_LOGGER(Development, TRI_LOG_CATEGORY_HEARTPULSE                           \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a);                                                                      \
  }                                                                               \
  while (0)

#else

#define LOGGER_HEARTPULSE(a)                                                      \
  while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief heartpulse
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_HEARTPULSE_I(i,a)                                                  \
  do {                                                                            \
    VOC_LOGGER_I(Development, i, TRI_LOG_CATEGORY_HEARTPULSE                      \
      << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__) \
      << a);                                                                      \
  }                                                                               \
  while (0)

#else

#define LOGGER_HEARTPULSE_I(i,a)                                                  \
  while (0)

#endif

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
/// @brief sets the log level
////////////////////////////////////////////////////////////////////////////////

void TRI_SetLogLevelLogging (std::string const& level);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the log severity
////////////////////////////////////////////////////////////////////////////////

void TRI_SetLogSeverityLogging (std::string const& severities);

////////////////////////////////////////////////////////////////////////////////
/// @brief defines an output prefix
////////////////////////////////////////////////////////////////////////////////

void TRI_SetPrefixLogging (std::string const& prefix);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the file to log for debug and trace
////////////////////////////////////////////////////////////////////////////////

void TRI_SetFileToLog (std::string const& file);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      class Logger
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief logger
///
/// This class provides various static members which can be used as logging
/// streams. Output to the logging stream is appended by using the operator <<,
/// as soon as a line is completed endl should be used to flush the stream.
/// Each line of output is prefixed by some informational data.
////////////////////////////////////////////////////////////////////////////////

    class Logger {
      friend class LoggerStream;
      Logger (Logger const&);
      Logger& operator= (Logger const&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           static public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief global logger
////////////////////////////////////////////////////////////////////////////////

        static Logger _singleton;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the application name
////////////////////////////////////////////////////////////////////////////////

        static void setApplicationName (string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the facility
////////////////////////////////////////////////////////////////////////////////

        static void setFacility (string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the host name
////////////////////////////////////////////////////////////////////////////////

        static void setHostName (string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the log format
///
/// If the log output is machine (or machine readable), then this parameter
/// allows you to configure the format of the output. The following placeholders
/// are available:
///
/// - @LIT{\%A} the application name
/// - @LIT{\%C} the category which caused the output (e.g. FATAL, ERROR, WARNING etc.)
/// - @LIT{\%E} extras
/// - @LIT{\%F} facility
/// - @LIT{\%H} the host name to log
/// - @LIT{\%K} task
/// - @LIT{\%M} message identifier
/// - @LIT{\%P} peg
/// - @LIT{\%S} severity
/// - @LIT{\%T} date/time stamp
/// - @LIT{\%U} measure unit
/// - @LIT{\%V} measure value
/// - @LIT{\%Z} date/time stamp in GMT (zulu)
/// - @LIT{\%f} source code module
/// - @LIT{\%l} source code line
/// - @LIT{\%m} source code method
/// - @LIT{\%p} pid
/// - @LIT{\%s} pthread identifier
/// - @LIT{\%t} tid
/// - @LIT{\%u} user identifier
/// - @LIT{\%x} the actual text
///
/// The default format is
///
/// @LIT{\%Z;1;\%S;\%C;\%H;\%p-\%t;\%F;\%A;\%f;\%m;\%K;\%f:\%l;\%x;\%P;\%u;\%V;\%U;\%E}
////////////////////////////////////////////////////////////////////////////////

        static void setLogFormat (string const& format);

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs a log message
////////////////////////////////////////////////////////////////////////////////

        static void output (string const& msg, LoggerData::Info const& info);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief delegates work to the underlying logger stream
////////////////////////////////////////////////////////////////////////////////

        template<typename T>
        LoggerStream operator<< (T const& value) {
          LoggerStream stream;

          stream << value;

          return stream;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief delegates work to the underlying logger stream
////////////////////////////////////////////////////////////////////////////////

        LoggerStream operator<< (LoggerInfo& value) {
          LoggerStream stream(value._info);
          return stream;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief delegates work to the underlying logger stream
////////////////////////////////////////////////////////////////////////////////

        LoggerStream operator<< (LoggerTiming& value) {
          value.measure();
          LoggerStream stream(value._info);
          return stream;
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

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new logger
////////////////////////////////////////////////////////////////////////////////

        Logger ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a logger
////////////////////////////////////////////////////////////////////////////////

        ~Logger ();
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
