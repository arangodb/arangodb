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

#include "PlanCollection.h"
#include "Cluster/ServerDefaults.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"

#include <velocypack/Collection.h>
#include <velocypack/Slice.h>

using namespace arangodb;

PlanCollection::PlanCollection() {}

ResultT<PlanCollection> PlanCollection::fromCreateAPIBody(
    VPackSlice input, ServerDefaults defaultValues) {
  if (!input.isObject()) {
    // Special handling to be backwards compatible error reporting
    // on "name"
    return Result{TRI_ERROR_ARANGO_ILLEGAL_NAME};
  }
  try {
    PlanCollection res;
    // Inject certain default values.
    // This will make sure the default configuration for Sharding attributes are
    // applied
    res.numberOfShards = defaultValues.numberOfShards;
    res.replicationFactor = defaultValues.replicationFactor;
    res.writeConcern = defaultValues.writeConcern;

    auto status = velocypack::deserializeWithStatus(input, res);
    if (status.ok()) {
      return res;
    }
    if (status.path() == "name") {
      // Special handling to be backwards compatible error reporting
      // on "name"
      return Result{TRI_ERROR_ARANGO_ILLEGAL_NAME};
    }
    return Result{
        TRI_ERROR_BAD_PARAMETER,
        status.error() +
            (status.path().empty() ? "" : " on path " + status.path())};
  } catch (basics::Exception const& e) {
    return Result{e.code(), e.message()};
  } catch (std::exception const& e) {
    return Result{TRI_ERROR_INTERNAL, e.what()};
  }
}

auto PlanCollection::Invariants::isNonEmpty(std::string const& value)
    -> inspection::Status {
  if (value.empty()) {
    return {"Value cannot be empty."};
  }
  return inspection::Status::Success{};
}

auto PlanCollection::Invariants::isGreaterZero(uint64_t const& value)
    -> inspection::Status {
  if (value > 0) {
    return inspection::Status::Success{};
  }
  return {"Value has to be > 0"};
}

auto PlanCollection::Invariants::isValidShardingStrategy(
    std::string const& strat) -> inspection::Status {
  // Note we may be better off with a lookup list here
  // Hash is first on purpose (default)
  if (strat == "hash" || strat == "enterprise-hash-smart-edge" ||
      strat == "community-compat" || strat == "enterprise-compat" ||
      strat == "enterprise-smart-edge-compat") {
    return inspection::Status::Success{};
  }
  return {
      "Please use 'hash' or remove, advanced users please "
      "pick a strategy from the documentation, " +
      strat + " is not allowed."};
}

auto PlanCollection::Invariants::isValidCollectionType(
    std::underlying_type_t<TRI_col_type_e> const& type) -> inspection::Status {
  if (type == TRI_col_type_e::TRI_COL_TYPE_DOCUMENT ||
      type == TRI_col_type_e::TRI_COL_TYPE_EDGE) {
    return inspection::Status::Success{};
  }
  return {"Only 2 (document) and 3 (edge) are allowed."};
}

arangodb::velocypack::Builder PlanCollection::toCollectionsCreate() {
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::serialize(builder, *this);
  if (builder.slice().hasKey(StaticStrings::SmartJoinAttribute) &&
      builder.slice()
          .get(StaticStrings::SmartJoinAttribute)
          .isEqualString("")) {
    // TODO: This is a hack to erase the SmartJoin attribute if it was not set
    // We need this to satisfy the API checks in LogicalCollection
    // `initializeSmartAttributes`
    return VPackCollection::remove(
        builder.slice(),
        std::vector<std::string>{StaticStrings::SmartJoinAttribute});
  }
  return builder;
}
