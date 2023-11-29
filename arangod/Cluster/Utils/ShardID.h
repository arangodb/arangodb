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

#include "Basics/ResultT.h"
#include "Inspection/Status.h"

#include <fmt/format.h>

#include <cstdint>
#include <string_view>

namespace arangodb {

struct ShardID {
  static ResultT<ShardID> shardIdFromString(std::string_view s);

  static ShardID invalidShard() noexcept;

  // Default constructor required for Inspectors.
  // Note: This shardID is considered Invalid.
  constexpr ShardID() noexcept : _id(0) {}

  explicit ShardID(uint64_t id) noexcept : _id(id) {}

  explicit ShardID(std::string_view id);

  ~ShardID() = default;
  ShardID(ShardID const&) = default;
  ShardID(ShardID&&) = default;
  ShardID& operator=(ShardID const&) = default;
  ShardID& operator=(ShardID&&) = default;

  operator std::string() const;

  friend auto operator<=>(ShardID const&, ShardID const&) noexcept = default;
  friend bool operator==(ShardID const&, ShardID const&) noexcept = default;

  bool operator==(std::string_view other) const;

  bool isValid() const noexcept;

  uint64_t id() const noexcept { return _id; }

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
          std::string msg{result.errorMessage()};
          return {std::move(msg)};
        }
        x = result.get();
        return {};
      }
      return res;
    } else {
      return f.apply(std::string{x});
    }
  }

 private:
  uint64_t _id;
};

// Make ShardID loggable
std::ostream& operator<<(std::ostream& o, arangodb::ShardID const& r);

}  // namespace arangodb

// Make ShardID ready to be used as key in std::unordered_map
template<>
struct std::hash<arangodb::ShardID> {
  [[nodiscard]] auto operator()(arangodb::ShardID const& v) const noexcept
      -> std::size_t;
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
    return fmt::format_to(ctx.out(), "s{}", std::to_string(shardId.id()));
  }
};

// Allow ShardID to be added to std::strings
std::string operator+(std::string_view text, arangodb::ShardID const& s);
// Allow ShardID to be added to std::strings
std::string operator+(arangodb::ShardID const& s, std::string_view text);
