////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

auto PlanCollection::Invariants::isValidCollectionType(
    std::underlying_type_t<TRI_col_type_e> const& type) -> inspection::Status {
  if (type == TRI_col_type_e::TRI_COL_TYPE_DOCUMENT ||
      type == TRI_col_type_e::TRI_COL_TYPE_EDGE) {
    return inspection::Status::Success{};
  }
  return {"Only 2 (document) and 3 (edge) are allowed."};
}

auto PlanCollection::Transformers::ReplicationSatellite::toSerialized(
    MemoryType v, SerializedType& result) -> arangodb::inspection::Status {
  result.add(VPackValue(v));
  return {};
}

auto PlanCollection::Transformers::ReplicationSatellite::fromSerialized(
    SerializedType const& b, MemoryType& result)
    -> arangodb::inspection::Status {
  auto v = b.slice();
  if (v.isString() && v.isEqualString("satellite")) {
    result = 0;
    return {};
  } else if (v.isNumber()) {
    result = v.getNumber<MemoryType>();
    if (result != 0) {
      return {};
    }
  }
  return {"Only an integer number or 'satellite' is allowed"};
}

arangodb::velocypack::Builder PlanCollection::toCollectionsCreate() {
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::serialize(builder, *this);
  // TODO: This is a hack to erase attributes that are not expected by follow up
  // APIS, it should be obsolete after refactoring is completed
  std::vector<std::string> attributesToErase{};
  if (builder.slice().hasKey(StaticStrings::SmartJoinAttribute) &&
      builder.slice()
          .get(StaticStrings::SmartJoinAttribute)
          .isEqualString("")) {
    // TODO: This is a hack to erase the SmartJoin attribute if it was not set
    // We need this to satisfy the API checks in LogicalCollection
    // `initializeSmartAttributes`
    attributesToErase.emplace_back(StaticStrings::SmartJoinAttribute);
  }
  if (builder.slice().hasKey(StaticStrings::ShardingStrategy) &&
      builder.slice().get(StaticStrings::ShardingStrategy).isEqualString("")) {
    // TODO: This is a hack to erase the ShardingStrategy attribute if it was
    // not set
    attributesToErase.emplace_back(StaticStrings::ShardingStrategy);
  }

  if (builder.slice().hasKey(StaticStrings::GraphSmartGraphAttribute) &&
      builder.slice()
          .get(StaticStrings::GraphSmartGraphAttribute)
          .isEqualString("")) {
    // TODO: This is a hack to erase the SmartGraphAttribute attribute if it was
    // not set
    attributesToErase.emplace_back(StaticStrings::GraphSmartGraphAttribute);
  }
  if (!attributesToErase.empty()) {
    return VPackCollection::remove(builder.slice(), attributesToErase);
  }
  return builder;
}
