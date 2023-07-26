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

#include "Cluster/ServerState.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"
#include "Basics/VelocyPackHelper.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

#include <velocypack/Collection.h>
#include <velocypack/Slice.h>

using namespace arangodb;

namespace {

auto rewriteStatusErrorMessage(inspection::Status const& status) -> Result {
  TRI_ASSERT(!status.ok());

  if (status.path() == StaticStrings::DataSourceName) {
    // Special handling to be backwards compatible error reporting
    // on "name"
    return Result{TRI_ERROR_ARANGO_ILLEGAL_NAME};
  }

  if (status.path().rfind(StaticStrings::KeyOptions, 0) == 0) {
    // Special handling to be backwards compatible error reporting
    // on "keyOptions"
    return Result{TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR, status.error()};
  }
  if (status.path() == StaticStrings::SmartJoinAttribute) {
    return Result{TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE, status.error()};
  }

  if (status.path() == StaticStrings::Schema) {
    // Schema should return a validation bad parameter, not only bad parameter
    return Result{TRI_ERROR_VALIDATION_BAD_PARAMETER, status.error()};
  }

  return Result{
      TRI_ERROR_BAD_PARAMETER,
      status.error() +
          (status.path().empty() ? "" : " on attribute " + status.path())};
}

auto rewriteStatusErrorMessageForRestore(inspection::Status const& status)
    -> Result {
  TRI_ASSERT(!status.ok());

  if (status.path() == StaticStrings::DataSourceName) {
    // Special handling to be backwards compatible error reporting
    // on "name"
    return Result{TRI_ERROR_ARANGO_ILLEGAL_NAME};
  }

  if (status.path() == StaticStrings::DataSourceType) {
    // Special handling to be backwards compatible error reporting
    // on "name"
    return Result{TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID};
  }

  if (status.path().rfind(StaticStrings::KeyOptions, 0) == 0) {
    // Special handling to be backwards compatible error reporting
    // on "keyOptions"
    return Result{TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR, status.error()};
  }
  if (status.path() == StaticStrings::SmartJoinAttribute) {
    return Result{TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE, status.error()};
  }

  if (status.path() == StaticStrings::Schema) {
    // Schema should return a validation bad parameter, not only bad parameter
    return Result{TRI_ERROR_VALIDATION_BAD_PARAMETER, status.error()};
  }

  return Result{
      TRI_ERROR_BAD_PARAMETER,
      status.error() +
          (status.path().empty() ? "" : " on attribute " + status.path())};
}

auto handleShards(std::string_view, VPackSlice value, VPackSlice fullBody,
                  DatabaseConfiguration const& config, VPackBuilder& result) {
  if (value.isObject() && !value.isEmptyObject() &&
      !fullBody.hasKey(StaticStrings::NumberOfShards)) {
    // If we get a valid list of Shards we use them to define the number of
    // shards. unless they are explicitly given in the input.
    result.add(StaticStrings::NumberOfShards, VPackValue(value.length()));
  }
}

auto isSingleServer() -> bool {
  return ServerState::instance()->isSingleServer();
}

bool isSmart(VPackSlice fullBody) {
#ifdef USE_ENTERPRISE
  return basics::VelocyPackHelper::getBooleanValue(
      fullBody, StaticStrings::IsSmart, false);
#else
  return false;
#endif
}

bool hasDistributeShardsLike(VPackSlice fullBody,
                             DatabaseConfiguration const& config) {
  // Only true if we have a non-empty string as DistributeShardsLike
  // Always false on SingleServer.
  return config.isOneShardDB ||
         (fullBody.hasKey(StaticStrings::DistributeShardsLike) &&
          fullBody.get(StaticStrings::DistributeShardsLike).isString() &&
          !fullBody.get(StaticStrings::DistributeShardsLike)
               .stringView()
               .empty());
}

bool shouldConsiderClusterAttribute(VPackSlice fullBody,
                                    DatabaseConfiguration const& config) {
  if (isSingleServer()) {
    // To simulate smart collections on SingleServer we need to consider cluster
    // attributes distributeShardsLike will be ignored anyway
    return isSmart(fullBody);
  } else {
    // DistributeShardsLike does superseed cluster attributes
    return !hasDistributeShardsLike(fullBody, config);
  }
}

#ifdef USE_ENTERPRISE
// This method is only used in Enterprise.
// It does not have any Enterprise requirements by itself.

bool isEdgeCollection(VPackSlice fullBody) {
  // Only true if we have a non-empty string as DistributeShardsLike
  auto type = fullBody.get(StaticStrings::DataSourceType);
  return type.isNumber() && type.getNumericValue<TRI_col_type_e>() ==
                                TRI_col_type_e::TRI_COL_TYPE_EDGE;
}
#endif

auto justKeep(std::string_view key, VPackSlice value, VPackSlice,
              DatabaseConfiguration const& config, VPackBuilder& result) {
  result.add(key, value);
}

auto handleType(std::string_view key, VPackSlice value, VPackSlice,
                DatabaseConfiguration const& config, VPackBuilder& result) {
  if (value.isString() && value.isEqualString("edge")) {
    // String edge translates to edge type
    result.add(key, VPackValue(TRI_COL_TYPE_EDGE));
  } else if (value.isNumber() &&
             value.getNumericValue<TRI_col_type_e>() == TRI_COL_TYPE_EDGE) {
    // Transform correct value for Edge to edge type
    result.add(key, VPackValue(TRI_COL_TYPE_EDGE));
  } else {
    // Just default copy original value, should be safe
    result.add(key, VPackValue(TRI_COL_TYPE_DOCUMENT));
  }
}

auto handleReplicationFactor(std::string_view key, VPackSlice value,
                             VPackSlice fullBody,
                             DatabaseConfiguration const& config,
                             VPackBuilder& result) {
  if (shouldConsiderClusterAttribute(fullBody, config)) {
    if (value.isNumber() && value.getNumericValue<int64_t>() == 0) {
      // Translate 0 to satellite
      result.add(key, VPackValue(StaticStrings::Satellite));
    } else {
      // All others just forward
      result.add(key, value);
    }
  }
  // Ignore if we have distributeShardsLike
}

auto handleReplicationFactorRestore(std::string_view key, VPackSlice value,
                                    VPackSlice fullBody,
                                    DatabaseConfiguration const& config,
                                    VPackBuilder& result) {
  if (shouldConsiderClusterAttribute(fullBody, config)) {
    if (value.isNumber()) {
      if (value.getNumericValue<int64_t>() == 0) {
        result.add(key, VPackValue(StaticStrings::Satellite));
      } else {
        // All others numbers just forward
        result.add(key, value);
      }
    } else if (value.isString() &&
               value.isEqualString(StaticStrings::Satellite)) {
      // Keep Satellite
      result.add(key, value);
    }
  }
  // Ignore if we have distributeShardsLike, or if none of above conditions
  // matches
}

auto handleBoolOnly(std::string_view key, VPackSlice value, VPackSlice,
                    DatabaseConfiguration const& config, VPackBuilder& result) {
  if (value.isBoolean()) {
    result.add(key, value);
  }
  // Ignore anything else
}

auto handleNumbersOnly(std::string_view key, VPackSlice value, VPackSlice,
                       DatabaseConfiguration const&, VPackBuilder& result) {
  if (value.isNumber()) {
    result.add(key, value);
  }
  // Ignore anything else
}

auto handleWriteConcern(std::string_view key, VPackSlice value,
                        VPackSlice fullBody,
                        DatabaseConfiguration const& config,
                        VPackBuilder& result) {
  if (!isSingleServer()) {
    // Just take in cluster
    result.add(key, value);
  }
}

auto handleWriteConcernRestore(std::string_view key, VPackSlice value,
                               VPackSlice fullBody,
                               DatabaseConfiguration const& config,
                               VPackBuilder& result) {
  if (!isSingleServer()) {
    // Only take numbers in Cluster
    handleNumbersOnly(key, value, fullBody, config, result);
  }
}

auto handleNumberOfShards(std::string_view key, VPackSlice value,
                          VPackSlice fullBody,
                          DatabaseConfiguration const& config,
                          VPackBuilder& result) {
  if (!hasDistributeShardsLike(fullBody, config) || isSmart(fullBody)) {
    justKeep(key, value, fullBody, config, result);
  } else if (config.maxNumberOfShards > 0 && value.isNumber() &&
             value.getNumericValue<uint32_t>() > config.maxNumberOfShards) {
    // If we restrict the number of Shards, and we get over this limit, we keep
    // the value, such that we trigger the according error message.
    result.add(key, value);
  }
  // Just ignore if we have distributeShardsLike
}

auto handleNumberOfShardsRestore(std::string_view key, VPackSlice value,
                                 VPackSlice fullBody,
                                 DatabaseConfiguration const& config,
                                 VPackBuilder& result) {
  if (!hasDistributeShardsLike(fullBody, config) || isSmart(fullBody)) {
    if (isSingleServer()) {
      justKeep(key, value, fullBody, config, result);
    } else {
      handleNumbersOnly(key, value, fullBody, config, result);
    }
  } else if (config.maxNumberOfShards > 0 && value.isNumber() &&
             value.getNumericValue<uint32_t>() > config.maxNumberOfShards) {
    // If we restrict the number of Shards, and we get over this limit, we keep
    // the value, such that we trigger the according error message.
    result.add(key, value);
  }
  // Just ignore if we have distributeShardsLike
}

auto handleComputedValuesRestore(std::string_view key, VPackSlice value,
                                 VPackSlice fullBody,
                                 DatabaseConfiguration const& config,
                                 VPackBuilder& result) {
  justKeep(key, value, fullBody, config, result);
}

auto handleOnlyObjects(std::string_view key, VPackSlice value, VPackSlice,
                       DatabaseConfiguration const& config,
                       VPackBuilder& result) {
  if (value.isObject()) {
    result.add(key, value);
  }
  // Ignore anything else
}

auto handleStringsOnly(std::string_view key, VPackSlice value, VPackSlice,
                       DatabaseConfiguration const& config,
                       VPackBuilder& result) {
  if (value.isString()) {
    result.add(key, value);
  }
  // Ignore anything else
}

auto handleDistributeShardsLike(std::string_view key, VPackSlice value,
                                VPackSlice fullBody,
                                DatabaseConfiguration const& config,
                                VPackBuilder& result) {
  if (!config.isOneShardDB) {
    if (isSingleServer() && !isSmart(fullBody)) {
      // Community can not use distributeShardsLike on SingleServer
      return;
    }
    justKeep(key, value, fullBody, config, result);
  }
  // In oneShardDb distributeShardsLike is forced.
}

auto handleSmartGraphAttribute(std::string_view key, VPackSlice value,
                               VPackSlice fullBody,
                               DatabaseConfiguration const& config,
                               VPackBuilder& result) {
#ifdef USE_ENTERPRISE
  if (!isEdgeCollection(fullBody)) {
    // Only allow smartGraphAttribute if we do not have an edge collection
    justKeep(key, value, fullBody, config, result);
  }
#endif
  // Ignore anything else
}

auto handleSmartJoinAttribute(std::string_view key, VPackSlice value,
                              VPackSlice fullBody,
                              DatabaseConfiguration const& config,
                              VPackBuilder& result) {
#ifdef USE_ENTERPRISE
  if (!isSingleServer()) {
    // Only allow smartJoinAttribute if we are no single server
    justKeep(key, value, fullBody, config, result);
  }
#endif
  // Ignore anything else
}

auto handleShardKeys(std::string_view key, VPackSlice value,
                     VPackSlice fullBody, DatabaseConfiguration const& config,
                     VPackBuilder& result) {
  if (!isSingleServer()) {
    // Cluster needs to handle all shardKeys and eventually reject
    justKeep(key, value, fullBody, config, result);
  } else if (!value.isArray() || isSmart(fullBody)) {
    // Single server checks if shardKeys look valid.
    // But it will never take their value.
    // Hence we keep invalid one, to produce errors,
    // but drp valid entries.
    justKeep(key, value, fullBody, config, result);
  }
}

auto handleIsSmart(std::string_view key, VPackSlice value, VPackSlice fullBody,
                   DatabaseConfiguration const& config, VPackBuilder& result) {
#ifdef USE_ENTERPRISE
  handleBoolOnly(key, value, fullBody, config, result);
#endif
  // Just ignore isSmart on community
}

auto handleShardingStrategy(std::string_view key, VPackSlice value,
                            VPackSlice fullBody,
                            DatabaseConfiguration const& config,
                            VPackBuilder& result) {
  if (!isSingleServer()) {
    handleStringsOnly(key, value, fullBody, config, result);
  }
  // Just ignore on SingleServer
}

auto handleShardingStrategyRestore(std::string_view key, VPackSlice value,
                                   VPackSlice fullBody,
                                   DatabaseConfiguration const& config,
                                   VPackBuilder& result) {
  if (!isSingleServer()) {
    if (value.isString()) {
      // We only handle strings here.
#ifndef USE_ENTERPRISE
      if (value.isEqualString("enterprise-hash-smart-edge")) {
        // Only enterprise can use this strategy, on community we ignore it.
        return;
      }
      if (value.isEqualString("enterprise-hex-smart-vertex")) {
        // Only enterprise can use this strategy, on community we use "hash"
        // here, but not the general default of community-compat
        result.add(key, "hash");
      }
#endif
    }
    handleStringsOnly(key, value, fullBody, config, result);
  } else {
    if (isSmart(fullBody)) {
      // We need to keep exactly this strategy to trigger a BAD_PARAMETER error.
      // All others are ignored.
      justKeep(key, value, fullBody, config, result);
    }
  }
  // Just ignore on SingleServer
}

auto logDeprecationMessage(Result const& res) -> void {
  LOG_TOPIC("ee638", ERR, arangodb::Logger::DEPRECATION)
      << "The createCollection request contains an illegal combination "
         "and "
         "will be rejected in the future: "
      << res;
}

auto makeRestoreAllowList() -> std::unordered_map<
    std::string_view,
    std::function<void(std::string_view key, VPackSlice value,
                       VPackSlice original, DatabaseConfiguration const& config,
                       VPackBuilder& result)>> const& {
  // This is the additional allowList for the restoreCollection request.
  // It is designed to be used in addition to the general allowList,
  // where the restoreAllowList takes precedence.
  static std::unordered_map<
      std::string_view,
      std::function<void(
          std::string_view key, VPackSlice value, VPackSlice original,
          DatabaseConfiguration const& config, VPackBuilder& result)>>
      allowListInstance{
          {StaticStrings::DataSourceType, handleNumbersOnly},
          {"shards", handleShards},
          {StaticStrings::MinReplicationFactor, handleWriteConcernRestore},
          {StaticStrings::WriteConcern, handleWriteConcernRestore},
          {StaticStrings::NumberOfShards, handleNumberOfShardsRestore},
          {StaticStrings::ComputedValues, handleComputedValuesRestore},
          {StaticStrings::ShardingStrategy, handleShardingStrategyRestore},
          {StaticStrings::ReplicationFactor, handleReplicationFactorRestore}};
  return allowListInstance;
}

auto makeAllowList() -> std::unordered_map<
    std::string_view,
    std::function<void(std::string_view key, VPackSlice value,
                       VPackSlice original, DatabaseConfiguration const& config,
                       VPackBuilder& result)>> const& {
  static std::unordered_map<
      std::string_view,
      std::function<void(
          std::string_view key, VPackSlice value, VPackSlice original,
          DatabaseConfiguration const& config, VPackBuilder& result)>>
      allowListInstance{
          // CollectionConstantProperties
          {StaticStrings::DataSourceSystem, handleBoolOnly},
          {StaticStrings::IsSmart, handleIsSmart},
          {StaticStrings::IsDisjoint, handleBoolOnly},
          {StaticStrings::CacheEnabled, handleBoolOnly},
          {StaticStrings::GraphSmartGraphAttribute, handleSmartGraphAttribute},
          {StaticStrings::SmartJoinAttribute, handleSmartJoinAttribute},
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
          {StaticStrings::ShardingStrategy, handleShardingStrategy},
          {StaticStrings::ShardKeys, handleShardKeys},
          {StaticStrings::DistributeShardsLike, handleDistributeShardsLike},

          // Collection Create Options
          {"avoidServers", justKeep}};
  return allowListInstance;
}

/**
 * Transform an illegal inbound body into a legal one, honoring the exact
 * behaviour of 3.11 version.
 *
 * @param body The original body
 * @param parsingResult The error result returned by
 * @return
 */
auto transformFromBackwardsCompatibleBody(VPackSlice body,
                                          DatabaseConfiguration const& config,
                                          Result parsingResult)
    -> ResultT<VPackBuilder> {
  TRI_ASSERT(parsingResult.fail());
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
        handler->second(key.stringView(), value, body, config, result);
      }
    }
  }
  return result;
}

