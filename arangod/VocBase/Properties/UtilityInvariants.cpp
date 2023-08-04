////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "UtilityInvariants.h"

#include "Basics/StaticStrings.h"
#include "Inspection/Access.h"
#include "Inspection/Status.h"

using namespace arangodb;

auto UtilityInvariants::isNonEmpty(std::string const& value)
    -> inspection::Status {
  if (value.empty()) {
    return {"Value cannot be empty."};
  }
  return inspection::Status::Success{};
}

auto UtilityInvariants::isNonEmptyIfPresent(
    std::optional<std::string> const& value) -> inspection::Status {
  if (value.has_value()) {
    return isNonEmpty(value.value());
  }
  return inspection::Status::Success{};
}

auto UtilityInvariants::isGreaterZero(uint64_t const& value)
    -> inspection::Status {
  if (value > 0) {
    return inspection::Status::Success{};
  }
  return {"Value has to be > 0"};
}

auto UtilityInvariants::isGreaterZeroIfPresent(
    std::optional<uint64_t> const& value) -> inspection::Status {
  if (value.has_value()) {
    return isGreaterZero(value.value());
  }
  return inspection::Status::Success{};
}

auto UtilityInvariants::isValidShardingStrategy(std::string const& strat)
    -> inspection::Status {
  // Note we may be better off with a lookup list here
  // Hash is first on purpose (default)
  if (strat == "" || strat == "hash" || strat == "enterprise-hash-smart-edge" ||
      strat == "community-compat" || strat == "enterprise-compat" ||
      strat == "enterprise-smart-edge-compat" ||
      strat == "enterprise-hex-smart-vertex") {
    return inspection::Status::Success{};
  }
  return {
      "Please use 'hash' or remove, advanced users please "
      "pick a strategy from the documentation, " +
      strat + " is not allowed."};
}

auto UtilityInvariants::isValidShardingStrategyIfPresent(
    std::optional<std::string> const& value) -> inspection::Status {
  if (value.has_value()) {
    return isValidShardingStrategy(value.value());
  }
  return inspection::Status::Success{};
}

auto UtilityInvariants::isValidCollectionType(
    std::underlying_type_t<TRI_col_type_e> const& type) -> inspection::Status {
  if (type == TRI_col_type_e::TRI_COL_TYPE_DOCUMENT ||
      type == TRI_col_type_e::TRI_COL_TYPE_EDGE) {
    return inspection::Status::Success{};
  }
  return {"Only 2 (document) and 3 (edge) are allowed."};
}
