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

#include "StateCommon.h"

#include <iostream>
#include <velocypack/Value.h>

using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;

StateGeneration::operator arangodb::velocypack::Value() const noexcept {
  return arangodb::velocypack::Value(value);
}

auto StateGeneration::operator+(std::uint64_t delta) const -> StateGeneration {
  return StateGeneration{value + delta};
}

auto StateGeneration::saturatedDecrement(uint64_t delta) const noexcept -> StateGeneration {
  if (value > delta) {
    return StateGeneration{value - delta};
  }
  return StateGeneration{0};
}

auto replicated_state::operator<<(std::ostream& os, StateGeneration g) -> std::ostream& {
  return os << g.value;
}
