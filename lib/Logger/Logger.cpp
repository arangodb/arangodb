////////////////////////////////////////////////////////////////////////////////
/// @brief logger
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

#include "Logger.h"

#ifdef TRI_ENABLE_SYSLOG
#define SYSLOG_NAMES
#include <syslog.h>
#endif

#include <fstream>

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"

using namespace triagens::basics;
using namespace std;

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

void TRI_SetLogLevelLogging (string const& level) {
  TRI_SetLogLevelLogging(level.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the log severity
////////////////////////////////////////////////////////////////////////////////

void TRI_SetLogSeverityLogging (string const& severities) {
  TRI_SetLogSeverityLogging(severities.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines an output prefix
////////////////////////////////////////////////////////////////////////////////

void TRI_SetPrefixLogging (string const& prefix) {
  TRI_SetPrefixLogging(prefix.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the file to log for debug and trace
////////////////////////////////////////////////////////////////////////////////

void TRI_SetFileToLog (string const& file) {
  TRI_SetFileToLog(file.c_str());
}

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
/// @brief logger format
////////////////////////////////////////////////////////////////////////////////

static string LoggerFormat = "%Z;1;%S;%C;%H;%p-%t;%F;%A;%f;%m;%K;%f:%l;%x;%P;%u;%V;%U;%E";

////////////////////////////////////////////////////////////////////////////////
/// @brief logger format for raw logging
////////////////////////////////////////////////////////////////////////////////

static string LoggerRawFormat = "\"%Z\"%x";

////////////////////////////////////////////////////////////////////////////////
/// @brief special characters which must be escaped
////////////////////////////////////////////////////////////////////////////////

static string const SpecialCharacters = ";%\r\t\n";

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
/// @brief outputs machine format
////////////////////////////////////////////////////////////////////////////////

static void OutputMachine (string const& text, LoggerData::Info const& info) {
  char const* format; 
  char const* end; 
  
  if (info._severity == TRI_LOG_SEVERITY_USAGE) {
    format = LoggerRawFormat.c_str();
    end    = format + LoggerRawFormat.size();
  }
  else {
    format = LoggerFormat.c_str();
    end    = format + LoggerFormat.size();
  }

  time_t tt = time(0);

  StringBuffer line(TRI_CORE_MEM_ZONE);

  for (;  format < end;  ++format) {
    if (*format == '%') {
      ++format;

      switch (*format) {

        // .............................................................................
        // end-of-line
        // .............................................................................

        case '\0':
          --format;
          break;

        // .............................................................................
        // application name
        // .............................................................................

        case 'A': {
          line.appendText(info._applicationName._name);
          break;
        }

        // .............................................................................
        // category
        // .............................................................................

        case 'C': {
          if (info._severity == TRI_LOG_SEVERITY_FUNCTIONAL && ! info._functional._name.empty()) {
            line.appendText(info._functional._name);
          }
          else {
            switch (info._category) {
              case TRI_LOG_CATEGORY_FATAL: line.appendText("FATAL"); break;
              case TRI_LOG_CATEGORY_ERROR: line.appendText("ERROR"); break;
              case TRI_LOG_CATEGORY_WARNING: { line.appendText("WARNING"); break; }
              case TRI_LOG_CATEGORY_REQUEST_IN_START: line.appendText("REQUEST-IN-START"); break;
              case TRI_LOG_CATEGORY_REQUEST_IN_END: line.appendText("REQUEST-IN-END"); break;
              case TRI_LOG_CATEGORY_REQUEST_OUT_START: line.appendText("REQUEST-OUT-START"); break;
              case TRI_LOG_CATEGORY_REQUEST_OUT_END: line.appendText("REQUEST-OUT-END"); break;
              case TRI_LOG_CATEGORY_HEARTBEAT: line.appendText("HEARTBEAT"); break;

              case TRI_LOG_CATEGORY_MODULE_IN_START: line.appendText("REQUEST-MODULE-IN-START"); break;
              case TRI_LOG_CATEGORY_MODULE_IN_END: line.appendText("REQUEST-MODULE-IN-END"); break;
              case TRI_LOG_CATEGORY_FUNCTION_IN_START: line.appendText("FUNCTION-IN-START"); break;
              case TRI_LOG_CATEGORY_FUNCTION_IN_END: line.appendText("FUNCTION-IN-END"); break;
              case TRI_LOG_CATEGORY_STEP: line.appendText("STEP"); break;
              case TRI_LOG_CATEGORY_LOOP: line.appendText("LOOP"); break;
              case TRI_LOG_CATEGORY_HEARTPULSE: line.appendText("HEARTPULSE"); break;
            }
          }

          break;
        }

        // .............................................................................
        // extras
        // .............................................................................

        case 'E': {
          for (vector<LoggerData::Extra>::const_iterator i = info._extras.begin();
               i != info._extras.end();
               ++i) {
            if (i != info._extras.begin()) {
              line.appendChar(';');
            }

            LoggerData::Extra const& extra = *i;

            line.appendText(StringUtils::escapeHex(extra._name, SpecialCharacters));
          }

          break;
        }

        // .............................................................................
        // facility
        // .............................................................................

        case 'F': {
          line.appendText(info._facility._name);
          break;
        }

        // .............................................................................
        // module name
        // .............................................................................

        case 'f': {
          line.appendText(info._position._file);
          break;
        }

        // .............................................................................
        // host name
        // .............................................................................

        case 'H': {
          line.appendText(info._hostName._name);
          break;
        }

        // .............................................................................
        // task
        // .............................................................................

        case 'K': {
          line.appendText(info._task._name);
          break;
        }

        // .............................................................................
        // line
        // .............................................................................

        case 'l': {
          line.appendInteger(info._position._line);
          break;
        }

        // .............................................................................
        // message identifier
        // .............................................................................

        case 'M': {
          line.appendText(info._messageIdentifier._name);
          break;
        }

        // .............................................................................
        // method name
        // .............................................................................

        case 'm': {
          line.appendText(info._position._function);
          break;
        }

        // .............................................................................
        // process identifier
        // .............................................................................

        case 'p': {
          line.appendInteger((uint64_t) info._processIdentifier._process);
          break;
        }

        // .............................................................................
        // peg
        // .............................................................................

        case 'P': {
          line.appendText(info._peg._name);
          break;
        }

        // .............................................................................
        // severity
        // .............................................................................

        case 'S': {
          switch (info._severity) {
            case TRI_LOG_SEVERITY_EXCEPTION: line.appendInteger(2);  break;
            case TRI_LOG_SEVERITY_FUNCTIONAL: line.appendInteger(5);  break;
            case TRI_LOG_SEVERITY_USAGE: line.appendInteger(5);  break;
            case TRI_LOG_SEVERITY_TECHNICAL: line.appendInteger(6);  break;
            case TRI_LOG_SEVERITY_DEVELOPMENT: line.appendInteger(7);  break;
            default: line.appendInteger(7);  break;
          }

          break;
        }

        // .............................................................................
        // pthread identifier
        // .............................................................................

        case 's': {
          line.appendInteger((uint64_t) info._processIdentifier._threadProcess);
          break;
        }

        // .............................................................................
        // timestamp
        // .............................................................................

        case 'T': {
          struct tm tb;

          TRI_gmtime(tt, &tb);

          char s[32];
          strftime(s,  sizeof(s) - 1, "%Y-%m-%dT%H:%M:%S", &tb);

          line.appendText(s);

          break;
        }

        // .............................................................................
        // thread identifier
        // .............................................................................

        case 't': {
          line.appendInteger((uint64_t) info._processIdentifier._thread);
          break;
        }

        // .............................................................................
        // measure unit
        // .............................................................................

        case 'U': {
          switch (info._measure._unit) {
            case LoggerData::UNIT_SECONDS:  line.appendText("s");  break;
            case LoggerData::UNIT_MILLI_SECONDS:  line.appendText("ms");  break;
            case LoggerData::UNIT_MICRO_SECONDS:  line.appendText("us");  break;
            case LoggerData::UNIT_NANO_SECONDS:  line.appendText("ns");  break;

            case LoggerData::UNIT_BYTE:  line.appendText("b");  break;
            case LoggerData::UNIT_KILO_BYTE:  line.appendText("kb");  break;
            case LoggerData::UNIT_MEGA_BYTE:  line.appendText("mb");  break;
            case LoggerData::UNIT_GIGA_BYTE:  line.appendText("gb");  break;

            case LoggerData::UNIT_LESS:  break;
          }

          break;
        }

        // .............................................................................
        // user identifier
        // .............................................................................

        case 'u': {
          line.appendText(info._userIdentifier._user);
          break;
        }

        // .............................................................................
        // measure value
        // .............................................................................

        case 'V': {
          line.appendDecimal(info._measure._value);
          break;
        }

        // .............................................................................
        // text
        // .............................................................................

        case 'x': {
          if (! info._prefix.empty()) {
            line.appendText(StringUtils::escapeHex(info._prefix, SpecialCharacters));
          }
          line.appendText(StringUtils::escapeHex(text, SpecialCharacters));

          break;
        }

        // .............................................................................
        // timestamp in zulu
        // .............................................................................

        case 'Z': {
          struct tm tb;

          TRI_gmtime(tt, &tb);

          char s[32];
          strftime(s,  sizeof(s) - 1, "%Y-%m-%dT%H:%M:%SZ", &tb);

          line.appendText(s);

          break;
        }
      }
    }
    else {
      line.appendChar(*format);
    }
  }

  TRI_RawLog(info._level, info._severity, line.c_str(), line.length());
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      class Logger
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  static variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief global logger
////////////////////////////////////////////////////////////////////////////////

Logger Logger::_singleton;

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

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the application name
////////////////////////////////////////////////////////////////////////////////

void Logger::setApplicationName (string const& name) {
  LoggerData::Info::_applicationName._name = name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the facility
////////////////////////////////////////////////////////////////////////////////

void Logger::setFacility (string const& name) {
  LoggerData::Info::_facility._name = name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the host name
////////////////////////////////////////////////////////////////////////////////

void Logger::setHostName (string const& name) {
  LoggerData::Info::_hostName._name = name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the log format
////////////////////////////////////////////////////////////////////////////////

void Logger::setLogFormat (string const& format) {
  LoggerFormat = format;
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
/// @brief constructs a new logger
////////////////////////////////////////////////////////////////////////////////

Logger::Logger () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a logger
////////////////////////////////////////////////////////////////////////////////

Logger::~Logger () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief output text
////////////////////////////////////////////////////////////////////////////////

void Logger::output (string const& text, LoggerData::Info const& info) {
  // human readable
  if (info._severity == TRI_LOG_SEVERITY_HUMAN) {
    if (! TRI_IsHumanLogging()) {
      return;
    }

    TRI_Log(info._position._function.c_str(),
            info._position._file.c_str(),
            info._position._line,
            info._level,
            info._severity,
            "%s", text.c_str());

    return;
  }
  // usage logging
  else if (info._severity == TRI_LOG_SEVERITY_USAGE) {
    if (! TRI_IsUsageLogging()) {
      return;
    }
  }

  // machine readable logging
  OutputMachine(text, info);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
