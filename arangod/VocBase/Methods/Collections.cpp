////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Collections.h"
#include "Basics/Common.h"

#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/V8Context.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8/JavaScriptSecurityContext.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/CollectionCreationInfo.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::methods;

Collections::Context::Context(TRI_vocbase_t& vocbase, LogicalCollection& coll)
    : _vocbase(vocbase), _coll(coll), _trx(nullptr), _responsibleForTrx(true) {}

Collections::Context::Context(TRI_vocbase_t& vocbase, LogicalCollection& coll,
                              transaction::Methods* trx)
    : _vocbase(vocbase), _coll(coll), _trx(trx), _responsibleForTrx(false) {
  TRI_ASSERT(_trx != nullptr);
}

Collections::Context::~Context() {
  if (_responsibleForTrx && _trx != nullptr) {
    delete _trx;
  }
}

transaction::Methods* Collections::Context::trx(AccessMode::Type const& type, bool embeddable,
                                                bool forceLoadCollection) {
  if (_responsibleForTrx && _trx == nullptr) {
    auto ctx = transaction::V8Context::CreateWhenRequired(_vocbase, embeddable);
    auto trx = std::make_unique<SingleCollectionTransaction>(ctx, _coll, type);
    if (!trx) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_OUT_OF_MEMORY,
                                     "Cannot create Transaction");
    }

    if (!forceLoadCollection) {
      // we actually need this hint here, so that the collection is not
      // loaded if it has status unloaded.
      trx->addHint(transaction::Hints::Hint::NO_USAGE_LOCK);
    }

    Result res = trx->begin();

    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
    _trx = trx.release();
  }
  // ADD asserts for running state and locking issues!
  return _trx;
}

TRI_vocbase_t& Collections::Context::vocbase() const { return _vocbase; }

LogicalCollection* Collections::Context::coll() const { return &_coll; }

void Collections::enumerate(TRI_vocbase_t* vocbase,
                            std::function<void(std::shared_ptr<LogicalCollection> const&)> const& func) {
  if (ServerState::instance()->isCoordinator()) {
    std::vector<std::shared_ptr<LogicalCollection>> colls =
        ClusterInfo::instance()->getCollections(vocbase->name());

    for (std::shared_ptr<LogicalCollection> const& c : colls) {
      if (!c->deleted()) {
        func(c);
      }
    }
  } else {
    for (auto& c : vocbase->collections(false)) {
      if (!c->deleted()) {
        func(c);
      }
    }
  }
}

