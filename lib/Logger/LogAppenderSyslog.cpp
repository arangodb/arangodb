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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Logger/LogAppenderSyslog.h"

using namespace arangodb;

#ifdef ARANGODB_ENABLE_SYSLOG

// we need to define SYSLOG_NAMES for linux to get a list of names
#define SYSLOG_NAMES
#include <syslog.h>

#ifdef ARANGODB_ENABLE_SYSLOG_STRINGS
#include "syslog_names.h"
#endif

#include <cstring>
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"

using namespace arangodb::basics;

bool LogAppenderSyslog::_opened(false);

void LogAppenderSyslog::close() {
  if (_opened) {
    _opened = false;
    ::closelog();
  }
}

LogAppenderSyslog::LogAppenderSyslog(std::string const& facility,
                                     std::string const& name, std::string const& filter)
    : LogAppender(filter) {
  // no logging
  std::string sysname = name.empty() ? "[arangod]" : name;

  // find facility
  int value = LOG_LOCAL0;

  if ('0' <= facility[0] && facility[0] <= '9') {
    value = StringUtils::int32(facility);
  } else {
    CODE* ptr = reinterpret_cast<CODE*>(facilitynames);

    while (ptr->c_name != nullptr) {
      if (strcmp(ptr->c_name, facility.c_str()) == 0) {
        value = ptr->c_val;
        break;
      }

      ++ptr;
    }
  }

  // and open logging, openlog does not have a return value...
  ::openlog(sysname.c_str(), LOG_CONS | LOG_PID, value);
  _opened = true;
}

void LogAppenderSyslog::logMessage(LogLevel level, std::string const& message, size_t offset) {
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

  if (_opened) {
    ::syslog(priority, "%s", message.c_str() + offset);
  }
}

std::string LogAppenderSyslog::details() {
  return "More error details may be provided in the syslog";
}

#else

LogAppenderSyslog::LogAppenderSyslog(std::string const& facility,
                                     std::string const& name, std::string const& filter)
    : LogAppender(filter) {
  std::abort();
}

void LogAppenderSyslog::close() {}

#endif
