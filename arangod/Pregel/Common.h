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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <chrono>
#include <string>

#include <Inspection/VPack.h>
#include <Inspection/Transformers.h>

namespace arangodb::pregel {

namespace static_strings {
constexpr auto start = std::string_view{"start"};
constexpr auto end = std::string_view{"end"};

}  // namespace static_strings

using TimeStamp = std::chrono::system_clock::time_point;

struct TimeInterval {
  std::optional<TimeStamp> start;
  std::optional<TimeStamp> end;
};

template<typename Inspector>
auto inspect(Inspector& f, TimeInterval& x) {
  return f.object(x).fields(
      f.field(static_strings::start, x.start)
          .transformWith(inspection::TimeStampTransformer{}),
      f.field(static_strings::end, x.end)
          .transformWith(inspection::TimeStampTransformer{}));
}
}  // namespace arangodb::pregel
