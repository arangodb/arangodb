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

#include "CreateCollectionBody.h"

#include "VocBase/Properties/DatabaseConfiguration.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"

#include <velocypack/Collection.h>
#include <velocypack/Slice.h>

using namespace arangodb;

namespace {

bool hasDistributeShardsLike(VPackSlice fullBody) {
  // Only true if we have a non-empty string as DistributeShardsLike
  return fullBody.hasKey(StaticStrings::DistributeShardsLike) &&
         fullBody.get(StaticStrings::DistributeShardsLike).isString() &&
         !fullBody.get(StaticStrings::DistributeShardsLike)
              .stringView()
              .empty();
}


auto justKeep(std::string_view key, VPackSlice value, VPackSlice, VPackBuilder& result) {
  result.add(key, value);
}

auto handleType(std::string_view key, VPackSlice value, VPackSlice, VPackBuilder& result) {
  if (value.isString() && value.isEqualString("edge")) {
    // String edge translates to edge type
    result.add(key, VPackValue(TRI_COL_TYPE_EDGE));
  } else if (value.isNumber() && value.getNumericValue<TRI_col_type_e>() == TRI_COL_TYPE_EDGE) {
    // Transform correct value for Edge to edge type
    result.add(key, VPackValue(TRI_COL_TYPE_EDGE));
  } else {
    // Just default copy original value, should be safe
    result.add(key, VPackValue(TRI_COL_TYPE_DOCUMENT));
  }
}

auto handleReplicationFactor(std::string_view key, VPackSlice value, VPackSlice fullBody, VPackBuilder& result) {
  if (!hasDistributeShardsLike(fullBody)) {
    if (value.isNumber()) {
      if (value.getNumericValue<int64_t>() == 0) {
        result.add(key, VPackValue(StaticStrings::Satellite));
      } else if (value.isInteger() && value.getNumericValue<int64_t>() > 0) {
        // Just forward
        result.add(key, value);
      }
    }
    // Ignore other entries
  }
  // Ignore if we have distributeShardsLike
}

auto handleBoolOnly(std::string_view key, VPackSlice value, VPackSlice, VPackBuilder& result) {
  if (value.isBoolean()) {
    result.add(key, value);
  }
  // Ignore anything else
}

auto handleOnlyPositiveIntegers(std::string_view key, VPackSlice value, VPackSlice, VPackBuilder& result) {
  if (value.isNumber() && value.isInteger() && value.getNumericValue<int64_t>() > 0) {
    result.add(key, value);
  }
  // Ignore anything else
}

auto handleWriteConcern(std::string_view key, VPackSlice value, VPackSlice fullBody, VPackBuilder& result) {
  if (!hasDistributeShardsLike(fullBody)) {
    handleOnlyPositiveIntegers(key, value, fullBody, result);
  }
  // Just ignore if we have distributeShardsLike
}

auto handleNumberOfShards(std::string_view key, VPackSlice value, VPackSlice fullBody, VPackBuilder& result) {
  if (!hasDistributeShardsLike(fullBody)) {
    justKeep(key, value, fullBody, result);
  }
  // Just ignore if we have distributeShardsLike
}

auto handleOnlyObjects(std::string_view key, VPackSlice value, VPackSlice, VPackBuilder& result) {
  if (value.isObject()) {
    result.add(key, value);
  }
  // Ignore anything else
}

auto handleStringsOnly(std::string_view key, VPackSlice value, VPackSlice, VPackBuilder& result) {
  if (value.isString()) {
    result.add(key, value);
  }
  // Ignore anything else
}


auto makeAllowList() -> std::unordered_map<std::string_view, std::function<void(std::string_view key, VPackSlice value, VPackSlice original, VPackBuilder& result)>> {
  return {// CollectionConstantProperties
          {StaticStrings::DataSourceSystem, handleBoolOnly},
          {StaticStrings::IsSmart, handleBoolOnly},
          {StaticStrings::IsDisjoint, handleBoolOnly},
          {StaticStrings::CacheEnabled, handleBoolOnly},
          {StaticStrings::GraphSmartGraphAttribute, justKeep},
          {StaticStrings::SmartJoinAttribute, justKeep},
          {StaticStrings::DataSourceType, handleType},
          {StaticStrings::KeyOptions, handleOnlyObjects},

          // CollectionMutableProperties
          {StaticStrings::DataSourceName, justKeep},
          {StaticStrings::Schema, justKeep},
          {StaticStrings::ComputedValues, justKeep},

          // CollectionInternalProperties,
          {StaticStrings::Id, justKeep},
          {StaticStrings::SyncByRevision, handleBoolOnly},
          {StaticStrings::UsesRevisionsAsDocumentIds, justKeep},
          {StaticStrings::IsSmartChild, justKeep},
          {StaticStrings::DataSourceDeleted, justKeep},
          {StaticStrings::InternalValidatorTypes, justKeep},

          // ClusteringMutableProperties
          {StaticStrings::WaitForSyncString, handleBoolOnly},
          {StaticStrings::ReplicationFactor, handleReplicationFactor},
          {StaticStrings::MinReplicationFactor, handleWriteConcern},
          {StaticStrings::WriteConcern, handleWriteConcern},

          // ClusteringConstantProperties
          {StaticStrings::NumberOfShards, handleNumberOfShards},
          {StaticStrings::ShardingStrategy, handleStringsOnly},
          {StaticStrings::ShardKeys, justKeep},
          {StaticStrings::DistributeShardsLike, handleStringsOnly},

          // Collection Create Options
          {"avoidServers", justKeep}};
}

/**
 * Transform an illegal inbound body into a legal one, honoring the exact behaviour
 * of 3.11 version.
 *
 * @param body The original body
 * @param parsingResult The error result returned by
 * @return
 */
auto transformFromBackwardsCompatibleBody(VPackSlice body, Result parsingResult) -> ResultT<VPackBuilder> {
  TRI_ASSERT(parsingResult.fail());
//  LOG_DEVEL << "Start tranforming body " << body.toJson() << " into a legal one";
  VPackBuilder result;
  {
    VPackObjectBuilder objectGuard(&result);
    auto keyAllowList = makeAllowList();
    for (auto const& [key, value] : VPackObjectIterator(body)) {
      auto handler = keyAllowList.find(key.stringView());
      if (handler == keyAllowList.end()) {
        // We ignore all keys we are not aware of
        continue;
      } else {
        // All others have implemented a handler
        handler->second(key.stringView(), value, body, result);
      }
    }
  }
//  LOG_DEVEL << "Transformed body " << body.toJson() << " into" << result.slice().toJson();
  return result;
}
}

