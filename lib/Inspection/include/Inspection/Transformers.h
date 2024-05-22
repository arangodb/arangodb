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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <string>
#include <chrono>
#include <vector>
#include "Basics/TimeString.h"
#include "Basics/ErrorCode.h"
#include "Inspection/Status.h"

#include "date/date.h"
#include "fmt/core.h"

namespace arangodb::inspection {

struct TimeStampTransformer {
  using SerializedType = std::string;
  using clock = std::chrono::system_clock;
  static constexpr const char* formatString = "%FT%TZ";
  auto toSerialized(clock::time_point source, std::string& target) const
      -> inspection::Status {
    target = date::format(formatString, floor<std::chrono::seconds>(source));
    return {};
  }
  auto fromSerialized(std::string const& source,
                      clock::time_point& target) const -> inspection::Status {
    auto in = std::istringstream{source};
    in >> date::parse(formatString, target);

    if (in.fail()) {
      return inspection::Status(
          fmt::format("failed to parse timestamp `{}` using format string `{}`",
                      source, formatString));
    } else {
      return {};
    }
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

template<class Map>
struct MapToListTransformer {
  struct Entry {
    typename Map::key_type key;
    typename Map::mapped_type value;

    friend inline auto inspect(auto& f, Entry& x) {
      return f.object(x).fields(f.field("key", x.key),
                                f.field("value", x.value));
    }
  };
  using SerializedType = std::vector<Entry>;

  auto toSerialized(Map const& source, SerializedType& target) const
      -> inspection::Status {
    target.reserve(source.size());
    target.clear();
    for (auto const& [key, value] : source) {
      target.push_back({key, value});
    }
    return {};
  }
  auto fromSerialized(SerializedType const& source, Map& target) const
      -> inspection::Status {
    target.clear();
    for (auto const& entry : source) {
      target.emplace(entry.key, entry.value);
    }
    return {};
  }
};
template<class Map>
auto mapToListTransformer(Map const&) {
  return MapToListTransformer<Map>{};
}

template<class Inspector>
auto inspect(Inspector& f, ErrorCodeTransformer::ErrorCodeWithMessage& x) {
  return f.object(x).fields(f.field("code", x.code),
                            f.field("message", x.message));
}

}  // namespace arangodb::inspection
