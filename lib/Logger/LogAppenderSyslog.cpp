////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Logger/LogMessage.h"

using namespace arangodb;

#ifdef ARANGODB_ENABLE_SYSLOG

// we need to define SYSLOG_NAMES for linux to get a list of names
#define SYSLOG_NAMES
#include <syslog.h>

#ifdef ARANGODB_ENABLE_SYSLOG_STRINGS
#include "syslog_names.h"
#endif

#include <cstring>
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

using namespace arangodb::basics;

namespace {
// The writable strings warning is suppressed to prevent clang from complaining
// about pointing to a string constant using a char *, which happens in
// particular because musl defines CODE to be a struct containing just a `char
// *`, but requires using CODE * to iterate through facilitynames.
//
// Extra fun is to be had because facilitynames is a macro, which is why this
// code just accesses facilitynames by indexing
// findSyslogFacilityByName returns -1 as fallback; this is just dealt with
// below by setting the facility to LOG_LOCAL0 as a last resort, as error
// handling is not great with syslog
#if defined(__clang__)
#if __has_warning("-Wwritable-strings")
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wwritable-strings"
#endif
#endif
auto findSyslogFacilityByName(std::string const& facility) -> int {
  for (auto i = size_t{0}; i < LOG_NFACILITIES; ++i) {
    if (facilitynames[i].c_name == nullptr) {
      return -1;
    }
    if (strncmp(facilitynames[i].c_name, facility.c_str(), facility.size()) ==
        0) {
      return facilitynames[i].c_val;
    }
  }
  return -1;
}
#if defined(__clang__)
#if __has_warning("-Wwritable-strings")
#pragma clang diagnostic pop
#endif
#endif

}  // namespace

bool LogAppenderSyslog::_opened(false);

void LogAppenderSyslog::close() {
  if (_opened) {
    _opened = false;
    ::closelog();
  }
}

LogAppenderSyslog::LogAppenderSyslog(std::string const& facility,
                                     std::string const& name)
    : LogAppender(), _sysname(name.empty() ? "[arangod]" : name) {
  // find facility
  int value = LOG_LOCAL0;

  if ('0' <= facility[0] && facility[0] <= '9') {
    value = StringUtils::int32(facility);
  } else {
    value = findSyslogFacilityByName(name);
  }

  // try to be safe(er) with what is passed to openlog
  if (value < 0 or value >= LOG_NFACILITIES) {
    value = LOG_LOCAL0;
  }

  // from man 3 syslog:
  //   The argument ident in the call of openlog() is probably stored as-is.
  //   Thus, if the string it points to is changed, syslog() may start
  //   prepending the changed string, and  if
  //    the string it points to ceases to exist, the results are undefined. Most
  //    portable is to use a string constant.

  // and open logging, openlog does not have a return value...
  ::openlog(_sysname.c_str(), LOG_CONS | LOG_PID, value);
  _opened = true;
}

void LogAppenderSyslog::logMessage(LogMessage const& message) {
  int priority = LOG_ERR;

  switch (message._level) {
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
    ::syslog(priority, "%s", message._message.c_str() + message._offset);
  }
}

std::string LogAppenderSyslog::details() const {
  return "More error details may be provided in the syslog";
}

#endif
