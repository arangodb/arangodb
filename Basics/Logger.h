////////////////////////////////////////////////////////////////////////////////
/// @brief logger wrapper for C logging
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
/// @author Achim Brandt
/// @author Copyright 2007-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_LOGGER_H
#define TRIAGENS_BASICS_LOGGER_H 1

#include <Basics/Common.h>

#include <Basics/logging.h>
#include <Basics/LoggerInfo.h>
#include <Basics/LoggerTiming.h>
#include <Basics/LoggerStream.h>
#include <Basics/Timing.h>

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

#define LOGGER_FATAL                                                              \
  if (TRI_IsHumanLogging() && TRI_IsFatalLogging())                               \
    triagens::basics::Logger::singleton                                           \
    << TRI_LOG_LEVEL_FATAL                                                        \
    << TRI_LOG_SEVERITY_HUMAN                                                     \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_FATAL \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs fatal errors
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_FATAL_I(a)                                                         \
  if (TRI_IsHumanLogging() && TRI_IsFatalLogging())                               \
    triagens::basics::Logger::singleton                                           \
    << (a)                                                                        \
    << TRI_LOG_LEVEL_FATAL                                                        \
    << TRI_LOG_SEVERITY_HUMAN                                                     \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_FATAL_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs errors
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_ERROR                                                              \
  if (TRI_IsHumanLogging() && TRI_IsErrorLogging())                               \
    triagens::basics::Logger::singleton                                           \
    << TRI_LOG_LEVEL_ERROR                                                        \
    << TRI_LOG_SEVERITY_HUMAN                                                     \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_ERROR \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs errors
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_ERROR_I(a)                                                         \
  if (TRI_IsHumanLogging() && TRI_IsErrorLogging())                               \
    triagens::basics::Logger::singleton                                           \
    << (a)                                                                        \
    << TRI_LOG_LEVEL_ERROR                                                        \
    << TRI_LOG_SEVERITY_HUMAN                                                     \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_ERROR_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs warnings
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_WARNING                                                              \
  if (TRI_IsHumanLogging() && TRI_IsWarningLogging())                               \
    triagens::basics::Logger::singleton                                             \
    << TRI_LOG_LEVEL_WARNING                                                        \
    << TRI_LOG_SEVERITY_HUMAN                                                       \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_WARNING \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs warnings
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_WARNING_I(a)                                                         \
  if (TRI_IsHumanLogging() && TRI_IsWarningLogging())                               \
    triagens::basics::Logger::singleton                                             \
    << (a)                                                                          \
    << TRI_LOG_LEVEL_WARNING                                                        \
    << TRI_LOG_SEVERITY_HUMAN                                                       \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_WARNING_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs info messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_INFO                                                              \
  if (TRI_IsHumanLogging() && TRI_IsInfoLogging())                               \
    triagens::basics::Logger::singleton                                          \
    << TRI_LOG_LEVEL_INFO                                                        \
    << TRI_LOG_SEVERITY_HUMAN                                                    \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_INFO \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs info messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_INFO_I(a)                                                         \
  if (TRI_IsHumanLogging() && TRI_IsInfoLogging())                               \
    triagens::basics::Logger::singleton                                          \
    << (a)                                                                       \
    << TRI_LOG_LEVEL_INFO                                                        \
    << TRI_LOG_SEVERITY_HUMAN                                                    \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_INFO_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs debug messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_DEBUG                                                              \
  if (TRI_IsHumanLogging() && TRI_IsDebugLogging())                               \
    triagens::basics::Logger::singleton                                           \
    << TRI_LOG_LEVEL_DEBUG                                                        \
    << TRI_LOG_SEVERITY_HUMAN                                                     \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_DEBUG \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs debug messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_DEBUG_I(a)                                                         \
  if (TRI_IsHumanLogging() && TRI_IsDebugLogging())                               \
    triagens::basics::Logger::singleton                                           \
    << (a)                                                                        \
    << TRI_LOG_LEVEL_DEBUG                                                        \
    << TRI_LOG_SEVERITY_HUMAN                                                     \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_DEBUG_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs trace messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_TRACE                                                              \
  if (TRI_IsHumanLogging() && TRI_IsTraceLogging())                               \
    triagens::basics::Logger::singleton                                           \
    << TRI_LOG_LEVEL_TRACE                                                        \
    << TRI_LOG_SEVERITY_HUMAN                                                     \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_TRACE \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs trace messages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_TRACE_I(a)                                                         \
  if (TRI_IsHumanLogging() && TRI_IsTraceLogging())                               \
    triagens::basics::Logger::singleton                                           \
    << (a)                                                                        \
    << TRI_LOG_LEVEL_TRACE                                                        \
    << TRI_LOG_SEVERITY_HUMAN                                                     \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_TRACE_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs non-human messsages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define VOC_LOGGER(severity)                                                   \
  if (TRI_Is ## severity ## Logging ())                                        \
    triagens::basics::Logger::singleton                                        \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define VOC_LOGGER(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief logs non-human messsages
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define VOC_LOGGER_I(severity, a)                                              \
  if (TRI_Is ## severity ## Logging ())                                        \
    triagens::basics::Logger::singleton                                        \
    << (a)                                                                     \
    << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define VOC_LOGGER_I(a,b) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-in start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_IN_START                                                \
  VOC_LOGGER(Technical)                                                        \
    << TRI_LOG_CATEGORY_REQUEST_IN_START

#else

#define LOGGER_REQUEST_IN_START \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-in start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_IN_START_I(a)                                           \
  VOC_LOGGER_I(Technical, a)                                                   \
    << TRI_LOG_CATEGORY_REQUEST_IN_START

#else

#define LOGGER_REQUEST_IN_START_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-in end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_IN_END                                                  \
  VOC_LOGGER(Technical)                                                        \
    << TRI_LOG_CATEGORY_REQUEST_IN_END

#else

#define LOGGER_REQUEST_IN_END \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-in end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_IN_END_I(a)                                             \
  VOC_LOGGER_I(Technical, a)                                                   \
    << TRI_LOG_CATEGORY_REQUEST_IN_END

#else

#define LOGGER_REQUEST_IN_END_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-out start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_OUT_START                                               \
  VOC_LOGGER(Technical)                                                        \
    << TRI_LOG_CATEGORY_REQUEST_OUT_START

#else

#define LOGGER_REQUEST_OUT_START \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-out start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_OUT_START_I(a)                                          \
  VOC_LOGGER_I(Technical, a)                                                   \
    << TRI_LOG_CATEGORY_REQUEST_OUT_START

#else

#define LOGGER_REQUEST_OUT_START_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-out end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_OUT_END                                                 \
  VOC_LOGGER(Technical)                                                        \
    << TRI_LOG_CATEGORY_REQUEST_OUT_END

#else

#define LOGGER_REQUEST_OUT_END \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief request-out end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_REQUEST_OUT_END_I(a)                                            \
  VOC_LOGGER_I(Technical, a)                                                   \
    << TRI_LOG_CATEGORY_REQUEST_OUT_END

#else

#define LOGGER_REQUEST_OUT_END_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief module in start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_MODULE_IN_START                                                 \
  VOC_LOGGER(Technical)                                                        \
    << TRI_LOG_CATEGORY_MODULE_IN_START

#else

#define LOGGER_MODULE_IN_START \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief module in start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_MODULE_IN_START_I(a)                                            \
  VOC_LOGGER_I(Technical, a)                                                   \
    << TRI_LOG_CATEGORY_MODULE_IN_START

#else

#define LOGGER_MODULE_IN_START_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief module in end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_MODULE_IN_END                                                   \
  VOC_LOGGER(Technical)                                                        \
    << TRI_LOG_CATEGORY_MODULE_IN_END

#else

#define LOGGER_MODULE_IN_END \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief module in end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_MODULE_IN_END_I(a)                                              \
  VOC_LOGGER_I(Technical, a)                                                   \
    << TRI_LOG_CATEGORY_MODULE_IN_END

#else

#define LOGGER_MODULE_IN_END_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function in start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_FUNCTION_IN_START                                               \
  VOC_LOGGER(Technical)                                                        \
    << TRI_LOG_CATEGORY_FUNCTION_IN_START

#else

#define LOGGER_FUNCTION_IN_START \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function in start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_FUNCTION_IN_START_I(a)                                          \
  VOC_LOGGER_I(Technical, a)                                                   \
    << TRI_LOG_CATEGORY_FUNCTION_IN_START

#else

#define LOGGER_FUNCTION_IN_START_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function in end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_FUNCTION_IN_END                                                 \
  VOC_LOGGER(Technical)                                                        \
    << TRI_LOG_CATEGORY_FUNCTION_IN_END

#else

#define LOGGER_FUNCTION_IN_END \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function in end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_FUNCTION_IN_END_I(a)                                            \
  VOC_LOGGER_I(Technical, a)                                                   \
    << TRI_LOG_CATEGORY_FUNCTION_IN_END

#else

#define LOGGER_FUNCTION_IN_END_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function step
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_STEP                                                            \
  VOC_LOGGER(Development)                                                      \
  << TRI_LOG_CATEGORY_STEP                                                     \
  << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_STEP \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function step
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_STEP_I(a)                                                       \
  VOC_LOGGER_I(Development, a)                                                 \
  << TRI_LOG_CATEGORY_STEP                                                     \
  << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_STEP_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function loop
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_LOOP                                                            \
  VOC_LOGGER(Development)                                                      \
  << TRI_LOG_CATEGORY_LOOP                                                     \
  << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_LOOP \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief function loop
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_LOOP_I(a)                                                       \
  VOC_LOGGER_I(Development, a)                                                 \
  << TRI_LOG_CATEGORY_LOOP                                                     \
  << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_LOOP_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_HEARTBEAT                                                       \
  VOC_LOGGER(Technical)                                                        \
  << TRI_LOG_CATEGORY_HEARTBEAT                                                \
  << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_HEARTBEAT \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_HEARTBEAT_I(a)                                                  \
  VOC_LOGGER_I(Technical, a)                                                   \
  << TRI_LOG_CATEGORY_HEARTBEAT                                                \
  << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_HEARTBEAT_I(a) \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief heartpulse
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_HEARTPULSE                                                      \
  VOC_LOGGER(Development)                                                      \
  << TRI_LOG_CATEGORY_HEARTPULSE                                               \
  << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_HEARTPULSE \
  if (false) triagens::basics::Logger::singleton

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief heartpulse
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_LOGGER

#define LOGGER_HEARTPULSE_I(a)                                                 \
  VOC_LOGGER_I(Development, a)                                                 \
  << TRI_LOG_CATEGORY_HEARTPULSE                                               \
  << triagens::basics::LoggerData::Position(__FUNCTION__, __FILE__, __LINE__)

#else

#define LOGGER_HEARTPULSE_I(a) \
  if (false) triagens::basics::Logger::singleton

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
/// @brief creates a log appender for file output
////////////////////////////////////////////////////////////////////////////////

TRI_log_appender_t* TRI_CreateLogAppenderFile (std::string const& filename);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a syslog appender
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_SYSLOG

TRI_log_appender_t* TRI_CreateLogAppenderSyslog (std::string const& name, std::string const& facility);

#endif

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

        static Logger singleton;

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
          LoggerStream stream(value.info);
          return stream;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief delegates work to the underlying logger stream
////////////////////////////////////////////////////////////////////////////////

        LoggerStream operator<< (LoggerTiming& value) {
          value.measure();
          LoggerStream stream(value.info);
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

        Logger () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a logger
////////////////////////////////////////////////////////////////////////////////

        ~Logger () {
        }
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
