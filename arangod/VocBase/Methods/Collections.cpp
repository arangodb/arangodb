////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Collections.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Auth/UserManager.h"
#include "Basics/Exceptions.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/fasthash.h"
#include "Cluster/ClusterCollectionMethods.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Graph/Graph.h"
#include "Graph/GraphManager.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Sharding/ShardingFeature.h"
#include "Sharding/ShardingInfo.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#ifdef USE_V8
#include "Transaction/V8Context.h"
#endif
#include "Utilities/NameValidator.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/ComputedValues.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/CollectionCreationInfo.h"
#include "VocBase/Properties/CreateCollectionBody.h"
#include "VocBase/Properties/DatabaseConfiguration.h"
#include "VocBase/vocbase.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>

#include <string>
#include <string_view>
#include <unordered_set>

using namespace arangodb;
using namespace arangodb::methods;

using Helper = arangodb::basics::VelocyPackHelper;

namespace {
constexpr std::string_view moduleName("collections management");

bool checkIfDefinedAsSatellite(velocypack::Slice properties) {
  if (properties.hasKey(StaticStrings::ReplicationFactor)) {
    if (properties.get(StaticStrings::ReplicationFactor).isNumber()) {
      auto replFactor =
          properties.get(StaticStrings::ReplicationFactor).getNumber<size_t>();
      if (replFactor == 0) {
        return true;
      }
    } else if (properties.get(StaticStrings::ReplicationFactor).isString()) {
      if (properties.get(StaticStrings::ReplicationFactor)
              .isEqualString(StaticStrings::Satellite)) {
        return true;
      }
    }
  }
  return false;
}

bool isLocalCollection(CollectionCreationInfo const& info) {
  return (!ServerState::instance()->isCoordinator() &&
          Helper::stringUInt64(
              info.properties.get(StaticStrings::DataSourcePlanId)) == 0);
}

bool isSystemName(CollectionCreationInfo const& info) {
  return NameValidator::isSystemName(info.name);
}

Result validateCreationInfo(CollectionCreationInfo const& info,
                            TRI_vocbase_t& vocbase,
                            bool isSingleServerSmartGraph,
                            bool enforceReplicationFactor,
                            bool isLocalCollection, bool isSystemName,
                            bool allowSystem = false) {
  // check whether the name of the collection is valid
  bool extendedNames = vocbase.extendedNames();
  if (auto res = CollectionNameValidator::validateName(
          allowSystem, extendedNames, info.name);
      res.fail()) {
    events::CreateCollection(vocbase.name(), info.name, res.errorNumber());
    return res;
  }

  // check the collection type in _info
  if (info.collectionType != TRI_col_type_e::TRI_COL_TYPE_DOCUMENT &&
      info.collectionType != TRI_col_type_e::TRI_COL_TYPE_EDGE) {
    events::CreateCollection(vocbase.name(), info.name,
                             TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    return {TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID};
  }

  // validate number of shards and replication factor
  if (ServerState::instance()->isCoordinator() || isSingleServerSmartGraph) {
    Result res = ShardingInfo::validateShardsAndReplicationFactor(
        info.properties, vocbase.server(), enforceReplicationFactor);
    if (res.fail()) {
      return res;
    }
  }

  std::vector<std::string> shardKeys;
  if (ServerState::instance()->isCoordinator()) {
    bool isSmart = basics::VelocyPackHelper::getBooleanValue(
        info.properties, StaticStrings::IsSmart, false);
    size_t replicationFactor = 1;
    Result res = ShardingInfo::extractReplicationFactor(
        info.properties, isSmart, replicationFactor);
    if (res.ok()) {
      // extract shard keys
      res = ShardingInfo::extractShardKeys(info.properties, replicationFactor,
                                           shardKeys);
    }
    if (res.fail()) {
      return res;
    }
  }

  // validate computed values
  auto result = ComputedValues::buildInstance(
      vocbase, shardKeys, info.properties.get(StaticStrings::ComputedValues),
      transaction::OperationOriginInternal{ComputedValues::moduleName});
  if (result.fail()) {
    return result.result();
  }

  // All collections on a single server should be local collections.
  // A Coordinator should never have local collections.
  // On an Agent, all collections should be local collections.
  // On a DBServer, the only local collections should be system collections
  // (like _statisticsRaw). Non-local (system or not) collections are shards,
  // so don't have system-names, even if they are system collections!
  switch (ServerState::instance()->getRole()) {
    case ServerState::ROLE_SINGLE:
      TRI_ASSERT(isLocalCollection);
      break;
    case ServerState::ROLE_DBSERVER:
      TRI_ASSERT(isLocalCollection == isSystemName);
      break;
    case ServerState::ROLE_COORDINATOR:
      TRI_ASSERT(!isLocalCollection);
      break;
    case ServerState::ROLE_AGENT:
      TRI_ASSERT(isLocalCollection);
      break;
    case ServerState::ROLE_UNDEFINED:
      TRI_ASSERT(false);
  }

  if (isLocalCollection && !isSingleServerSmartGraph) {
    // the combination "isSmart" and replicationFactor "satellite" does not make
    // any sense. note: replicationFactor "satellite" can also be expressed as
    // replicationFactor 0.
    VPackSlice s = info.properties.get(StaticStrings::IsSmart);
    auto replicationFactorSlice =
        info.properties.get(StaticStrings::ReplicationFactor);
    if (s.isTrue() &&
        ((replicationFactorSlice.isNumber() &&
          replicationFactorSlice.getNumber<int>() == 0) ||
         (replicationFactorSlice.isString() &&
          replicationFactorSlice.stringView() == StaticStrings::Satellite))) {
      return {TRI_ERROR_BAD_PARAMETER,
              "invalid combination of 'isSmart' and 'satellite' "
              "replicationFactor"};
    }
  }

  return {};
}

// Returns a builder that combines the information from infos and cluster
// related information.
VPackBuilder createCollectionProperties(
    TRI_vocbase_t const& vocbase,
    std::vector<CollectionCreationInfo> const& infos,
    bool allowEnterpriseCollectionsOnSingleServer) {
  StorageEngine& engine = vocbase.engine();
  VPackBuilder builder;
  VPackBuilder helper;

  builder.openArray();

  for (auto const& info : infos) {
    TRI_ASSERT(builder.isOpenArray());
    TRI_ASSERT(info.properties.isObject());

    helper.clear();
    helper.openObject();
    helper.add(arangodb::StaticStrings::DataSourceType,
               VPackValue(static_cast<int>(info.collectionType)));
    helper.add(arangodb::StaticStrings::DataSourceName, VPackValue(info.name));

    // generate a rocksdb collection object id in case it does not exist
    if (allowEnterpriseCollectionsOnSingleServer) {
      engine.addParametersForNewCollection(helper, info.properties);
    }

    bool addUseRevs = ServerState::instance()->isSingleServerOrCoordinator();
    bool useRevs =
        !info.properties
             .get(arangodb::StaticStrings::UsesRevisionsAsDocumentIds)
             .isFalse() &&
        LogicalCollection::currentVersion() >= LogicalCollection::Version::v37;

    if (addUseRevs) {
      helper.add(arangodb::StaticStrings::UsesRevisionsAsDocumentIds,
                 arangodb::velocypack::Value(useRevs));
    }

    if (!isLocalCollection(info) || allowEnterpriseCollectionsOnSingleServer) {
      auto const& cl = vocbase.server().getFeature<ClusterFeature>();
      auto replicationFactorSlice =
          info.properties.get(StaticStrings::ReplicationFactor);

      if (replicationFactorSlice.isNone()) {
        auto factor = vocbase.replicationFactor();
        if (factor > 0 && isSystemName(info)) {
          factor = std::max(vocbase.replicationFactor(),
                            cl.systemReplicationFactor());
        }
        helper.add(StaticStrings::ReplicationFactor, VPackValue(factor));
      };

      bool isSatellite = checkIfDefinedAsSatellite(info.properties);

      if (!isSystemName(info)) {
        // system-collections will be sharded normally. only user collections
        // will get the forced sharding
        if (vocbase.server().getFeature<ClusterFeature>().forceOneShard() ||
            vocbase.isOneShard()) {
          // force one shard, and force distributeShardsLike to be "_graphs"
          helper.add(StaticStrings::NumberOfShards, VPackValue(1));
          if (!isSatellite) {
            // SatelliteCollections must not be sharded like a
            // non-SatelliteCollection.
            helper.add(StaticStrings::DistributeShardsLike,
                       VPackValue(vocbase.shardingPrototypeName()));
          }
        }
      }

      if (!isSatellite) {
        auto writeConcernSlice =
            info.properties.get(StaticStrings::WriteConcern);
        if (writeConcernSlice
                .isNone()) {  // "minReplicationFactor" deprecated in 3.6
          writeConcernSlice =
              info.properties.get(StaticStrings::MinReplicationFactor);
        }

        if (writeConcernSlice.isNone()) {
          helper.add(StaticStrings::MinReplicationFactor,
                     VPackValue(vocbase.writeConcern()));
          helper.add(StaticStrings::WriteConcern,
                     VPackValue(vocbase.writeConcern()));
        }
      }
    } else {  // single server or agent
      bool distributionSet = false;
#ifdef USE_ENTERPRISE
      TRI_ASSERT(ServerState::instance()->isSingleServer() ||
                 ServerState::instance()->isAgent());
      // Special case for sharded graphs with satellites
      if (info.properties.hasKey(StaticStrings::DistributeShardsLike)) {
        // 1.) Either we distribute like another satellite collection
        auto distributeShardsLikeSlice =
            info.properties.get(StaticStrings::DistributeShardsLike);
        if (distributeShardsLikeSlice.isString()) {
          auto distributeShardsLikeColName =
              distributeShardsLikeSlice.copyString();
          auto coll = vocbase.lookupCollection(distributeShardsLikeColName);
          if (coll != nullptr && coll->isSatellite()) {
            helper.add(StaticStrings::DistributeShardsLike,
                       VPackValue(distributeShardsLikeColName));
            helper.add(StaticStrings::ReplicationFactor,
                       VPackValue(coll->replicationFactor()));
            distributionSet = true;
          }
        }
      } else if (info.properties.hasKey(StaticStrings::ReplicationFactor)) {
        // 2.) OR the collection itself is a satellite collection
        if (info.properties.get(StaticStrings::ReplicationFactor).isString() &&
            info.properties.get(StaticStrings::ReplicationFactor)
                    .copyString() == StaticStrings::Satellite) {
          distributionSet = true;
        }
      }
#endif
      if (!distributionSet) {
        helper.add(
            StaticStrings::DistributeShardsLike,
            VPackSlice::nullSlice());  // delete empty string from info slice
        helper.add(StaticStrings::ReplicationFactor, VPackSlice::nullSlice());
      }
      helper.add(StaticStrings::MinReplicationFactor,
                 VPackSlice::nullSlice());  // deprecated
      helper.add(StaticStrings::WriteConcern, VPackSlice::nullSlice());
    }

    helper.close();

    VPackBuilder merged =
        VPackCollection::merge(info.properties, helper.slice(), false, true);

    bool haveShardingFeature = (ServerState::instance()->isCoordinator() ||
                                allowEnterpriseCollectionsOnSingleServer) &&
                               vocbase.server().hasFeature<ShardingFeature>();
    if (haveShardingFeature &&
        !info.properties.get(StaticStrings::ShardingStrategy).isString()) {
      // NOTE: We need to do this in a second merge as the feature call requires
      // the DataSourceType to be set in the JSON, which has just been done by
      // the call above.
      helper.clear();
      helper.openObject();
      helper.add(StaticStrings::ShardingStrategy,
                 VPackValue(vocbase.server()
                                .getFeature<ShardingFeature>()
                                .getDefaultShardingStrategyForNewCollection(
                                    merged.slice())));
      helper.close();
      merged =
          VPackCollection::merge(merged.slice(), helper.slice(), false, true);
    }

    builder.add(merged.slice());
  }

  TRI_ASSERT(builder.isOpenArray());
  builder.close();

  return builder;
}
}  // namespace

Collections::Context::Context(std::shared_ptr<LogicalCollection> coll)
    : _coll(std::move(coll)), _trx(nullptr), _responsibleForTrx(true) {}

Collections::Context::Context(std::shared_ptr<LogicalCollection> coll,
                              transaction::Methods* trx)
    : _coll(std::move(coll)), _trx(trx), _responsibleForTrx(false) {
  TRI_ASSERT(_trx != nullptr);
}

Collections::Context::~Context() {
  if (_responsibleForTrx && _trx != nullptr) {
    delete _trx;
  }
}

futures::Future<transaction::Methods*> Collections::Context::trx(
    AccessMode::Type const& type, bool embeddable) {
  if (_responsibleForTrx && _trx == nullptr) {
    auto origin = transaction::OperationOriginREST{::moduleName};

#ifdef USE_V8
    auto ctx = transaction::V8Context::createWhenRequired(_coll->vocbase(),
                                                          origin, embeddable);
#else
    auto ctx = transaction::StandaloneContext::create(_coll->vocbase(), origin);
#endif
    auto trx = std::make_unique<SingleCollectionTransaction>(std::move(ctx),
                                                             *_coll, type);

    Result res = co_await trx->beginAsync();

    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
    _trx = trx.release();
  }
  // ADD asserts for running state and locking issues!
  co_return _trx;
}

std::shared_ptr<LogicalCollection> Collections::Context::coll() const {
  return _coll;
}

/// @brief check if a name belongs to a collection
bool Collections::hasName(CollectionNameResolver const& resolver,
                          LogicalCollection const& collection,
                          std::string const& collectionName) {
  if (collectionName == collection.name()) {
    return true;
  }

  if (collectionName == std::to_string(collection.id().id())) {
    return true;
  }

  // Shouldn't it just be: If we are on DBServer we also have to check for
  // global ID name and cid should be the shard.
  if (ServerState::instance()->isCoordinator()) {
    if (collectionName == resolver.getCollectionNameCluster(collection.id())) {
      return true;
    }
    return false;
  }

  return (collectionName == resolver.getCollectionName(collection.id()));
}

std::vector<std::shared_ptr<LogicalCollection>> Collections::sorted(
    TRI_vocbase_t& vocbase) {
  std::vector<std::shared_ptr<LogicalCollection>> result;

  if (ServerState::instance()->isCoordinator()) {
    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
    std::vector<std::shared_ptr<LogicalCollection>> colls =
        ci.getCollections(vocbase.name());

    result.reserve(colls.size());

    for (std::shared_ptr<LogicalCollection>& c : colls) {
      if (!c->deleted()) {
        result.emplace_back(std::move(c));
      }
    }
  } else {
    result = vocbase.collections(false);
  }

  std::sort(result.begin(), result.end(),
            [](std::shared_ptr<LogicalCollection> const& lhs,
               std::shared_ptr<LogicalCollection> const& rhs) -> bool {
              return arangodb::basics::StringUtils::tolower(lhs->name()) <
                     arangodb::basics::StringUtils::tolower(rhs->name());
            });

  return result;
}

void Collections::enumerate(
    TRI_vocbase_t* vocbase,
    std::function<void(std::shared_ptr<LogicalCollection> const&)> const&
        func) {
  if (ServerState::instance()->isCoordinator()) {
    auto& ci = vocbase->server().getFeature<ClusterFeature>().clusterInfo();
    std::vector<std::shared_ptr<LogicalCollection>> colls =
        ci.getCollections(vocbase->name());

    for (std::shared_ptr<LogicalCollection> const& c : colls) {
      if (!c->deleted()) {
        func(c);
      }
    }
  } else {
    for (auto const& c : vocbase->collections(false)) {
      if (!c->deleted()) {
        func(c);
      }
    }
  }
}

/*static*/ Result methods::Collections::lookup(  // find collection
    TRI_vocbase_t const& vocbase,                // vocbase to search
    std::string const& name,                     // collection name
    std::shared_ptr<LogicalCollection>& ret) {
  if (name.empty()) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  if (ServerState::instance()->isCoordinator()) {
    try {
      if (!vocbase.server().hasFeature<ClusterFeature>()) {
        return arangodb::Result(  // result
            TRI_ERROR_INTERNAL,   // code
            "failure to find 'ClusterInfo' instance while searching for "
            "collection"  // message
        );
      }
      auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();

      auto coll = ci.getCollectionNT(vocbase.name(), name);
      if (coll) {
        // check authentication after ensuring the collection exists
        if (!ExecContext::current().canUseCollection(
                vocbase.name(), coll->name(), auth::Level::RO)) {
          return Result(TRI_ERROR_FORBIDDEN,
                        absl::StrCat("No access to collection '", name, "'"));
        }

        ret = std::move(coll);

        return Result();
      } else {
        return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                      "collection not found");
      }
    } catch (basics::Exception const& ex) {
      return Result(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      return Result(TRI_ERROR_INTERNAL,
                    "internal error during collection lookup");
    }

    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  auto coll = vocbase.lookupCollection(name);

  if (coll != nullptr) {
    // check authentication after ensuring the collection exists
    if (!ExecContext::current().canUseCollection(vocbase.name(), coll->name(),
                                                 auth::Level::RO)) {
      return Result(TRI_ERROR_FORBIDDEN,
                    absl::StrCat("No access to collection '", name, "'"));
    }
    try {
      ret = std::move(coll);
    } catch (basics::Exception const& ex) {
      return Result(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      return Result(
          TRI_ERROR_INTERNAL,
          "internal error during collection lookup - canUseCollection");
    }

    return Result();
  }

  return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

[[nodiscard]] arangodb::ResultT<std::vector<std::shared_ptr<LogicalCollection>>>
Collections::create(         // create collection
    TRI_vocbase_t& vocbase,  // collection vocbase
    OperationOptions const& options,
    std::vector<CreateCollectionBody> collections,  // Collections to create
    bool createWaitsForSyncReplication,             // replication wait flag
    bool enforceReplicationFactor,                  // replication factor flag
    bool isNewDatabase, bool allowEnterpriseCollectionsOnSingleServer,
    bool isRestore) {
  // Let's first check if we are allowed to create the collections
  ExecContext const& exec = options.context();
  if (!exec.canUseDatabase(vocbase.name(), auth::Level::RW)) {
    for (auto const& col : collections) {
      events::CreateCollection(vocbase.name(), col.name, TRI_ERROR_FORBIDDEN);
    }
    return arangodb::Result(  // result
        TRI_ERROR_FORBIDDEN,  // code
        absl::StrCat("cannot create collection in ", vocbase.name(), ": ",
                     TRI_errno_string(TRI_ERROR_FORBIDDEN))  // message
    );
  }

  // TODO: Discuss should this be Prod Assert?
  // We are trying to protect against someone creating collections
  // in a database that is to be deleted.
  // Or should this be a possible error?
  TRI_ASSERT(!vocbase.isDangling());

  auto config = vocbase.getDatabaseConfiguration();
  config.enforceReplicationFactor = enforceReplicationFactor;

  auto numberOfPublicCollections = collections.size();
  // NOTE: We use index access here, as collections may be modified
  // during this code
  for (size_t i = 0; i < numberOfPublicCollections; ++i) {
    auto& col = collections[i];
    {
      // TODO: Can be replaced inspection for computed values
      // We use this here to validate the format.
      // But also to include optional properties as default values.
      TRI_ASSERT(col.shardKeys.has_value());
      auto result = ComputedValues::buildInstance(
          vocbase, col.shardKeys.value(), col.computedValues.slice(),
          transaction::OperationOriginInternal{ComputedValues::moduleName});
      if (result.fail()) {
        return result.result();
      }

      {
        if (result.get() != nullptr) {
          // The constructor of ComputedValues wil introduce some internal
          // default properties. Hence we will take the valid output generated
          // by the parsed computed values instance to write into the
          // collection.
          VPackBuilder builder;
          result.get()->toVelocyPack(builder);
          col.computedValues = std::move(builder);
        }
      }

      if (ServerState::instance()->isCoordinator()) {
        // This is only relevant for Coordinators
        appendSmartEdgeCollections(col, collections, config.idGenerator);
      }
    }
  }

  arangodb::ResultT<std::vector<std::shared_ptr<LogicalCollection>>> results{
      TRI_ERROR_INTERNAL};

  if (ServerState::instance()->isCoordinator()) {
    // Here we do have a cluster setup. In that case, we will create many
    // collections in one go (batch-wise).
    results = ClusterCollectionMethods::createCollectionsOnCoordinator(
        vocbase, collections, false, createWaitsForSyncReplication,
        enforceReplicationFactor, isNewDatabase);
    if (results.fail()) {
      for (auto const& info : collections) {
        events::CreateCollection(vocbase.name(), info.name,
                                 results.errorNumber());
      }
      return results;
    }
    if (results->empty()) {
      // TODO: CHeck if this can really happen after refactoring is completed
      // We may be able to guarantee error when nothing could be done.
      for (auto const& info : collections) {
        events::CreateCollection(vocbase.name(), info.name, TRI_ERROR_INTERNAL);
      }
      return Result(TRI_ERROR_INTERNAL, "createCollectionsOnCoordinator");
    }
  } else {
    TRI_ASSERT(ServerState::instance()->isSingleServer() ||
               ServerState::instance()->isDBServer() ||
               ServerState::instance()->isAgent());
    // Here we do have a single server setup, or we're either on a DBServer
    // / Agency. In that case, we're not batching collection creating.
    // Therefore, we need to iterate over the infoSlice and create each
    // collection one by one.
    results = vocbase.createCollections(
        collections, allowEnterpriseCollectionsOnSingleServer);
    if (results.fail()) {
      for (auto const& info : collections) {
        events::CreateCollection(vocbase.name(), info.name,
                                 results.errorNumber());
      }
      return results;
    }
  }

  // Grant access to the collections.
  // This is identical on cluster and SingleServer
  try {
    // in case of success we grant the creating user RW access
    auth::UserManager* um = AuthenticationFeature::instance()->userManager();
    if (um != nullptr && !exec.isSuperuser() && !isRestore) {
      // this should not fail, we can not get here without database RW access
      // however, there may be races for updating the users account, so we try
      // a few times in case of a conflict
      int tries = 0;
      while (true) {
        Result r = um->updateUser(exec.user(), [&](auth::User& entry) {
          for (auto const& col : *results) {
            // do not grant rights on system collections
            if (!col->system()) {
              entry.grantCollection(vocbase.name(), col->name(),
                                    auth::Level::RW);
            }
          }
          return TRI_ERROR_NO_ERROR;
        });
        if (r.ok() || r.is(TRI_ERROR_USER_NOT_FOUND) ||
            r.is(TRI_ERROR_USER_EXTERNAL)) {
          // it seems to be allowed to created collections with an unknown user
          break;
        }
        if (!r.is(TRI_ERROR_ARANGO_CONFLICT) || ++tries == 10) {
          // This could be 116bb again, as soon as "old code" is removed
          LOG_TOPIC("116bc", WARN, Logger::AUTHENTICATION)
              << "Updating user failed with error: " << r.errorMessage()
              << ". giving up!";
          for (auto const& col : *results) {
            events::CreateCollection(vocbase.name(), col->name(),
                                     r.errorNumber());
          }
          return r;
        }
        // try again in case of conflict
        // This could be ff123 again, as soon as "old code" is removed
        LOG_TOPIC("ff124", TRACE, Logger::AUTHENTICATION)
            << "Updating user failed with error: " << r.errorMessage()
            << ". trying again";
      }
    }
  } catch (basics::Exception const& ex) {
    for (auto const& info : collections) {
      events::CreateCollection(vocbase.name(), info.name, ex.code());
    }
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    for (auto const& info : collections) {
      events::CreateCollection(vocbase.name(), info.name, TRI_ERROR_INTERNAL);
    }
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    for (auto const& info : collections) {
      events::CreateCollection(vocbase.name(), info.name, TRI_ERROR_INTERNAL);
    }
    return Result(TRI_ERROR_INTERNAL, "cannot create collection");
  }
  for (auto const& info : collections) {
    if (!ServerState::instance()->isSingleServer()) {
      // don't log here (again) for single servers, because on the single
      // server we will log the creation of each collection inside
      // vocbase::createCollectionWorker
      events::CreateCollection(vocbase.name(), info.name, TRI_ERROR_NO_ERROR);
    }

    // TODO: update auditLog reporting
    OperationResult result(Result(), info.toCollectionsCreate().steal(),
                           options);
    events::PropertyUpdateCollection(vocbase.name(), info.name, result);
  }

  return results;
}

/*static*/ arangodb::Result Collections::createShard(  // create shard
    TRI_vocbase_t& vocbase,                            // collection vocbase
    OperationOptions const& options,
    ShardID const& name,                     // shardName
    TRI_col_type_e collectionType,           // collection type
    arangodb::velocypack::Slice properties,  // collection properties
    std::shared_ptr<LogicalCollection>& ret) {
  TRI_ASSERT(ServerState::instance()->isDBServer());
  // NOTE: This is original Collections::create but we stripped of
  // everything that is not relevant on DBServers.

  if (collectionType != TRI_col_type_e::TRI_COL_TYPE_DOCUMENT &&
      collectionType != TRI_col_type_e::TRI_COL_TYPE_EDGE) {
    events::CreateCollection(vocbase.name(), std::string{name},
                             TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    return TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
  }

  std::vector<CollectionCreationInfo> infos{{name, collectionType, properties}};

  bool allowEnterpriseCollectionsOnSingleServer = false;
  TRI_ASSERT(!vocbase.isDangling());
  {
    // Validate if Collection info is valid (We only have one entry so front is
    // enough) This does actually not matter, we cannot get into a code path
    // where it is read.
    bool enforceReplicationFactor = false;
    bool allowSystem = false;
    // LocalCollection checks if we are trying to create a "real" collection
    // We cannot have this, we have a shard.
    TRI_ASSERT(!isLocalCollection(infos.front()));
    auto res = validateCreationInfo(
        infos.front(), vocbase, allowEnterpriseCollectionsOnSingleServer,
        enforceReplicationFactor, isLocalCollection(infos.front()), false,
        allowSystem);
    if (res.fail()) {
      // Audit Log the error
      events::CreateCollection(vocbase.name(), name, res.errorNumber());
      return res;
    }
  }

  // construct a builder that contains information from all elements of infos
  // and cluster related information
  VPackBuilder builder = createCollectionProperties(
      vocbase, infos, allowEnterpriseCollectionsOnSingleServer);

  VPackSlice infoSlice = builder.slice();

  TRI_ASSERT(infoSlice.isArray());
  TRI_ASSERT(infoSlice.length() == 1);
  TRI_ASSERT(infoSlice.length() == infos.size());

  try {
    auto collections = vocbase.createCollections(
        infoSlice, allowEnterpriseCollectionsOnSingleServer);

    // We do not have user management on Shards.
    // If we ever change this we may need to add permission granting here.
    TRI_ASSERT(AuthenticationFeature::instance()->userManager() == nullptr);

    for (auto const& info : infos) {
      // Add audit logging for the one collection we have
      events::CreateCollection(vocbase.name(), std::string{name},
                               TRI_ERROR_NO_ERROR);
      velocypack::Builder reportBuilder(info.properties);
      OperationResult result(Result(), reportBuilder.steal(), options);
      events::PropertyUpdateCollection(vocbase.name(), name, result);
    }
    // We asked for one shard. It did not throw on creation.
    // So we expect to get exactly one back
    ADB_PROD_ASSERT(collections.size() == 1);
    ret = std::move(collections[0]);
    return TRI_ERROR_NO_ERROR;
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "cannot create collection");
  }
}

void Collections::applySystemCollectionProperties(
    CreateCollectionBody& col, TRI_vocbase_t const& vocbase,
    DatabaseConfiguration const& config, bool isLegacyDatabase) {
  col.isSystem = true;
  // that forces all collections to be on the same physical DBserver
  auto designatedLeaderName = vocbase.isSystem() && !isLegacyDatabase
                                  ? StaticStrings::UsersCollection
                                  : StaticStrings::GraphsCollection;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // This implements some stunt to figure out if we are in a mock environment.
  // If that is the case we may not have all System collections.
  // So we better not enforce distributeShardsLike.
  bool isMock = false;
  if (vocbase.server().hasFeature<EngineSelectorFeature>()) {
    StorageEngine& engine =
        vocbase.server().template getFeature<EngineSelectorFeature>().engine();

    isMock = (engine.typeName() == "Mock");
  } else {
    // We do not even have a full server, can only be a mock.
    isMock = true;
  }

  if (isMock) {
    designatedLeaderName = col.name;
  }
#endif

  if (col.name == designatedLeaderName) {
    // The leading collection needs to define sharding
    col.replicationFactor = vocbase.replicationFactor();
    if (vocbase.server().hasFeature<ClusterFeature>() &&
        vocbase.replicationFactor() != 0) {
      // do not adjust replication factor for satellite collections
      col.replicationFactor =
          (std::max)(col.replicationFactor.value(),
                     static_cast<uint64_t>(vocbase.server()
                                               .getFeature<ClusterFeature>()
                                               .systemReplicationFactor()));
    }
  } else {
    // Others only follow
    col.distributeShardsLike = designatedLeaderName;
  }

  auto res = col.applyDefaultsAndValidateDatabaseConfiguration(config);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(),
        absl::StrCat("Created illegal default system collection attributes: ",
                     res.errorMessage()));
  }
}

void Collections::createSystemCollectionProperties(
    std::string const& collectionName, VPackBuilder& bb,
    TRI_vocbase_t const& vocbase) {
  uint32_t defaultReplicationFactor = vocbase.replicationFactor();
  uint32_t defaultWriteConcern = vocbase.writeConcern();

  if (vocbase.server().hasFeature<ClusterFeature>()) {
    defaultReplicationFactor =
        std::max(defaultReplicationFactor, vocbase.server()
                                               .getFeature<ClusterFeature>()
                                               .systemReplicationFactor());
  }

  {
    VPackObjectBuilder scope(&bb);
    bb.add(StaticStrings::DataSourceSystem, VPackSlice::trueSlice());
    bb.add(StaticStrings::WaitForSyncString, VPackSlice::falseSlice());
    bb.add(StaticStrings::ReplicationFactor,
           VPackValue(defaultReplicationFactor));
    bb.add(StaticStrings::MinReplicationFactor,
           VPackValue(defaultWriteConcern));  // deprecated
    bb.add(StaticStrings::WriteConcern, VPackValue(defaultWriteConcern));

    // that forces all collections to be on the same physical DBserver
    if (vocbase.isSystem()) {
      if (collectionName != StaticStrings::UsersCollection) {
        bb.add(StaticStrings::DistributeShardsLike,
               VPackValue(StaticStrings::UsersCollection));
      }
    } else {
      if (collectionName != StaticStrings::GraphsCollection) {
        bb.add(StaticStrings::DistributeShardsLike,
               VPackValue(StaticStrings::GraphsCollection));
      }
    }
  }
}

/*static*/ Result Collections::createSystem(
    TRI_vocbase_t& vocbase, OperationOptions const& options,
    std::string const& name, bool isNewDatabase,
    std::shared_ptr<LogicalCollection>& createdCollection) {
  Result res = methods::Collections::lookup(vocbase, name, createdCollection);

  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    CreateCollectionBody newCollection;
    newCollection.name = name;
    methods::Collections::applySystemCollectionProperties(
        newCollection, vocbase, vocbase.getDatabaseConfiguration(), false);

    auto collection = Collections::create(vocbase, options, {newCollection},
                                          true,  // waitsForSyncReplication
                                          true,  // enforceReplicationFactor
                                          isNewDatabase);
    // Operation either failed, or we have created exactly one collection.
    TRI_ASSERT(collection.fail() || collection.get().size() == 1);
    if (collection.ok()) {
      createdCollection = collection.get().front();
    }
    return collection.result();
  }

  TRI_ASSERT(res.fail() || createdCollection);
  return res;
}

futures::Future<Result> Collections::properties(Context& ctxt,
                                                VPackBuilder& builder) {
  auto coll = ctxt.coll();
  TRI_ASSERT(coll != nullptr);
  ExecContext const& exec = ExecContext::current();
  bool canRead = exec.canUseCollection(coll->name(), auth::Level::RO);
  if (!canRead || exec.databaseAuthLevel() == auth::Level::NONE) {
    co_return Result(
        TRI_ERROR_FORBIDDEN,
        absl::StrCat("cannot access collection '", coll->name(), "'"));
  }

  std::unordered_set<std::string> ignoreKeys{StaticStrings::AllowUserKeys,
                                             StaticStrings::DataSourceCid,
                                             "count",
                                             StaticStrings::DataSourceDeleted,
                                             StaticStrings::DataSourceId,
                                             StaticStrings::Indexes,
                                             StaticStrings::DataSourceName,
                                             "path",
                                             StaticStrings::DataSourcePlanId,
                                             "shards",
                                             "status",
                                             StaticStrings::DataSourceType,
                                             StaticStrings::Version};

  if (ServerState::instance()->isSingleServer() &&
      (!coll->isSatellite() && !coll->isSmart())) {
    // 1. These are either relevant for cluster
    // 2. Or for collections which have additional cluster properties set for
    // future dump and restore use case. Currently those are supported during
    // graph creation. Therefore, we need those properties for satellite and
    // graph collections.
    ignoreKeys.insert(
        {StaticStrings::DistributeShardsLike, StaticStrings::IsSmart,
         StaticStrings::NumberOfShards, StaticStrings::ReplicationFactor,
         StaticStrings::MinReplicationFactor, StaticStrings::ShardKeys,
         StaticStrings::ShardingStrategy, StaticStrings::IsDisjoint});

    // this transaction is held longer than the following if...
    auto trx = co_await ctxt.trx(AccessMode::Type::READ, true);
    TRI_ASSERT(trx != nullptr);
  }

  // note that we have an ongoing transaction here if we are in single-server
  // case
  VPackBuilder props = coll->toVelocyPackIgnore(
      ignoreKeys, LogicalDataSource::Serialization::Properties);
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackObjectIterator(props.slice()));

