////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Baesler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_DATETIME_H
#define ARANGODB_BASICS_DATETIME_H 1

#include <chrono>
#include <regex>
#include <string>

#include "Basics/Common.h"

#include <date/date.h>
#include <velocypack/StringRef.h>

namespace arangodb {

using tp_sys_clock_ms =
    std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

using d_sys_clock_ms =
    std::chrono::duration<std::chrono::milliseconds>;

namespace basics {
bool parseDateTime(arangodb::velocypack::StringRef dateTime,
                   tp_sys_clock_ms& date_tp);

bool regexIsoDuration(arangodb::velocypack::StringRef isoDuration, 
                      std::match_results<char const*>& durationParts);

/// @brief formats a date(time) value according to formatString
std::string formatDate(std::string const& formatString,
                       tp_sys_clock_ms const& dateValue);

struct ParsedDuration {
  int years = 0;
  int months = 0;
  int weeks = 0;
  int days = 0;
  int hours = 0;
  int minutes = 0;
  int seconds = 0;
  int milliseconds = 0;
};

bool parseIsoDuration(arangodb::velocypack::StringRef duration,
                      ParsedDuration& output);
}  // namespace basics
}  // namespace arangodb

#endif