/*static*/ arangodb::Result methods::Collections::lookup(  // find collection
    TRI_vocbase_t const& vocbase,                          // vocbase to search
    std::string const& name,                               // collection name
    FuncCallback func  // invoke on found collection
) {
  if (name.empty()) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  ExecContext const* exec = ExecContext::CURRENT;

  if (ServerState::instance()->isCoordinator()) {
    try {
      auto ci = ClusterInfo::instance();

      if (!ci) {
        return arangodb::Result(  // result
            TRI_ERROR_INTERNAL,   // code
            "failure to find 'ClusterInfo' instance while searching for "
            "collection"  // message
        );
      }

      auto coll = ci->getCollectionNT(vocbase.name(), name);

      if (coll) {
        // check authentication after ensuring the collection exists
        if (exec != nullptr &&
            !exec->canUseCollection(vocbase.name(), coll->name(), auth::Level::RO)) {
          return Result(TRI_ERROR_FORBIDDEN,
                        "No access to collection '" + name + "'");
        }

        func(coll);

        return Result();
      } else {
        return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      }
    } catch (basics::Exception const& ex) {
      return Result(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      return Result(TRI_ERROR_INTERNAL);
    }

    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  auto coll = vocbase.lookupCollection(name);

  if (coll != nullptr) {
    // check authentication after ensuring the collection exists
    if (exec != nullptr &&
        !exec->canUseCollection(vocbase.name(), coll->name(), auth::Level::RO)) {
      return Result(TRI_ERROR_FORBIDDEN,
                    "No access to collection '" + name + "'");
    }
    try {
      func(coll);
    } catch (basics::Exception const& ex) {
      return Result(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      return Result(TRI_ERROR_INTERNAL);
    }

    return Result();
  }

  return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

/*static*/ arangodb::Result Collections::create(    // create collection
    TRI_vocbase_t& vocbase,                         // collection vocbase
    std::string const& name,                        // collection name
    TRI_col_type_e collectionType,                  // collection type
    arangodb::velocypack::Slice const& properties,  // collection properties
    bool createWaitsForSyncReplication,             // replication wait flag
    bool enforceReplicationFactor,                  // replication factor flag
    FuncCallback func  // invoke on collection creation
) {
  if (name.empty()) {
    events::CreateCollection(vocbase.name(), name, TRI_ERROR_ARANGO_ILLEGAL_NAME);
    return TRI_ERROR_ARANGO_ILLEGAL_NAME;
  } else if (collectionType != TRI_col_type_e::TRI_COL_TYPE_DOCUMENT &&
             collectionType != TRI_col_type_e::TRI_COL_TYPE_EDGE) {
    events::CreateCollection(vocbase.name(), name, TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    return TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
  }
  std::vector<CollectionCreationInfo> infos{{name, collectionType, properties}};
  return create(vocbase, infos, createWaitsForSyncReplication, enforceReplicationFactor,
                [&func](std::vector<std::shared_ptr<LogicalCollection>> const& cols) {
                  TRI_ASSERT(cols.size() == 1);
                  func(cols[0]);
                });
}

Result Collections::create(TRI_vocbase_t& vocbase,
                           std::vector<CollectionCreationInfo> const& infos,
                           bool createWaitsForSyncReplication, bool enforceReplicationFactor,
                           MultiFuncCallback const& func) {
  ExecContext const* exec = ExecContext::CURRENT;
  if (exec && !exec->canUseDatabase(vocbase.name(), auth::Level::RW)) {
    for (auto const& info : infos) {
      events::CreateCollection(vocbase.name(), info.name, TRI_ERROR_FORBIDDEN);
    }
    return arangodb::Result(                             // result
        TRI_ERROR_FORBIDDEN,                             // code
        "cannot create collection in " + vocbase.name()  // message
    );
  }

  TRI_ASSERT(!vocbase.isDangling());
  ShardingFeature* feature = nullptr;
  if (ServerState::instance()->isCoordinator()) {
    feature = application_features::ApplicationServer::getFeature<ShardingFeature>(
        "Sharding");
  }
  VPackBuilder builder;
  VPackBuilder helper;
  builder.openArray();

  for (auto const& info : infos) {
    TRI_ASSERT(builder.isOpenArray());

    if (info.name.empty()) {
      return TRI_ERROR_ARANGO_ILLEGAL_NAME;
    } else if (info.collectionType != TRI_col_type_e::TRI_COL_TYPE_DOCUMENT &&
               info.collectionType != TRI_col_type_e::TRI_COL_TYPE_EDGE) {
      return TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
    }

    TRI_ASSERT(info.properties.isObject());
    helper.clear();
    helper.openObject();
    helper.add(arangodb::StaticStrings::DataSourceType,
               arangodb::velocypack::Value(static_cast<int>(info.collectionType)));
    helper.add(arangodb::StaticStrings::DataSourceName,
               arangodb::velocypack::Value(info.name));

    //auto shardingSlice = info.properties.get(StaticStrings::Sharding);
    auto replicationFactorSlice = info.properties.get(StaticStrings::ReplicationFactor);
    if(replicationFactorSlice.isNone()) {
      auto factor = vocbase.replicationFactor();
      if(factor > 0 && vocbase.IsSystemName(info.name)) {
        // system collections should at least be replicated twice
        factor = std::max(vocbase.replicationFactor(), 2u);
      }
      helper.add(StaticStrings::ReplicationFactor, VPackValue(vocbase.replicationFactor()));
    }

    // system collections will be shareded normally - we avoid a self reference when creating _graphs
    if(vocbase.sharding() == "single" && !vocbase.IsSystemName(info.name)) {
      auto distribute = info.properties.get(StaticStrings::DistributeShardsLike);
      if(distribute.isNone()) {
        helper.add(StaticStrings::DistributeShardsLike, VPackValue(StaticStrings::GraphCollection));
      }
    }

    helper.close();

    VPackBuilder merged =
        VPackCollection::merge(info.properties, helper.slice(), false, true);

    if (feature != nullptr && !info.properties.get("shardingStrategy").isString()) {
      // NOTE: We need to do this in a second merge as the geature call requires to have the
      // DataSourceType set in the JSON, which has just been done by the call above.
      helper.clear();
      helper.openObject();
      TRI_ASSERT(ServerState::instance()->isCoordinator());
      helper.add("shardingStrategy", VPackValue(feature->getDefaultShardingStrategyForNewCollection(
                                         merged.slice())));
      helper.close();
      merged = VPackCollection::merge(merged.slice(), helper.slice(), false, true);
    }

    builder.add(merged.slice());
  }

  TRI_ASSERT(builder.isOpenArray());
  builder.close();

  VPackSlice const infoSlice = builder.slice();
  std::vector<std::shared_ptr<LogicalCollection>> collections;
  TRI_ASSERT(infoSlice.isArray());
  TRI_ASSERT(infoSlice.length() >= 1);
  TRI_ASSERT(infoSlice.length() == infos.size());
  collections.reserve(infoSlice.length());
  try {
    if (ServerState::instance()->isCoordinator()) {
      collections =
          ClusterMethods::createCollectionOnCoordinator(vocbase, infoSlice, false,
                                                        createWaitsForSyncReplication,
                                                        enforceReplicationFactor);

      if (collections.empty()) {
        for (auto const& info : infos) {
          events::CreateCollection(vocbase.name(), info.name, TRI_ERROR_INTERNAL);
        }
        return Result(TRI_ERROR_INTERNAL, "createCollectionsOnCoordinator");
      }
    } else {
      for (auto slice : VPackArrayIterator(infoSlice)) {
        // Single server does not yet have a multi collection implementation
        auto col = vocbase.createCollection(slice);
        TRI_ASSERT(col != nullptr);
        collections.emplace_back(col);
      }
    }
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "cannot create collection");
  }

  // Grant access to the collections.
  // This is identical on cluster and SingleServer
  try {
    // in case of success we grant the creating user RW access
    auth::UserManager* um = AuthenticationFeature::instance()->userManager();
    if (um != nullptr && exec != nullptr && !exec->isSuperuser()) {
      // this should not fail, we can not get here without database RW access
      // however, there may be races for updating the users account, so we try
      // a few times in case of a conflict
      int tries = 0;
      while (true) {
        Result r = um->updateUser(exec->user(), [&](auth::User& entry) {
          for (auto const& col : collections) {
            // do not grant rights on system collections
            if (!col->system()) {
              entry.grantCollection(vocbase.name(), col->name(), auth::Level::RW);
              events::CreateCollection(vocbase.name(), col->name(), TRI_ERROR_NO_ERROR);
            }
          }
          return TRI_ERROR_NO_ERROR;
        });
        if (r.ok() || r.is(TRI_ERROR_USER_NOT_FOUND) || r.is(TRI_ERROR_USER_EXTERNAL)) {
          // it seems to be allowed to created collections with an unknown user
          break;
        }
        if (!r.is(TRI_ERROR_ARANGO_CONFLICT) || ++tries == 10) {
          LOG_TOPIC("116bb", WARN, Logger::FIXME)
              << "Updating user failed with error: " << r.errorMessage() << ". giving up!";
          for (auto const& col : collections) {
            events::CreateCollection(vocbase.name(), col->name(), r.errorNumber());
          }
          return r;
        }
        // try again in case of conflict
        LOG_TOPIC("ff123", TRACE, Logger::FIXME)
            << "Updating user failed with error: " << r.errorMessage() << ". trying again";
      }
    }
    func(collections);
  } catch (basics::Exception const& ex) {
    for (auto const& info : infos) {
      events::CreateCollection(vocbase.name(), info.name, ex.code());
    }
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    for (auto const& info : infos) {
      events::CreateCollection(vocbase.name(), info.name, TRI_ERROR_INTERNAL);
    }
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    for (auto const& info : infos) {
      events::CreateCollection(vocbase.name(), info.name, TRI_ERROR_INTERNAL);
    }
    return Result(TRI_ERROR_INTERNAL, "cannot create collection");
  }
  for (auto const& info : infos) {
    events::CreateCollection(vocbase.name(), info.name, TRI_ERROR_NO_ERROR);
  }

  return TRI_ERROR_NO_ERROR;
}

/*static*/ Result Collections::createSystem(
    TRI_vocbase_t& vocbase,
    std::string const& name) {
  FuncCallback const noop = [](std::shared_ptr<LogicalCollection> const&)->void{};

  auto res = methods::Collections::lookup(vocbase, name, noop);

  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    uint32_t defaultReplFactor = 1;

    auto* cl = application_features::ApplicationServer::lookupFeature<ClusterFeature>("Cluster");

    if (cl != nullptr) {
      defaultReplFactor = cl->systemReplicationFactor();
    }

    VPackBuilder bb;

    {
      VPackObjectBuilder scope(&bb);
      bb.add("isSystem", VPackSlice::trueSlice());
      bb.add("waitForSync", VPackSlice::falseSlice());
      bb.add("journalSize", VPackValue(1024 * 1024));
      bb.add("replicationFactor", VPackValue(defaultReplFactor));
      if (name != "_graphs") {
        // that forces all collections to be on the same physical DBserver
        bb.add("distributeShardsLike", VPackValue("_graphs"));
      }
    }

    res = Collections::create(
      vocbase, // vocbase to create in
      name, // collection name top create
      TRI_COL_TYPE_DOCUMENT, // collection type to create
      bb.slice(), // collection definition to create
      true,  // waitsForSyncReplication
      true,  // enforceReplicationFactor
      noop); // callback
  }

  return res;
}

Result Collections::load(TRI_vocbase_t& vocbase, LogicalCollection* coll) {
  TRI_ASSERT(coll != nullptr);

  if (ServerState::instance()->isCoordinator()) {
#ifdef USE_ENTERPRISE
    return ULColCoordinatorEnterprise(coll->vocbase().name(),
                                      std::to_string(coll->id()), TRI_VOC_COL_STATUS_LOADED);
#else
    auto ci = ClusterInfo::instance();

    return ci->setCollectionStatusCoordinator(coll->vocbase().name(),
                                              std::to_string(coll->id()),
                                              TRI_VOC_COL_STATUS_LOADED);
#endif
  }

  auto ctx = transaction::V8Context::CreateWhenRequired(vocbase, true);
  SingleCollectionTransaction trx(ctx, *coll, AccessMode::Type::READ);
  Result res = trx.begin();

  if (res.fail()) {
    return res;
  }

  return trx.finish(res);
}

Result Collections::unload(TRI_vocbase_t* vocbase, LogicalCollection* coll) {
  if (ServerState::instance()->isCoordinator()) {
#ifdef USE_ENTERPRISE
    return ULColCoordinatorEnterprise(vocbase->name(), std::to_string(coll->id()),
                                      TRI_VOC_COL_STATUS_UNLOADED);
#else
    auto ci = ClusterInfo::instance();

    return ci->setCollectionStatusCoordinator(vocbase->name(),
                                              std::to_string(coll->id()),
                                              TRI_VOC_COL_STATUS_UNLOADED);
#endif
  }

  return vocbase->unloadCollection(coll, false);
}

Result Collections::properties(Context& ctxt, VPackBuilder& builder) {
  LogicalCollection* coll = ctxt.coll();
  TRI_ASSERT(coll != nullptr);
  ExecContext const* exec = ExecContext::CURRENT;
  if (exec != nullptr) {
    bool canRead = exec->canUseCollection(coll->name(), auth::Level::RO);
    if (exec->databaseAuthLevel() == auth::Level::NONE || !canRead) {
      return Result(TRI_ERROR_FORBIDDEN, "cannot access " + coll->name());
    }
  }

  std::unordered_set<std::string> ignoreKeys{
      "allowUserKeys", "cid",    "count",  "deleted", "id",   "indexes", "name",
      "path",          "planId", "shards", "status",  "type", "version"};

  if (!ServerState::instance()->isCoordinator()) {
    // These are only relevant for cluster
    ignoreKeys.insert({"distributeShardsLike", "isSmart", "numberOfShards",
                       "replicationFactor", "shardKeys", "shardingStrategy"});

    // this transaction is held longer than the following if...
    auto trx = ctxt.trx(AccessMode::Type::READ, true, false);
    TRI_ASSERT(trx != nullptr);
  }

  // note that we have an ongoing transaction here if we are in single-server
  // case
  VPackBuilder props =
      coll->toVelocyPackIgnore(ignoreKeys, LogicalDataSource::makeFlags(
                                               LogicalDataSource::Serialize::Detailed));
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackObjectIterator(props.slice()));

  return TRI_ERROR_NO_ERROR;
}

Result Collections::updateProperties(LogicalCollection& collection,
                                     velocypack::Slice const& props, bool partialUpdate) {
  ExecContext const* exec = ExecContext::CURRENT;

  if (exec != nullptr) {
    bool canModify = exec->canUseCollection(collection.name(), auth::Level::RW);

    if ((exec->databaseAuthLevel() != auth::Level::RW || !canModify)) {
      return TRI_ERROR_FORBIDDEN;
    }
  }

  if (ServerState::instance()->isCoordinator()) {
    ClusterInfo* ci = ClusterInfo::instance();
    auto info = ci->getCollection(collection.vocbase().name(),
                                  std::to_string(collection.id()));

    return info->properties(props, partialUpdate);
  } else {
    auto ctx = transaction::V8Context::CreateWhenRequired(collection.vocbase(), false);
    SingleCollectionTransaction trx(ctx, collection, AccessMode::Type::EXCLUSIVE);
    Result res = trx.begin();

    if (!res.ok()) {
      return res;
    }

    // try to write new parameter to file
    auto updateRes = collection.properties(props, partialUpdate);

    if (!updateRes.ok()) {
      return updateRes;
    }

    auto physical = collection.getPhysical();
    TRI_ASSERT(physical != nullptr);

    return physical->persistProperties();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to rename collections in _graphs as well
////////////////////////////////////////////////////////////////////////////////

static int RenameGraphCollections(TRI_vocbase_t* vocbase, std::string const& oldName,
                                  std::string const& newName) {
  V8DealerFeature* dealer = V8DealerFeature::DEALER;
  if (dealer == nullptr || !dealer->isEnabled()) {
    return TRI_ERROR_NO_ERROR;  // V8 might is disabled
  }

  StringBuffer buffer(true);
  buffer.appendText(
      "require('@arangodb/general-graph-common')._renameCollection(");
  buffer.appendJsonEncoded(oldName.c_str(), oldName.size());
  buffer.appendChar(',');
  buffer.appendJsonEncoded(newName.c_str(), newName.size());
  buffer.appendText(");");

  JavaScriptSecurityContext securityContext =
      JavaScriptSecurityContext::createInternalContext();
  V8ContextGuard guard(vocbase, securityContext);

  auto isolate = guard.isolate();
  v8::HandleScope scope(isolate);
  TRI_ExecuteJavaScriptString(isolate, isolate->GetCurrentContext(),
                              TRI_V8_ASCII_PAIR_STRING(isolate, buffer.c_str(),
                                                       buffer.length()),
                              TRI_V8_ASCII_STRING(isolate, "collection rename"), false);

  return TRI_ERROR_NO_ERROR;
}

Result Collections::rename(LogicalCollection& collection,
                           std::string const& newName, bool doOverride) {
  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    return TRI_ERROR_CLUSTER_UNSUPPORTED;
  }

  if (newName.empty()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "<name> must be non-empty");
  }

  ExecContext const* exec = ExecContext::CURRENT;
  if (exec != nullptr) {
    if (!exec->canUseDatabase(auth::Level::RW) ||
        !exec->canUseCollection(collection.name(), auth::Level::RW)) {
      return TRI_ERROR_FORBIDDEN;
    }
  }

  // check required to pass
  // shell-collection-rocksdb-noncluster.js::testSystemSpecial
  if (collection.system()) {
    return TRI_set_errno(TRI_ERROR_FORBIDDEN);
  }

  if (!doOverride) {
    auto isSystem = TRI_vocbase_t::IsSystemName(collection.name());

    if (isSystem && !TRI_vocbase_t::IsSystemName(newName)) {
      // a system collection shall not be renamed to a non-system collection
      // name
      return arangodb::Result(TRI_ERROR_ARANGO_ILLEGAL_NAME,
                              "a system collection shall not be renamed to a "
                              "non-system collection name");
    } else if (!isSystem && TRI_vocbase_t::IsSystemName(newName)) {
      return arangodb::Result(TRI_ERROR_ARANGO_ILLEGAL_NAME,
                              "a non-system collection shall not be renamed to "
                              "a system collection name");
    }

    if (!TRI_vocbase_t::IsAllowedName(isSystem, arangodb::velocypack::StringRef(newName))) {
      return TRI_ERROR_ARANGO_ILLEGAL_NAME;
    }
  }

  std::string const oldName(collection.name());
  auto res = collection.vocbase().renameCollection(collection.id(), newName);

  if (!res.ok()) {
    return res;
  }

  // rename collection inside _graphs as well
  return RenameGraphCollections(&(collection.vocbase()), oldName, newName);
}

#ifndef USE_ENTERPRISE

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection, case of a coordinator in a cluster
////////////////////////////////////////////////////////////////////////////////

static Result DropVocbaseColCoordinator(arangodb::LogicalCollection* collection,
                                        bool allowDropSystem) {
  if (collection->system() && !allowDropSystem) {
    return TRI_ERROR_FORBIDDEN;
  }

  auto& databaseName = collection->vocbase().name();
  auto cid = std::to_string(collection->id());
  ClusterInfo* ci = ClusterInfo::instance();
  auto res = ci->dropCollectionCoordinator(databaseName, cid, 300.0);

  if (!res.ok()) {
    return res;
  }

  collection->setStatus(TRI_VOC_COL_STATUS_DELETED);

  return TRI_ERROR_NO_ERROR;
}
#endif

/*static*/ arangodb::Result Collections::drop(  // drop collection
    arangodb::LogicalCollection& coll,          // collection to drop
    bool allowDropSystem,  // allow dropping system collection
    double timeout         // single-server drop timeout
) {
  ExecContext const* exec = ExecContext::CURRENT;

  if (exec  // have exec context
      && !(exec->canUseDatabase(coll.vocbase().name(), auth::Level::RW)  // vocbase modifiable
           && exec->canUseCollection(coll.name(), auth::Level::RW)  // collection modifiable
           )) {
    events::DropCollection(coll.vocbase().name(), coll.name(), TRI_ERROR_FORBIDDEN);
    return arangodb::Result(                                     // result
        TRI_ERROR_FORBIDDEN,                                     // code
        "Insufficient rights to drop collection " + coll.name()  // message
    );
  }

  auto const& dbname = coll.vocbase().name();
  std::string const collName = coll.name();
  Result res;

  // If we are a coordinator in a cluster, we have to behave differently:
  if (ServerState::instance()->isCoordinator()) {
#ifdef USE_ENTERPRISE
    res = DropColCoordinatorEnterprise(&coll, allowDropSystem);
#else
    res = DropVocbaseColCoordinator(&coll, allowDropSystem);
#endif
  } else {
    res = coll.vocbase().dropCollection(coll.id(), allowDropSystem, timeout);
  }

  auth::UserManager* um = AuthenticationFeature::instance()->userManager();

  if (res.ok() && um != nullptr) {
    res = um->enumerateUsers([&](auth::User& entry) -> bool {
      return entry.removeCollection(dbname, collName);
    }, /*retryOnConflict*/true);
  }

  events::DropCollection(coll.vocbase().name(), coll.name(), res.errorNumber());

  return res;
}

Result Collections::warmup(TRI_vocbase_t& vocbase, LogicalCollection const& coll) {
  ExecContext const* exec = ExecContext::CURRENT;  // disallow expensive ops
  if (exec != nullptr && !exec->canUseCollection(coll.name(), auth::Level::RO)) {
    return Result(TRI_ERROR_FORBIDDEN);
  }

  if (ServerState::instance()->isCoordinator()) {
    auto cid = std::to_string(coll.id());
    return warmupOnCoordinator(vocbase.name(), cid);
  }

  auto ctx = transaction::V8Context::CreateWhenRequired(vocbase, false);
  SingleCollectionTransaction trx(ctx, coll, AccessMode::Type::READ);
  Result res = trx.begin();

  if (res.fail()) {
    return res;
  }

  auto idxs = coll.getIndexes();
  auto poster = [](std::function<void()> fn) -> void {
    SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW, fn);
  };
  auto queue = std::make_shared<basics::LocalTaskQueue>(poster);

  for (auto& idx : idxs) {
    idx->warmup(&trx, queue);
  }

  queue->dispatchAndWait();

  if (queue->status() == TRI_ERROR_NO_ERROR) {
    res = trx.commit();
  } else {
    return queue->status();
  }

  return res;
}

Result Collections::revisionId(Context& ctxt, TRI_voc_rid_t& rid) {
  if (ServerState::instance()->isCoordinator()) {
    auto& databaseName = ctxt.coll()->vocbase().name();
    auto cid = std::to_string(ctxt.coll()->id());
    return revisionOnCoordinator(databaseName, cid, rid);
  }

  rid = ctxt.coll()->revision(ctxt.trx(AccessMode::Type::READ, true, true));

  return TRI_ERROR_NO_ERROR;
}

/// @brief Helper implementation similar to ArangoCollection.all() in v8
/*static*/ arangodb::Result Collections::all(TRI_vocbase_t& vocbase, std::string const& cname,
                                             DocCallback const& cb) {
  // Implement it like this to stay close to the original
  if (ServerState::instance()->isCoordinator()) {
    auto empty = std::make_shared<VPackBuilder>();
    std::string q = "FOR r IN @@coll RETURN r";
    auto binds = std::make_shared<VPackBuilder>();
    binds->openObject();
    binds->add("@coll", VPackValue(cname));
    binds->close();
    arangodb::aql::Query query(false, vocbase, aql::QueryString(q), binds,
                               std::make_shared<VPackBuilder>(), arangodb::aql::PART_MAIN);
    auto queryRegistry = QueryRegistryFeature::registry();
    TRI_ASSERT(queryRegistry != nullptr);
    aql::QueryResult queryResult = query.executeSync(queryRegistry);

    Result res = queryResult.result;
    if (queryResult.result.ok()) {
      VPackSlice array = queryResult.data->slice();
      for (VPackSlice doc : VPackArrayIterator(array)) {
        cb(doc.resolveExternal());
      }
    }
    return res;
  } else {
    auto ctx = transaction::V8Context::CreateWhenRequired(vocbase, true);
    SingleCollectionTransaction trx(ctx, cname, AccessMode::Type::READ);
    Result res = trx.begin();

    if (res.fail()) {
      return res;
    }

    // We directly read the entire cursor. so batchsize == limit
    OperationCursor opCursor(trx.indexScan(cname, transaction::Methods::CursorType::ALL));

    opCursor.allDocuments([&](LocalDocumentId const& token,
                              VPackSlice doc) { cb(doc.resolveExternal()); },
                          1000);

    return trx.finish(res);
  }
}
