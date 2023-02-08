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
#include <Inspection/VPackWithErrorT.h>

#include <ostream>
#include <fmt/core.h>
#include <fmt/format.h>

namespace arangodb::pregel {
struct ExecutionNumber {
  // FIXME: handle invalid execution numbers better
  ExecutionNumber() : value(0) {}
  explicit ExecutionNumber(uint64_t v) : value(v) {}
  friend auto operator<=>(ExecutionNumber, ExecutionNumber) noexcept = default;

  uint64_t value;
};

template<class Inspector>
auto inspect(Inspector& f, ExecutionNumber& x) {
  if constexpr (Inspector::isLoading) {
    uint64_t v = 0;
    auto res = f.apply(v);
    if (res.ok()) {
      x = ExecutionNumber(v);
    }
    return res;
  } else {
    return f.apply(x.value);
  }
}

auto operator<<(std::ostream&, arangodb::pregel::ExecutionNumber const&)
    -> std::ostream&;

}  // namespace arangodb::pregel

template<>
struct fmt::formatter<arangodb::pregel::ExecutionNumber> {
  template<typename FormatContext>
  auto format(arangodb::pregel::ExecutionNumber number, FormatContext& ctx) {
    return fmt::format_to(ctx.out(), "{}", number.value);
  }
  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }
};

namespace std {
template<>
struct hash<arangodb::pregel::ExecutionNumber> {
  size_t operator()(arangodb::pregel::ExecutionNumber const& x) const noexcept {
    return std::hash<uint64_t>()(x.value);
  };
};
}  // namespace std
