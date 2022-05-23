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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <string>
#include <chrono>
#include "Basics/TimeString.h"
#include "Basics/ErrorCode.h"

namespace arangodb::inspection {

struct TimeStampTransformer {
  using SerializedType = std::string;
  using clock = std::chrono::system_clock;
  auto toSerialized(clock::time_point source, std::string& target) const
      -> inspection::Status {
    target = timepointToString(source);
    return {};
  }
  auto fromSerialized(std::string const& source,
                      clock::time_point& target) const -> inspection::Status {
    target = stringToTimepoint(source);
    return {};
  }
};

template<typename Duration>
struct DurationTransformer {
  using SerializedType = typename Duration::rep;
  auto toSerialized(Duration source, SerializedType& target) const
      -> inspection::Status {
    target = source.count();
    return {};
  }
  auto fromSerialized(SerializedType source, Duration& target) const
      -> inspection::Status {
    target = Duration{source};
    return {};
  }
};

struct ErrorCodeTransformer {
  struct ErrorCodeWithMessage {
    ErrorCode::ValueType code;
    std::string message;
  };
  using SerializedType = ErrorCodeWithMessage;

  auto toSerialized(ErrorCode source, ErrorCodeWithMessage& target) const
      -> inspection::Status {
    target.code = source.operator ErrorCode::ValueType();
    target.message = to_string(source);
    return {};
  }
  auto fromSerialized(ErrorCodeWithMessage const& source,
                      ErrorCode& target) const -> inspection::Status {
    target = ErrorCode(source.code);
    return {};
  }
};

template<class Inspector>
auto inspect(Inspector& f, ErrorCodeTransformer::ErrorCodeWithMessage& x) {
  return f.object(x).fields(f.field("code", x.code),
                            f.field("message", x.message));
}

}  // namespace arangodb::inspection
