////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <iostream>
#include <velocypack/Slice.h>
#include <compare>

namespace arangodb::velocypack {
class Value;
template<typename, typename>
struct Extractor;
}
namespace arangodb {
namespace replication2::replicated_state {

struct StateGeneration {
  constexpr StateGeneration() noexcept : value{0} {}
  constexpr explicit StateGeneration(std::uint64_t value) noexcept
      : value{value} {}
  std::uint64_t value;

  [[nodiscard]] auto saturatedDecrement(uint64_t delta = 1) const noexcept -> StateGeneration;

  friend auto operator<=>(StateGeneration const&, StateGeneration const&) = default;

  [[nodiscard]] auto operator+(std::uint64_t delta) const -> StateGeneration;

  friend auto operator<<(std::ostream&, StateGeneration) -> std::ostream&;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

auto operator<<(std::ostream&, StateGeneration) -> std::ostream&;
}  // namespace replication2::replicated_state

template <>
struct arangodb::velocypack::Extractor<replication2::replicated_state::StateGeneration> {
  static auto extract(velocypack::Slice slice) -> replication2::replicated_state::StateGeneration {
    return replication2::replicated_state::StateGeneration{
        slice.getNumericValue<std::uint64_t>()};
  }
};
}  // namespace arangodb