  co_return TRI_ERROR_NO_ERROR;
}

futures::Future<Result> Collections::updateProperties(
    LogicalCollection& collection, velocypack::Slice props,
    OperationOptions const& options) {
  ExecContext const& exec = ExecContext::current();
  bool canModify = exec.canUseCollection(collection.name(), auth::Level::RW);

  if (!canModify || !exec.canUseDatabase(auth::Level::RW)) {
    co_return TRI_ERROR_FORBIDDEN;
  }

  if (ServerState::instance()->isCoordinator()) {
    ClusterInfo& ci = collection.vocbase()
                          .server()
                          .getFeature<ClusterFeature>()
                          .clusterInfo();
    auto info = ci.getCollection(collection.vocbase().name(),
                                 std::to_string(collection.id().id()));

    // replication checks
    if (auto s = props.get(StaticStrings::ReplicationFactor); s.isNumber()) {
      int64_t replFactor = Helper::getNumericValue<int64_t>(
          props, StaticStrings::ReplicationFactor, 0);
      if (replFactor > 0 &&
          static_cast<size_t>(replFactor) > ci.getCurrentDBServers().size()) {
        co_return TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS;
      }
    }

    // not an error: for historical reasons the write concern is read from the
    // variable "minReplicationFactor" if it exists
    uint64_t writeConcern =
        Helper::getNumericValue(props, StaticStrings::MinReplicationFactor, 0);
    if (props.hasKey(StaticStrings::WriteConcern)) {
      writeConcern =
          Helper::getNumericValue(props, StaticStrings::WriteConcern, 0);
    }

    // write-concern checks
    if (writeConcern > ci.getCurrentDBServers().size()) {
      TRI_ASSERT(writeConcern > 0);
      co_return TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS;
    }

    Result res = ShardingInfo::validateShardsAndReplicationFactor(
        props, collection.vocbase().server(), false);
    if (res.fail()) {
      co_return res;
    }

    auto rv = info->properties(props);
    if (rv.ok()) {
      velocypack::Builder builder(props);
      OperationResult result(rv, builder.steal(), options);
      events::PropertyUpdateCollection(collection.vocbase().name(),
                                       collection.name(), result);
    }
    co_return rv;

  } else {
    // The following lambda isolates the core of the update operation. This
    // stays the same, regardless of replication version.
    auto doUpdate = [&]() -> Result {
      auto res = collection.properties(props);
      if (res.ok()) {
        velocypack::Builder builder(props);
        OperationResult result(res, builder.steal(), options);
        events::PropertyUpdateCollection(collection.vocbase().name(),
                                         collection.name(), result);
      }
      return res;
    };

    if (collection.replicationVersion() == replication::Version::TWO) {
      // In replication2, the exclusive lock is already acquired.
      co_return doUpdate();
    }

    auto origin =
        transaction::OperationOriginREST{"collection properties update"};
#ifdef USE_V8
    auto ctx = transaction::V8Context::createWhenRequired(collection.vocbase(),
                                                          origin, false);
#else
    auto ctx =
        transaction::StandaloneContext::create(collection.vocbase(), origin);
#endif

    SingleCollectionTransaction trx(std::move(ctx), collection,
                                    AccessMode::Type::EXCLUSIVE);
    Result res = co_await trx.beginAsync();
    if (res.ok()) {
      co_return doUpdate();
    }
    co_return res;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to rename collections in _graphs as well
////////////////////////////////////////////////////////////////////////////////

static ErrorCode RenameGraphCollections(TRI_vocbase_t& vocbase,
                                        std::string const& oldName,
                                        std::string const& newName) {
  ExecContextSuperuserScope exscope;

  graph::GraphManager gmngr{vocbase, transaction::OperationOriginInternal{
                                         "renaming graph collections"}};
  bool r = gmngr.renameGraphCollection(oldName, newName);
  if (!r) {
    return TRI_ERROR_FAILED;
  }
  return TRI_ERROR_NO_ERROR;
}

Result Collections::rename(LogicalCollection& collection,
                           std::string const& newName, bool doOverride) {
  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    return {TRI_ERROR_CLUSTER_UNSUPPORTED};
  }

  if (newName.empty()) {
    return {TRI_ERROR_BAD_PARAMETER, "<name> must be non-empty"};
  }

  ExecContext const& exec = ExecContext::current();
  if (!exec.canUseDatabase(auth::Level::RW) ||
      !exec.canUseCollection(collection.name(), auth::Level::RW)) {
    return {TRI_ERROR_FORBIDDEN};
  }

  std::string const oldName(collection.name());
  if (oldName == newName) {
    // no actual name change
    return {};
  }

  // check required to pass
  // shell-collection-rocksdb-noncluster.js::testSystemSpecial
  if (collection.system()) {
    return {TRI_ERROR_FORBIDDEN};
  }

  if (!doOverride) {
    bool const isSystem = NameValidator::isSystemName(oldName);

    if (isSystem != NameValidator::isSystemName(newName)) {
      // a system collection shall not be renamed to a non-system collection
      // name or vice versa
      return {TRI_ERROR_ARANGO_ILLEGAL_NAME,
              "a system collection shall not be renamed to a "
              "non-system collection name or vice versa"};
    }

    bool extendedNames = collection.vocbase().extendedNames();
    if (auto res = CollectionNameValidator::validateName(
            isSystem, extendedNames, newName);
        res.fail()) {
      return res;
    }
  }

  auto res = collection.vocbase().renameCollection(collection.id(), newName);

  if (!res.ok()) {
    return res;
  }

  // rename collection inside _graphs as well
  return RenameGraphCollections(collection.vocbase(), oldName, newName);
}

#ifndef USE_ENTERPRISE

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection, case of a coordinator in a cluster
////////////////////////////////////////////////////////////////////////////////

static Result DropVocbaseColCoordinator(LogicalCollection* collection,
                                        bool allowDropSystem) {
  if (collection->system() && !allowDropSystem) {
    return TRI_ERROR_FORBIDDEN;
  }

  auto& databaseName = collection->vocbase().name();
  auto cid = std::to_string(collection->id().id());
  ClusterInfo& ci =
      collection->vocbase().server().getFeature<ClusterFeature>().clusterInfo();
  auto res = ci.dropCollectionCoordinator(databaseName, cid, 300.0);

  if (res.ok()) {
    collection->setDeleted();
  }

  return res;
}
#endif

/*static*/ Result Collections::drop(  // drop collection
    LogicalCollection& coll,          // collection to drop
    CollectionDropOptions options) {
  ExecContext const& exec = ExecContext::current();
  if (!exec.canUseDatabase(coll.vocbase().name(),
                           auth::Level::RW) ||  // vocbase modifiable
      !exec.canUseCollection(coll.name(),
                             auth::Level::RW)) {  // collection modifiable
    events::DropCollection(coll.vocbase().name(), coll.name(),
                           TRI_ERROR_FORBIDDEN);
    return arangodb::Result(  // result
        TRI_ERROR_FORBIDDEN,  // code
        absl::StrCat("Insufficient rights to drop collection ",
                     coll.name())  // message
    );
  }

  auto const& dbname = coll.vocbase().name();
  std::string const collName = coll.name();
  Result res;

  if (!options.allowDropGraphCollection &&
      ServerState::instance()->isSingleServerOrCoordinator()) {
    graph::GraphManager gm(coll.vocbase(),
                           transaction::OperationOriginREST{
                               "checking graph membership of collection"});
    res = gm.applyOnAllGraphs([&collName](std::unique_ptr<graph::Graph> graph)
                                  -> Result {
      TRI_ASSERT(graph != nullptr);
      if (graph->hasOrphanCollection(collName)) {
        return {TRI_ERROR_GRAPH_MUST_NOT_DROP_COLLECTION,
                absl::StrCat(
                    TRI_errno_string(TRI_ERROR_GRAPH_MUST_NOT_DROP_COLLECTION),
                    ": collection '", collName,
                    "' is an orphan collection inside graph '", graph->name(),
                    "'")};
      }
      if (graph->hasEdgeCollection(collName)) {
        return {
            TRI_ERROR_GRAPH_MUST_NOT_DROP_COLLECTION,
            absl::StrCat(
                TRI_errno_string(TRI_ERROR_GRAPH_MUST_NOT_DROP_COLLECTION),
                ": collection '", collName,
                "' is an edge collection inside graph '", graph->name(), "'")};
      }
      if (graph->hasVertexCollection(collName)) {
        return {
            TRI_ERROR_GRAPH_MUST_NOT_DROP_COLLECTION,
            absl::StrCat(
                TRI_errno_string(TRI_ERROR_GRAPH_MUST_NOT_DROP_COLLECTION),
                ": collection '", collName,
                "' is a vertex collection inside graph '", graph->name(), "'")};
      }
      return {};
    });

    if (res.fail()) {
      events::DropCollection(coll.vocbase().name(), coll.name(),
                             res.errorNumber());
      return res;
    }
  }

// If we are a coordinator in a cluster, we have to behave differently:
#ifdef USE_ENTERPRISE
  res = DropColEnterprise(&coll, options.allowDropSystem);
#else
  if (ServerState::instance()->isCoordinator()) {
    res = DropVocbaseColCoordinator(&coll, options.allowDropSystem);
  } else {
    res = coll.vocbase().dropCollection(coll.id(), options.allowDropSystem);
  }
#endif

  LOG_TOPIC_IF("1bf4d", WARN, Logger::ENGINES,
               res.fail() && res.isNot(TRI_ERROR_FORBIDDEN) &&
                   res.isNot(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) &&
                   res.isNot(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND))
      << "error while dropping collection: '" << collName << "' error: '"
      << res.errorMessage() << "'";

  if (ADB_LIKELY(!options.keepUserRights)) {
    auth::UserManager* um = AuthenticationFeature::instance()->userManager();

    if (res.ok() && um != nullptr) {
      res = um->enumerateUsers(
          [&](auth::User& entry) -> bool {
            return entry.removeCollection(dbname, collName);
          },
          /*retryOnConflict*/ true);
    }
  }
  events::DropCollection(coll.vocbase().name(), coll.name(), res.errorNumber());

  return res;
}

futures::Future<Result> Collections::warmup(TRI_vocbase_t& vocbase,
                                            LogicalCollection const& coll) {
  ExecContext const& exec = ExecContext::current();
  if (!exec.canUseCollection(coll.name(), auth::Level::RO)) {
    return futures::makeFuture(Result(TRI_ERROR_FORBIDDEN));
  }

  if (ServerState::instance()->isCoordinator()) {
    auto cid = std::to_string(coll.id().id());
    auto& feature = vocbase.server().getFeature<ClusterFeature>();
    OperationOptions options(exec);
    return warmupOnCoordinator(feature, vocbase.name(), cid, options);
  }

  StorageEngine& engine = vocbase.engine();

  auto idxs = coll.getPhysical()->getReadyIndexes();
  for (auto const& idx : idxs) {
    if (idx->canWarmup()) {
      TRI_IF_FAILURE("warmup::executeDirectly") {
        // when this failure point is set, execute the warmup directly
        idx->warmup();
        continue;
      }
      engine.scheduleFullIndexRefill(vocbase.name(), coll.name(), idx->id());
    }
  }
  return futures::makeFuture(Result());
}

futures::Future<OperationResult> Collections::revisionId(
    Context& ctxt, OperationOptions const& options) {
  if (ServerState::instance()->isCoordinator()) {
    auto& databaseName = ctxt.coll()->vocbase().name();
    auto cid = std::to_string(ctxt.coll()->id().id());
    auto& feature =
        ctxt.coll()->vocbase().server().getFeature<ClusterFeature>();
    co_return co_await revisionOnCoordinator(feature, databaseName, cid,
                                             options);
  }

  RevisionId rid =
      ctxt.coll()->revision(co_await ctxt.trx(AccessMode::Type::READ, true));

  VPackBuilder builder;
  builder.add(VPackValue(rid.toString()));

  co_return OperationResult(Result(), builder.steal(), options);
}

futures::Future<Result> Collections::checksum(LogicalCollection& collection,
                                              bool withRevisions, bool withData,
                                              uint64_t& checksum,
                                              RevisionId& revId) {
  if (ServerState::instance()->isCoordinator()) {
    auto cid = std::to_string(collection.id().id());
    auto& feature = collection.vocbase().server().getFeature<ClusterFeature>();
    OperationOptions options(ExecContext::current());
    auto res = checksumOnCoordinator(feature, collection.vocbase().name(), cid,
                                     options, withRevisions, withData)
                   .get();
    if (res.ok()) {
      revId = RevisionId::fromSlice(res.slice().get("revision"));
      checksum = res.slice().get("checksum").getUInt();
    }
    co_return res.result;
  }

  ResourceMonitor monitor(GlobalResourceMonitor::instance());

  auto origin = transaction::OperationOriginREST{"checksumming collection"};
#ifdef USE_V8
  auto ctx = transaction::V8Context::createWhenRequired(collection.vocbase(),
                                                        origin, true);
#else
  auto ctx =
      transaction::StandaloneContext::create(collection.vocbase(), origin);
#endif
  SingleCollectionTransaction trx(std::move(ctx), collection,
                                  AccessMode::Type::READ);
  Result res = co_await trx.beginAsync();

  if (res.fail()) {
    co_return res;
  }

  revId = collection.revision(&trx);
  checksum = 0;

  // We directly read the entire cursor. so batchsize == limit
  auto iterator =
      trx.indexScan(monitor, collection.name(),
                    transaction::Methods::CursorType::ALL, ReadOwnWrites::no);

  iterator->allDocuments([&](LocalDocumentId, aql::DocumentData&&,
                             VPackSlice slice) {
    uint64_t localHash =
        transaction::helpers::extractKeyFromDocument(slice).hashString();

    if (withRevisions) {
      localHash +=
          transaction::helpers::extractRevSliceFromDocument(slice).hash();
    }

    if (withData) {
      // with data
      uint64_t const n = slice.length() ^ 0xf00ba44ba5;
      uint64_t seed = fasthash64_uint64(n, 0xdeadf054);

      for (auto it : VPackObjectIterator(slice, false)) {
        // loop over all attributes, but exclude _rev, _id and _key
        // _id is different for each collection anyway, _rev is covered by
        // withRevisions, and _key was already handled before
        VPackValueLength keyLength;
        char const* key = it.key.getString(keyLength);
        if (keyLength >= 3 && key[0] == '_' &&
            ((keyLength == 3 && memcmp(key, "_id", 3) == 0) ||
             (keyLength == 4 &&
              (memcmp(key, "_key", 4) == 0 || memcmp(key, "_rev", 4) == 0)))) {
          // exclude attribute
          continue;
        }

        localHash ^= it.key.hash(seed) ^ 0xba5befd00d;
        localHash += it.value.normalizedHash(seed) ^ 0xd4129f526421;
      }
    }

    checksum ^= localHash;
    return true;
  });

  co_return trx.finish(res);
}

/// @brief the list of collection attributes that are allowed by user-input
/// this is to avoid retyping the same list twice
#define COMMON_ALLOWED_COLLECTION_INPUT_ATTRIBUTES                             \
  StaticStrings::DataSourceSystem, StaticStrings::DataSourceId,                \
      StaticStrings::KeyOptions, StaticStrings::WaitForSyncString,             \
      StaticStrings::CacheEnabled, StaticStrings::ShardKeys,                   \
      StaticStrings::NumberOfShards, StaticStrings::DistributeShardsLike,      \
      "avoidServers", StaticStrings::IsSmart, StaticStrings::ShardingStrategy, \
      StaticStrings::GraphSmartGraphAttribute, StaticStrings::Schema,          \
      StaticStrings::SmartJoinAttribute, StaticStrings::ReplicationFactor,     \
      StaticStrings::MinReplicationFactor, /* deprecated */                    \
      StaticStrings::WriteConcern, "servers", StaticStrings::ComputedValues

arangodb::velocypack::Builder Collections::filterInput(
    arangodb::velocypack::Slice properties, bool allowDC2DCAttributes) {
#ifdef USE_ENTERPRISE
  if (allowDC2DCAttributes) {
    return velocypack::Collection::keep(
        properties,
        std::unordered_set<std::string>{
            StaticStrings::IsDisjoint, StaticStrings::InternalValidatorTypes,
            COMMON_ALLOWED_COLLECTION_INPUT_ATTRIBUTES});
  }
#endif
  return velocypack::Collection::keep(
      properties, std::unordered_set<std::string>{
                      COMMON_ALLOWED_COLLECTION_INPUT_ATTRIBUTES});
}

#ifndef USE_ENTERPRISE
void Collections::appendSmartEdgeCollections(
    CreateCollectionBody&, std::vector<CreateCollectionBody>&,
    std::function<DataSourceId()> const&) {
  // Nothing to do here.
}
#endif
