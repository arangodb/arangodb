////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <chrono>
#include "date/date.h"

inline std::string timepointToString(
    std::chrono::system_clock::time_point const& t) {
  return date::format("%Y-%m-%dT%H:%M:%SZ",
                      std::chrono::floor<std::chrono::seconds>(t));
}

inline std::string timepointToString(
    std::chrono::system_clock::duration const& d) {
  return timepointToString(std::chrono::system_clock::time_point() + d);
}

inline std::chrono::system_clock::time_point stringToTimepoint(
    std::string_view s) {
  // FIXME: now copy :(
  auto in = std::istringstream{std::string{s}};
  auto target = std::chrono::system_clock::time_point{};
  in >> date::parse("%Y-%m-%dT%H:%M:%SZ", target);

  // FIXME: error handling would be nice.
  if (!in.fail()) {
    return target;
  } else {
    return {};
  }
}
