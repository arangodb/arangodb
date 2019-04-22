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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_LOGGER_LOG_TIME_FORMAT_H
#define ARANGODB_LOGGER_LOG_TIME_FORMAT_H 1

#include "Basics/Common.h"

#include <iosfwd>

namespace arangodb {
namespace LogTimeFormats {

enum class TimeFormat {
  Uptime,
  UptimeMillis,
  UptimeMicros,
  UnixTimestamp,
  UnixTimestampMillis,
  UnixTimestampMicros,
  UTCDateString,
  UTCDateStringMillis,
  LocalDateString,
};

/// @brief whether or not the specified format is a local one
bool isLocalFormat(TimeFormat format);

/// @brief return the name of the default log time format
std::string defaultFormatName();

/// @brief return the names of all log time formats
std::unordered_set<std::string> getAvailableFormatNames();

/// @brief derive the time format from the name
TimeFormat formatFromName(std::string const& name);

/// @brief writes the current time into the given buffer,
/// in the specified format
void writeTime(std::ostream& out, TimeFormat format);

}  // namespace LogTimeFormats
}  // namespace arangodb

#endif
