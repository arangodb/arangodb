////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <string>
#include <unordered_map>

#include "LogTimeFormat.h"

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/datetime.h"
#include "Basics/debugging.h"
#include "Basics/system-functions.h"

namespace {

std::unordered_map<std::string, arangodb::LogTimeFormats::TimeFormat> const formatMap{
  { "uptime", arangodb::LogTimeFormats::TimeFormat::Uptime }, 
  { "uptime-millis", arangodb::LogTimeFormats::TimeFormat::UptimeMillis }, 
  { "uptime-micros", arangodb::LogTimeFormats::TimeFormat::UptimeMicros }, 
  { "timestamp", arangodb::LogTimeFormats::TimeFormat::UnixTimestamp }, 
  { "timestamp-millis", arangodb::LogTimeFormats::TimeFormat::UnixTimestampMillis }, 
  { "timestamp-micros", arangodb::LogTimeFormats::TimeFormat::UnixTimestampMicros }, 
  { "utc-datestring", arangodb::LogTimeFormats::TimeFormat::UTCDateString }, 
  { "utc-datestring-millis", arangodb::LogTimeFormats::TimeFormat::UTCDateStringMillis }, 
  { "local-datestring", arangodb::LogTimeFormats::TimeFormat::LocalDateString }, 
};

std::chrono::time_point<std::chrono::system_clock> const startTime = std::chrono::system_clock::now();
    
void appendNumber(uint64_t value, std::string& out, size_t size) {
  char buffer[22];
  size_t len = arangodb::basics::StringUtils::itoa(value, &buffer[0]);
  while (len < size) {
    out.push_back('0');
    --size;
  }
  out.append(&buffer[0], len);
}

}

namespace arangodb {
namespace LogTimeFormats {

/// @brief whether or not the specified format is a local one
bool isLocalFormat(TimeFormat format) {
  return format == TimeFormat::LocalDateString; 
}

/// @brief whether or not the specified format produces string outputs
/// (in contrast to numeric outputs)
bool isStringFormat(TimeFormat format) {
  return format == TimeFormat::UTCDateString ||
         format == TimeFormat::UTCDateStringMillis ||
         format == TimeFormat::LocalDateString;
}

/// @brief return the name of the default log time format
std::string defaultFormatName() {
  return "utc-datestring";
}

/// @brief return the names of all log time formats
std::unordered_set<std::string> getAvailableFormatNames() {
  std::unordered_set<std::string> formats;
  for (auto const& it : ::formatMap) {
    formats.insert(it.first);
  }
  return formats;
}

/// @brief derive the time format from the name
TimeFormat formatFromName(std::string const& name) {
  auto it = ::formatMap.find(name);
  // if this assertion does not hold true, some ArangoDB developer xxxed up
  TRI_ASSERT(it != ::formatMap.end());

  if (it == ::formatMap.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid time format");
  }

  return (*it).second;
}
  
void writeTime(std::string& out, TimeFormat format) {
  std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();

  if (format == TimeFormat::Uptime) {
    // integral uptime value
    arangodb::basics::StringUtils::itoa(uint64_t(std::chrono::duration_cast<std::chrono::seconds>(tp - ::startTime).count()), out);
  } else if (format == TimeFormat::UptimeMillis) {
    // uptime with millisecond precision
    std::chrono::system_clock::time_point tp2(std::chrono::duration_cast<std::chrono::seconds>(tp - ::startTime));
    std::chrono::system_clock::time_point tp3(std::chrono::duration_cast<std::chrono::milliseconds>(tp - ::startTime));
    arangodb::basics::StringUtils::itoa(uint64_t(std::chrono::duration_cast<std::chrono::seconds>(tp2.time_since_epoch()).count()), out);
    out.push_back('.');
    appendNumber(uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(tp3 - tp2).count()), out, 3);
  } else if (format == TimeFormat::UptimeMicros) {
    // uptime with microsecond precision
    std::chrono::system_clock::time_point tp2(std::chrono::duration_cast<std::chrono::seconds>(tp - ::startTime));
    std::chrono::system_clock::time_point tp3(std::chrono::duration_cast<std::chrono::microseconds>(tp - ::startTime));
    arangodb::basics::StringUtils::itoa(uint64_t(std::chrono::duration_cast<std::chrono::seconds>(tp2.time_since_epoch()).count()), out);
    out.push_back('.');
    appendNumber(uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(tp3 - tp2).count()), out, 6);
  } else if (format == TimeFormat::UnixTimestamp) {
    // integral unix timestamp
    arangodb::basics::StringUtils::itoa(uint64_t(std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count()), out);
  } else if (format == TimeFormat::UnixTimestampMillis) {
    // unix timestamp with millisecond precision
    std::chrono::system_clock::time_point tp2(std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()));
    arangodb::basics::StringUtils::itoa(uint64_t(std::chrono::duration_cast<std::chrono::seconds>(tp2.time_since_epoch()).count()), out);
    out.push_back('.');
    appendNumber(uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(tp - tp2).count()), out, 3);
  } else if (format == TimeFormat::UnixTimestampMicros) {
    // unix timestamp with microsecond precision
    std::chrono::system_clock::time_point tp2(std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()));
    arangodb::basics::StringUtils::itoa(uint64_t(std::chrono::duration_cast<std::chrono::seconds>(tp2.time_since_epoch()).count()), out);
    out.push_back('.');
    appendNumber(uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(tp - tp2).count()), out, 6);
  } else {
    // all date-string variants handled here
    if (format == TimeFormat::UTCDateString) {
      // UTC datestring
      out.append(arangodb::basics::formatDate("%yyyy-%mm-%ddT%hh:%ii:%ssZ", tp_sys_clock_ms(std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()))));
    } else if (format == TimeFormat::UTCDateStringMillis) {
      // UTC datestring with milliseconds
      out.append(arangodb::basics::formatDate("%yyyy-%mm-%ddT%hh:%ii:%ss.%fffZ", tp_sys_clock_ms(std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()))));
    } else if (format == TimeFormat::LocalDateString) {
      // local datestring
      time_t tt = time(nullptr);
      struct tm tb;

      TRI_localtime(tt, &tb);
      char buffer[32];
      strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tb);
      out.append(&buffer[0]);
    }
  }
}

}  // namespace LogTimeFormats
}  // namespace arangodb
