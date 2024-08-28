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
/// @author Achim Brandt
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <iosfwd>
#include <string>

namespace arangodb {
enum class LogLevel {
  DEFAULT = 0,
  FATAL = 1,
  ERR = 2,
  WARN = 3,
  INFO = 4,
  DEBUG = 5,
  TRACE = 6
};

template<typename Inspector>
auto inspect(Inspector& f, LogLevel& l) {
  return f.enumeration(l).transformedValues(
      [](std::string& s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::toupper(c); });
      },
      LogLevel::DEFAULT, "DEFAULT",  //
      LogLevel::FATAL, "FATAL",      //
      LogLevel::ERR, "ERROR",        //
      LogLevel::ERR, "ERR",          //
      LogLevel::WARN, "WARNING",     //
      LogLevel::WARN, "WARN",        //
      LogLevel::INFO, "INFO",        //
      LogLevel::DEBUG, "DEBUG",      //
      LogLevel::TRACE, "TRACE");
}
}  // namespace arangodb

std::ostream& operator<<(std::ostream&, arangodb::LogLevel);