/**
 * Transform an illegal inbound body into a legal one, honoring the exact
 * behaviour of 3.11 version, for Restore api
 *
 * @param body The original body
 * @param parsingResult The error result returned by
 * @return
 */
auto transformFromBackwardsCompatibleRestoreBody(
    VPackSlice body, DatabaseConfiguration const& config, Result parsingResult)
    -> ResultT<VPackBuilder> {
  TRI_ASSERT(parsingResult.fail());
  VPackBuilder result;
  {
    VPackObjectBuilder objectGuard(&result);
    auto preferList = makeRestoreAllowList();
    auto keyAllowList = makeAllowList();
    for (auto const& [key, value] : VPackObjectIterator(body)) {
      // Special handling of some keys that behave differently then in
      // createCollectionAPI.
      auto preferHandler = preferList.find(key.stringView());
      if (preferHandler != preferList.end()) {
        preferHandler->second(key.stringView(), value, body, config, result);
        continue;
      }
      auto handler = keyAllowList.find(key.stringView());
      if (handler == keyAllowList.end()) {
        // We ignore all keys we are not aware of
        continue;
      } else {
        // All others have implemented a handler
        handler->second(key.stringView(), value, body, config, result);
      }
    }
  }
  return result;
}

#ifndef USE_ENTERPRISE
Result validateEnterpriseFeaturesNotUsed(CreateCollectionBody const& body) {
  if (body.isSatellite()) {
    return Result{
        TRI_ERROR_ONLY_ENTERPRISE,
        "satellite collections or only available in enterprise version"};
  }
  if (body.isSmart || body.isSmartChild) {
    return Result{TRI_ERROR_ONLY_ENTERPRISE,
                  "SmartGraphs or only available in enterprise version"};
  }
  return {TRI_ERROR_NO_ERROR};
}
#endif

