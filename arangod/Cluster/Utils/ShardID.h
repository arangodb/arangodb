////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Exceptions.h"
#include "Basics/ResultT.h"
#include "Basics/StringUtils.h"
#include "Inspection/Status.h"

#include <compare>
#include <cstdint>
#include <fmt/format.h>
#include <velocypack/Value.h>

namespace arangodb {

struct ShardID {
  static ResultT<ShardID> shardIdFromString(std::string_view s) {
    if (s.empty() || s.at(0) != 's') {
      return Result{TRI_ERROR_BAD_PARAMETER,
                    "Expected ShardID to start with 's'"};
    }
    auto res = basics::StringUtils::try_uint64(s.data() + 1, s.length() - 1);
    if (res.fail()) {
      return std::move(res.result());
    }
    return ShardID{res.get()};
  }

  static ShardID invalidShard() { return ShardID{0}; }

  // Default constructor required for Inspectors.
  // Note: This shardID is considered Invalid.
  ShardID() : id(0) {}

  explicit ShardID(uint64_t id) : id(id) {}

  explicit ShardID(std::string_view id) {
    auto maybeShardID = shardIdFromString(id);
    if (maybeShardID.fail()) {
      THROW_ARANGO_EXCEPTION(maybeShardID.result());
    }
    *this = maybeShardID.get();
  }

  ~ShardID() = default;
  ShardID(ShardID const&) = default;
  ShardID(ShardID&&) = default;
  ShardID& operator=(ShardID const&) = default;
  ShardID& operator=(ShardID&&) = default;

  operator std::string() const { return "s" + std::to_string(id); }

  operator arangodb::velocypack::Value() const {
    return arangodb::velocypack::Value("s" + std::to_string(id));
  }

  friend auto operator<=>(ShardID const&, ShardID const&) = default;
  friend bool operator==(ShardID const&, ShardID const&) = default;

  bool operator==(std::string_view other) const {
    return other == std::string{*this};
  }

  bool operator==(std::string const& other) const {
    return other == std::string{*this};
  }

  bool isValid() const noexcept {
    // We can never have ShardID 0. So we use it as invalid value.
    return id != 0;
  }

  // Add an inspector implementation, shardIDs will be serialized and
  // deserialized as "s" + number for compatibility reasons.
  template<class Inspector>
  inline friend auto inspect(Inspector& f, ShardID& x)
      -> arangodb::inspection::Status {
    if constexpr (Inspector::isLoading) {
      std::string v;
      auto res = f.apply(v);
      if (res.ok()) {
        auto result = shardIdFromString(v);
        if (result.fail()) {
          // NOTE: For some reason we cannot inline the errorMessage() into the
          // return in that case the string_view variant will be selected, which
          // is no valid input for status.
          std::string msg{std::move(result.errorMessage())};
          return {std::move(msg)};
        }
        x = result.get();
        return {};
      }
      return res;
    } else {
      return f.apply("s" + std::to_string(x.id));
    }
  }

  uint64_t id;
};

// Make ShardID logable
static inline std::ostream& operator<<(std::ostream& o,
                                       arangodb::ShardID const& r) {
  o << "s" << r.id;
  return o;
}

}  // namespace arangodb

// Make ShardID ready to be used as key in std::unordered_map
template<>
struct std::hash<arangodb::ShardID> {
  [[nodiscard]] auto operator()(arangodb::ShardID const& v) const noexcept
      -> std::size_t {
    return std::hash<uint64_t>{}(v.id);
  }
};

// Make ShardID fmt::formatable
template<>
struct fmt::formatter<arangodb::ShardID> {
  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(arangodb::ShardID const& shardId, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", "s" + std::to_string(shardId.id));
  }
};

// Allow ShardID to be added to std::strings
static inline std::string operator+(std::string const& text,
                                    arangodb::ShardID const& s) {
  return text + "s" + std::to_string(s.id);
}
// Allow ShardID to be added to std::strings
static inline std::string operator+(arangodb::ShardID const& s,
                                    std::string const& text) {
  return "s" + std::to_string(s.id) + text;
}