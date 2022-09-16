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
  if (value.has_value() && value.value().empty()) {
    return {"Value cannot be empty."};
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

auto UtilityInvariants::isValidShardingStrategy(std::string const& strat)
    -> inspection::Status {
  // Note we may be better off with a lookup list here
  // Hash is first on purpose (default)
  if (strat == "" || strat == "hash" || strat == "enterprise-hash-smart-edge" ||
      strat == "community-compat" || strat == "enterprise-compat" ||
      strat == "enterprise-smart-edge-compat") {
    return inspection::Status::Success{};
  }
  return {
      "Please use 'hash' or remove, advanced users please "
      "pick a strategy from the documentation, " +
      strat + " is not allowed."};
}

auto UtilityInvariants::isValidCollectionType(
    std::underlying_type_t<TRI_col_type_e> const& type) -> inspection::Status {
  if (type == TRI_col_type_e::TRI_COL_TYPE_DOCUMENT ||
      type == TRI_col_type_e::TRI_COL_TYPE_EDGE) {
    return inspection::Status::Success{};
  }
  return {"Only 2 (document) and 3 (edge) are allowed."};
}

auto UtilityInvariants::areShardKeysValid(std::vector<std::string> const& keys)
    -> inspection::Status {
  if (keys.empty() || keys.size() > 8) {
    return {"invalid number of shard keys for collection"};
  }
  for (auto const& sk : keys) {
    auto key = std::string_view{sk};
    // remove : char at the beginning or end (for enterprise)
    std::string_view stripped;
    if (!key.empty()) {
      if (key.front() == ':') {
        stripped = key.substr(1);
      } else if (key.back() == ':') {
        stripped = key.substr(0, key.size() - 1);
      } else {
        stripped = key;
      }
    }
    // system attributes are not allowed (except _key, _from and _to)
    if (stripped == StaticStrings::IdString ||
        stripped == StaticStrings::RevString) {
      return {"_id or _rev cannot be used as shard keys"};
    }
  }
  return inspection::Status::Success{};
}