CreateCollectionBody::CreateCollectionBody() {}

ResultT<CreateCollectionBody> parseAndValidate(
    DatabaseConfiguration const& config, VPackSlice input,
    std::function<void(CreateCollectionBody&)> applyDefaults) {
  try {
    CreateCollectionBody res;
    applyDefaults(res);
    auto status =
        velocypack::deserializeWithStatus(input, res, {}, InspectUserContext{});
    if (status.ok()) {
      // Inject default values, and finally check if collection is allowed
      auto result = res.applyDefaultsAndValidateDatabaseConfiguration(config);
      if (result.fail()) {
        return result;
      }
      return res;
    }
    if (status.path() == "name") {
      // Special handling to be backwards compatible error reporting
      // on "name"
      return Result{TRI_ERROR_ARANGO_ILLEGAL_NAME};
    }

    if (status.path().rfind("keyOptions", 0) == 0) {
      // Special handling to be backwards compatible error reporting
      // on "keyOptions"
      return Result{TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR, status.error()};
    }
    if (status.path() == StaticStrings::SmartJoinAttribute) {
      return Result{TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE, status.error()};
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

ResultT<CreateCollectionBody> CreateCollectionBody::fromCreateAPIBody(
    VPackSlice input, DatabaseConfiguration const& config) {
  if (!input.isObject()) {
    // Special handling to be backwards compatible error reporting
    // on "name"
    return Result{TRI_ERROR_ARANGO_ILLEGAL_NAME};
  }
  auto res =
      ::parseAndValidate(config, input, [](CreateCollectionBody& col) {});
  if (res.fail()) {
    auto newBody = transformFromBackwardsCompatibleBody(input, res.result());
    if (newBody.fail()) {
      return newBody.result();
    }
    auto compatibleRes = ::parseAndValidate(config, newBody->slice(), [](CreateCollectionBody& col) {});
    if (compatibleRes.ok()) {
        LOG_TOPIC("ee638", ERR, arangodb::Logger::DEPRECATION)
            << "The createCollection request contains an illegal combination and "
               "will be rejected in the future: "
            << res.result();
    }
    return compatibleRes;
  }
  return res;
}

ResultT<CreateCollectionBody> CreateCollectionBody::fromCreateAPIV8(
    VPackSlice properties, std::string const& name, TRI_col_type_e type,
    DatabaseConfiguration const& config) {
  if (name.empty()) {
    // Special handling to be backwards compatible error reporting
    // on "name"
    return Result{TRI_ERROR_ARANGO_ILLEGAL_NAME};
  }
  return ::parseAndValidate(config, properties,
                            [&name, &type](CreateCollectionBody& col) {
                              // Inject the default values given by V8 in a
                              // separate parameter
                              col.type = type;
                              col.name = name;
                            });
}

arangodb::velocypack::Builder
CreateCollectionBody::toCreateCollectionProperties(
    std::vector<CreateCollectionBody> const& collections) {
  arangodb::velocypack::Builder builder;
  VPackArrayBuilder guard{&builder};
  for (auto const& c : collections) {
    // TODO: This is copying multiple times.
    // Nevermind this is only temporary code.
    builder.add(c.toCollectionsCreate().slice());
  }
  return builder;
}

arangodb::velocypack::Builder CreateCollectionBody::toCollectionsCreate()
    const {
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::serializeWithContext(builder, *this,
                                             InspectUserContext{});
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
