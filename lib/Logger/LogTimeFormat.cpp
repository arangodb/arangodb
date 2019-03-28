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

#include "LogTimeFormat.h"
#include "Basics/Exceptions.h"

#include <chrono>

namespace {

std::unordered_map<std::string, arangodb::LogTimeFormats::TimeFormat> const formatMap{
  { "uptime", arangodb::LogTimeFormats::TimeFormat::Uptime }, 
  { "uptime-millis", arangodb::LogTimeFormats::TimeFormat::UptimeMillis }, 
  { "uptime-micros", arangodb::LogTimeFormats::TimeFormat::UptimeMicros }, 
  { "timestamp", arangodb::LogTimeFormats::TimeFormat::UnixTimestamp }, 
  { "timestamp-millis", arangodb::LogTimeFormats::TimeFormat::UnixTimestampMillis }, 
  { "timestamp-micros", arangodb::LogTimeFormats::TimeFormat::UnixTimestampMicros }, 
  { "utc-datestring", arangodb::LogTimeFormats::TimeFormat::UTCDateString }, 
//  { "utc-datestring-millis", arangodb::LogTimeFormats::TimeFormat::UTCDateStringMillis }, 
//  { "utc-datestring-micros", arangodb::LogTimeFormats::TimeFormat::UTCDateStringMicros }, 
  { "local-datestring", arangodb::LogTimeFormats::TimeFormat::LocalDateString }, 
//  { "local-datestring-micros", arangodb::LogTimeFormats::TimeFormat::LocalDateStringMillis }, 
//  { "local-datestring-micros", arangodb::LogTimeFormats::TimeFormat::LocalDateStringMicros }, 
};

std::chrono::time_point<std::chrono::steady_clock> const startTime = std::chrono::steady_clock::now();

}

namespace arangodb {
namespace LogTimeFormats {

/// @brief whether or not the specified format is a local one
bool isLocalFormat(TimeFormat format) {
  return format == TimeFormat::LocalDateString || 
         format == TimeFormat::LocalDateStringMillis ||
         format == TimeFormat::LocalDateStringMicros;
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
  
void writeTime(char* buffer, size_t bufferSize, TimeFormat format) {
  if (format == TimeFormat::Uptime) {
    // integral uptime value
    auto seconds = static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - ::startTime).count());
    snprintf(buffer, bufferSize, "%llu ", seconds);

  } else if (format == TimeFormat::UptimeMillis) {
    // uptime with millisecond precision
    auto millis = static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - ::startTime).count());
    auto tmp = millis / 1'000;
    snprintf(buffer, bufferSize, "%llu.%03llu ", tmp, millis - tmp * 1'000);

  } else if (format == TimeFormat::UptimeMicros) {
    // uptime with microsecond precision
    auto micros = static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - ::startTime).count());
    auto tmp = micros / 1'000'000;
    snprintf(buffer, bufferSize, "%llu.%06llu ", tmp, micros - tmp * 1'000'000);

  } else if (format == TimeFormat::UnixTimestamp) {
    // integral unix timestamp
    auto seconds = static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    snprintf(buffer, bufferSize, "%llu ", seconds);

  } else if (format == TimeFormat::UnixTimestampMillis) {
    // unix timestamp with millisecond precision
    auto millis = static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    auto tmp = millis / 1'000;
    snprintf(buffer, bufferSize, "%llu.%03llu ", tmp, millis - tmp * 1'000);

  } else if (format == TimeFormat::UnixTimestampMicros) {
    // unix timestamp with microsecond precision
    auto micros = static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    auto tmp = micros / 1'000'000;
    snprintf(buffer, bufferSize, "%llu.%06llu ", tmp, micros - tmp * 1'000'000);

  } else {
    // all date-string variants handled here
    time_t tt = time(nullptr);
    struct tm tb;

    if (format == TimeFormat::UTCDateString) {
      // UTC datestring
      TRI_gmtime(tt, &tb);
      strftime(buffer, bufferSize, "%Y-%m-%dT%H:%M:%SZ ", &tb);
/*
    } else if (format == TimeFormat::UTCDateStringMillis) {
      // UTC datestring with milliseconds
      TRI_gmtime(tt, &tb);
      strftime(buffer, bufferSize, "%Y-%m-%dT%H:%M:%S.000Z ", &tb);

    } else if (format == TimeFormat::UTCDateStringMicros) {
      // UTC datestring with microseconds
      TRI_gmtime(tt, &tb);
      strftime(buffer, bufferSize, "%Y-%m-%dT%H:%M:%S.000000Z ", &tb);
*/
    } else if (format == TimeFormat::LocalDateString) {
      // local datestring
      TRI_localtime(tt, &tb);
      strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S ", &tb);
/*
    } else if (format == TimeFormat::LocalDateStringMillis) {
      // local datestring with milliseconds
      TRI_localtime(tt, &tb);
      strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000 ", &tb);

    } else if (format == TimeFormat::LocalDateStringMicros) {
      // local datestring with microseconds
      TRI_localtime(tt, &tb);
      strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000000 ", &tb);
*/
    }
  } 
}

}  // namespace LogTimeFormats
}  // namespace arangodb