ResultT<CreateCollectionBody> parseAndValidate(
    DatabaseConfiguration const& config, VPackSlice input,
    std::function<void(CreateCollectionBody&)> applyDefaults,
    std::function<Result(inspection::Status const&)> const& statusToResult,
    std::function<void(CreateCollectionBody&)> applyCompatibilityHacks) {
  try {
    CreateCollectionBody res;
    applyDefaults(res);
    auto status =
        velocypack::deserializeWithStatus(input, res, {}, InspectUserContext{});
    if (status.ok()) {
      applyCompatibilityHacks(res);
      // Inject default values, and finally check if collection is allowed
      auto result = res.applyDefaultsAndValidateDatabaseConfiguration(config);
      if (result.fail()) {
        return result;
      }
#ifndef USE_ENTERPRISE
      result = validateEnterpriseFeaturesNotUsed(res);
      if (result.fail()) {
        return result;
      }
#endif
      return res;
    }
    return statusToResult(status);
  } catch (basics::Exception const& e) {
    return Result{e.code(), e.message()};
  } catch (std::exception const& e) {
    return Result{TRI_ERROR_INTERNAL, e.what()};
  }
}

}  // namespace

CreateCollectionBody::CreateCollectionBody() = default;

ResultT<CreateCollectionBody> CreateCollectionBody::fromCreateAPIBody(
    VPackSlice input, DatabaseConfiguration const& config,
    bool activateBackwardsCompatibility) {
  if (!input.isObject()) {
    // Special handling to be backwards compatible error reporting
    // on "name"
    return Result{TRI_ERROR_ARANGO_ILLEGAL_NAME};
  }
  auto res = ::parseAndValidate(
      config, input, [](CreateCollectionBody& col) {},
      ::rewriteStatusErrorMessage, [](CreateCollectionBody& col) {});
  if (activateBackwardsCompatibility && res.fail()) {
    auto newBody =
        transformFromBackwardsCompatibleBody(input, config, res.result());
    if (newBody.fail()) {
      return newBody.result();
    }
    auto compatibleRes = ::parseAndValidate(
        config, newBody->slice(), [](CreateCollectionBody& col) {},
        ::rewriteStatusErrorMessage, [](CreateCollectionBody& col) {});
    if (compatibleRes.ok()) {
      logDeprecationMessage(res.result());
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
  auto res = ::parseAndValidate(
      config, properties,
      [&name, &type](CreateCollectionBody& col) {
        // Inject the default values given by V8 in a
        // separate parameter
        col.type = type;
        col.name = name;
      },
      ::rewriteStatusErrorMessage,
      [](CreateCollectionBody& col) {
#ifdef USE_ENTERPRISE
        if (col.isDisjoint) {
          // We don't support disjoint collection creation for enterprise
          // over V8API
          col.isDisjoint = false;
        }
#endif
      });
  if (res.fail()) {
    auto newBody =
        transformFromBackwardsCompatibleBody(properties, config, res.result());
    if (newBody.fail()) {
      return newBody.result();
    }
    auto compatibleRes = ::parseAndValidate(
        config, newBody->slice(),
        [&name, &type](CreateCollectionBody& col) {
          // Inject the default values given by V8 in a
          // separate parameter
          col.type = type;
          col.name = name;
        },
        ::rewriteStatusErrorMessage,
        [](CreateCollectionBody& col) {
#ifdef USE_ENTERPRISE
          if (col.isDisjoint) {
            // We don't support disjoint collection creation for enterprise
            // over V8API
            col.isDisjoint = false;
          }
#endif
        });
    if (compatibleRes.ok()) {
      logDeprecationMessage(res.result());
    }
    return compatibleRes;
  }
  return res;
}

ResultT<CreateCollectionBody> CreateCollectionBody::fromRestoreAPIBody(
    VPackSlice input, DatabaseConfiguration const& config) {
  auto res = ::parseAndValidate(
      config, input, [](CreateCollectionBody& col) {},
      ::rewriteStatusErrorMessageForRestore,
      [&config](CreateCollectionBody& col) {
        // By all means, we cannot take an id from the outside. We need to generate an ID here.
        // So better waste one ID than having a collision.
        col.id = config.idGenerator();
        if (!col.shardingStrategy.has_value() &&
            !col.distributeShardsLike.has_value() &&
            config.defaultDistributeShardsLike.empty()) {
#if USE_ENTERPRISE
          col.shardingStrategy = "enterprise-compat";
#else
          col.shardingStrategy = "community-compat";
#endif
        }
      });

  if (res.fail()) {
    auto newBody = transformFromBackwardsCompatibleRestoreBody(input, config,
                                                               res.result());
    if (newBody.fail()) {
      return newBody.result();
    }
    // NOTE: We do not log a deprecation message here. The restore API is
    // forever backwards compatible.
    return ::parseAndValidate(
        config, newBody->slice(), [](CreateCollectionBody& col) {},
        ::rewriteStatusErrorMessageForRestore,
        [&config](CreateCollectionBody& col) {
          // By all means, we cannot take an id from the outside. We need to generate an ID here.
          // So better waste one ID than having a collision.
          col.id = config.idGenerator();
          if (!col.shardingStrategy.has_value() &&
              !col.distributeShardsLike.has_value() &&
              config.defaultDistributeShardsLike.empty()) {
#if USE_ENTERPRISE
            col.shardingStrategy = "enterprise-compat";
#else
            col.shardingStrategy = "community-compat";
#endif
          }
        });
  }
  return res;
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
