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
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/V8Context.h"
#include "Utils/OperationCursor.h"
#include "Utils/ExecContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::methods;

void Collections::enumerate(
    TRI_vocbase_t* vocbase,
    std::function<void(LogicalCollection*)> const& func) {
  if (ServerState::instance()->isCoordinator()) {
    std::vector<std::shared_ptr<LogicalCollection>> colls =
        ClusterInfo::instance()->getCollections(vocbase->name());
    for (std::shared_ptr<LogicalCollection> const& c : colls) {
      if (!c->deleted()) {
        func(c.get());
      }
    }

  } else {
    std::vector<arangodb::LogicalCollection*> colls =
        vocbase->collections(false);
    for (LogicalCollection* c : colls) {
      if (!c->deleted()) {
        func(c);
      }
    }
  }
}

Result methods::Collections::lookup(TRI_vocbase_t* vocbase,
                                    std::string const& name,
                                    FuncCallback func) {
  if (name.empty()) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  ExecContext const* exec = ExecContext::CURRENT;
  if (ServerState::instance()->isCoordinator()) {
    try {
      auto coll = ClusterInfo::instance()->getCollection(vocbase->name(), name);
      // check authentication after ensuring the collection exists
      if (exec != nullptr &&
          !exec->canUseCollection(vocbase->name(), coll->name(), auth::Level::RO)) {
        return Result(TRI_ERROR_FORBIDDEN, "No access to collection '" + name + "'");
      }
      func(coll.get());
      return Result();
    } catch (basics::Exception const& ex) {
      return Result(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      return Result(TRI_ERROR_INTERNAL);
    }
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  auto coll = vocbase->lookupCollection(name);

  if (coll != nullptr) {
    // check authentication after ensuring the collection exists
    if (exec != nullptr &&
        !exec->canUseCollection(vocbase->name(), coll->name(), auth::Level::RO)) {
      return Result(TRI_ERROR_FORBIDDEN, "No access to collection '" + name + "'");
    }
    try {
      func(coll.get());
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

Result Collections::create(TRI_vocbase_t* vocbase, std::string const& name,
                           TRI_col_type_e collectionType,
                           velocypack::Slice const& properties,
                           bool createWaitsForSyncReplication,
                           bool enforceReplicationFactor,
                           FuncCallback func) {
  if (name.empty()) {
    return TRI_ERROR_ARANGO_ILLEGAL_NAME;
  } else if (collectionType != TRI_col_type_e::TRI_COL_TYPE_DOCUMENT &&
             collectionType != TRI_col_type_e::TRI_COL_TYPE_EDGE) {
    return TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
  }

  ExecContext const* exec = ExecContext::CURRENT;
  if (exec != nullptr) {
    if (!exec->canUseDatabase(vocbase->name(), auth::Level::RW)) {
      return Result(TRI_ERROR_FORBIDDEN,
                    "cannot create collection in " + vocbase->name());
    } else if (!exec->isSuperuser() && !ServerState::writeOpsEnabled()) {
      return Result(TRI_ERROR_ARANGO_READ_ONLY, "server is in read-only mode");
    }
  }

  TRI_ASSERT(vocbase && !vocbase->isDangling());
  TRI_ASSERT(properties.isObject());

  VPackBuilder builder;

  builder.openObject();
  builder.add(
    arangodb::StaticStrings::DataSourceType,
    arangodb::velocypack::Value(static_cast<int>(collectionType))
  );
  builder.add(
    arangodb::StaticStrings::DataSourceName,
    arangodb::velocypack::Value(name)
  );
  builder.close();

  VPackBuilder info =
      VPackCollection::merge(properties, builder.slice(), false, true);
  VPackSlice const infoSlice = info.slice();

  try {
    ExecContext const* exe = ExecContext::CURRENT;
    if (ServerState::instance()->isCoordinator()) {
      auto col = ClusterMethods::createCollectionOnCoordinator(
        collectionType,
        *vocbase,
        infoSlice,
        false,
        createWaitsForSyncReplication,
        enforceReplicationFactor
      );

      if (!col) {
        return Result(TRI_ERROR_INTERNAL, "createCollectionOnCoordinator");
      }

      // do not grant rights on system collections
      // in case of success we grant the creating user RW access
      auth::UserManager* um = AuthenticationFeature::instance()->userManager();
      if (name[0] != '_' && um != nullptr && exe != nullptr && !exe->isSuperuser()) {
        // this should not fail, we can not get here without database RW access
        um->updateUser(
          ExecContext::CURRENT->user(), [&](auth::User& entry) {
            entry.grantCollection(vocbase->name(), name, auth::Level::RW);
            return TRI_ERROR_NO_ERROR;
        });
      }

      // reload otherwise collection might not be in yet
      func(col.get());
    } else {
      arangodb::LogicalCollection* col = vocbase->createCollection(infoSlice);
      TRI_ASSERT(col != nullptr);

      // do not grant rights on system collections
      // in case of success we grant the creating user RW access
      auth::UserManager* um = AuthenticationFeature::instance()->userManager();
      if (name[0] != '_' && um != nullptr && exe != nullptr && !exe->isSuperuser()) {
        // this should not fail, we can not get here without database RW access
        um->updateUser(
          ExecContext::CURRENT->user(), [&](auth::User& u) {
            u.grantCollection(vocbase->name(), name, auth::Level::RW);
            return TRI_ERROR_NO_ERROR;
          });
      }
      func(col);
    }
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "cannot create collection");
  }

  return TRI_ERROR_NO_ERROR;
}

Result Collections::load(TRI_vocbase_t& vocbase, LogicalCollection* coll) {
  TRI_ASSERT(coll != nullptr);

  if (ServerState::instance()->isCoordinator()) {
#ifdef USE_ENTERPRISE
    return ULColCoordinatorEnterprise(
      coll->vocbase().name(),
      std::to_string(coll->id()),
      TRI_VOC_COL_STATUS_LOADED
    );
#else
    auto ci = ClusterInfo::instance();

    return ci->setCollectionStatusCoordinator(
      coll->vocbase().name(),
      std::to_string(coll->id()),
      TRI_VOC_COL_STATUS_LOADED
    );
#endif
  }

  auto ctx = transaction::V8Context::CreateWhenRequired(vocbase, true);
  SingleCollectionTransaction trx(ctx, coll->id(), AccessMode::Type::READ);

  Result res = trx.begin();

  if (res.fail()) {
    return res;
  }

  return trx.finish(res);
}

Result Collections::unload(TRI_vocbase_t* vocbase, LogicalCollection* coll) {
  if (ServerState::instance()->isCoordinator()) {
#ifdef USE_ENTERPRISE
    return ULColCoordinatorEnterprise(
      vocbase->name(), std::to_string(coll->id()), TRI_VOC_COL_STATUS_UNLOADED
    );
#else
    auto ci = ClusterInfo::instance();

    return ci->setCollectionStatusCoordinator(
      vocbase->name(), std::to_string(coll->id()), TRI_VOC_COL_STATUS_UNLOADED
    );
#endif
  }

  return vocbase->unloadCollection(coll, false);
}

Result Collections::properties(LogicalCollection* coll, VPackBuilder& builder) {
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

  // this transaction is held longer than the following if...
  std::unique_ptr<SingleCollectionTransaction> trx;

  if (!ServerState::instance()->isCoordinator()) {
    // These are only relevant for cluster
    ignoreKeys.insert({"distributeShardsLike", "isSmart", "numberOfShards",
                       "replicationFactor", "shardKeys"});

    auto ctx =
      transaction::V8Context::CreateWhenRequired(coll->vocbase(), true);

    // populate the transaction object (which is used outside this if too)
    trx.reset(new SingleCollectionTransaction(
      ctx, coll->id(), AccessMode::Type::READ
    ));

    // we actually need this hint here, so that the collection is not
    // loaded if it has status unloaded.
    trx->addHint(transaction::Hints::Hint::NO_USAGE_LOCK);

    Result res = trx->begin();

    if (res.fail()) {
      return res;
    }
  }

  // note that we have an ongoing transaction here if we are in single-server
  // case
  VPackBuilder props = coll->toVelocyPackIgnore(ignoreKeys, true, false);
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackObjectIterator(props.slice()));

  return TRI_ERROR_NO_ERROR;
}

Result Collections::updateProperties(LogicalCollection* coll,
                                     VPackSlice const& props) {
  ExecContext const* exec = ExecContext::CURRENT;
  if (exec != nullptr) {
    bool canModify = exec->canUseCollection(coll->name(), auth::Level::RW);
    if ((exec->databaseAuthLevel() != auth::Level::RW || !canModify)) {
      return TRI_ERROR_FORBIDDEN;
    } else if (!exec->isSuperuser() && !ServerState::writeOpsEnabled()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_READ_ONLY,
                                     "server is in read-only mode");
    }
  }

  if (ServerState::instance()->isCoordinator()) {
    ClusterInfo* ci = ClusterInfo::instance();

    TRI_ASSERT(coll);

    auto info =
      ci->getCollection(coll->vocbase().name(), std::to_string(coll->id()));

    return info->updateProperties(props, false);
  } else {
    auto ctx =
      transaction::V8Context::CreateWhenRequired(coll->vocbase(), false);
    SingleCollectionTransaction trx(
      ctx, coll->id(), AccessMode::Type::EXCLUSIVE
    );
    Result res = trx.begin();

    if (!res.ok()) {
      return res;
    }

    // try to write new parameter to file
    bool doSync = DatabaseFeature::DATABASE->forceSyncProperties();
    arangodb::Result updateRes = coll->updateProperties(props, doSync);

    if (!updateRes.ok()) {
      return updateRes;
    }

    auto physical = coll->getPhysical();
    TRI_ASSERT(physical != nullptr);
    return physical->persistProperties();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to rename collections in _graphs as well
////////////////////////////////////////////////////////////////////////////////

static int RenameGraphCollections(TRI_vocbase_t* vocbase,
                                  std::string const& oldName,
                                  std::string const& newName) {
  V8DealerFeature* dealer = V8DealerFeature::DEALER;
  if (dealer == nullptr || !dealer->isEnabled()) {
    return TRI_ERROR_NO_ERROR; // V8 might is disabled
  }
  
  StringBuffer buffer(true);
  buffer.appendText("require('@arangodb/general-graph')._renameCollection(");
  buffer.appendJsonEncoded(oldName.c_str(), oldName.size());
  buffer.appendChar(',');
  buffer.appendJsonEncoded(newName.c_str(), newName.size());
  buffer.appendText(");");

  V8Context* context = dealer->enterContext(vocbase, false);
  if (context == nullptr) {
    LOG_TOPIC(WARN, Logger::FIXME) << "RenameGraphCollections: no V8 context";
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  TRI_DEFER(dealer->exitContext(context));

  auto isolate = context->_isolate;
  v8::HandleScope scope(isolate);
  TRI_ExecuteJavaScriptString(
      isolate, isolate->GetCurrentContext(),
      TRI_V8_ASCII_PAIR_STRING(isolate, buffer.c_str(), buffer.length()),
      TRI_V8_ASCII_STRING(isolate, "collection rename"), false);

  return TRI_ERROR_NO_ERROR;
}

Result Collections::rename(LogicalCollection* coll, std::string const& newName,
                           bool doOverride) {
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
        !exec->canUseCollection(coll->name(), auth::Level::RW)) {
      return TRI_ERROR_FORBIDDEN;
    }
  }

  std::string const oldName(coll->name());
  int res = coll->vocbase().renameCollection(coll, newName, doOverride);

  if (res != TRI_ERROR_NO_ERROR) {
    return Result(res, "cannot rename collection");
  }

  // rename collection inside _graphs as well
  return RenameGraphCollections(&(coll->vocbase()), oldName, newName);
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
  std::string errorMsg;

  int res = ci->dropCollectionCoordinator(databaseName, cid, errorMsg, 120.0);
  if (res != TRI_ERROR_NO_ERROR) {
    return Result(res, errorMsg);
  }
  collection->setStatus(TRI_VOC_COL_STATUS_DELETED);

  return TRI_ERROR_NO_ERROR;
}
#endif

Result Collections::drop(TRI_vocbase_t* vocbase, LogicalCollection* coll,
                         bool allowDropSystem, double timeout) {

  ExecContext const* exec = ExecContext::CURRENT;
  if (exec != nullptr) {
    if  (!exec->canUseDatabase(vocbase->name(), auth::Level::RW) ||
         !exec->canUseCollection(coll->name(), auth::Level::RW)) {
      return Result(TRI_ERROR_FORBIDDEN,
                    "Insufficient rights to drop collection " +
                    coll->name());
    } else if (!exec->isSuperuser() && !ServerState::writeOpsEnabled()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_READ_ONLY,
                                     "server is in read-only mode");
    }
  }

  TRI_ASSERT(coll);
  auto& dbname = coll->vocbase().name();
  std::string const collName = coll->name();

  Result res;
  // If we are a coordinator in a cluster, we have to behave differently:
  if (ServerState::instance()->isCoordinator()) {
#ifdef USE_ENTERPRISE
    res = DropColCoordinatorEnterprise(coll, allowDropSystem);
#else
    res = DropVocbaseColCoordinator(coll, allowDropSystem);
#endif
  } else {
    auto r =
      coll->vocbase().dropCollection(coll->id(), allowDropSystem, timeout).errorNumber();

    if (r != TRI_ERROR_NO_ERROR) {
      res.reset(r, "cannot drop collection");
    }
  }

  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (res.ok() && um != nullptr) {
    um->enumerateUsers([&](auth::User& entry) -> bool {
      return entry.removeCollection(dbname, collName);
    });
  }
  return res;
}

Result Collections::warmup(TRI_vocbase_t& vocbase, LogicalCollection* coll) {
  ExecContext const* exec = ExecContext::CURRENT; // disallow expensive ops

  if (!exec->isSuperuser() && !ServerState::writeOpsEnabled()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_READ_ONLY,
                                   "server is in read-only mode");
  }

  if (ServerState::instance()->isCoordinator()) {
    auto cid = std::to_string(coll->id());

    return warmupOnCoordinator(vocbase.name(), cid);
  }

  auto ctx = transaction::V8Context::CreateWhenRequired(vocbase, false);
  SingleCollectionTransaction trx(ctx, coll->id(), AccessMode::Type::READ);
  Result res = trx.begin();

  if (res.fail()) {
    return res;
  }

  auto idxs = coll->getIndexes();
  auto poster = [](std::function<void()> fn) -> void {
    SchedulerFeature::SCHEDULER->post(fn);
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

Result Collections::revisionId(
    TRI_vocbase_t& vocbase,
    LogicalCollection* coll,
    TRI_voc_rid_t& rid
) {
  TRI_ASSERT(coll != nullptr);
  auto& databaseName = coll->vocbase().name();
  auto cid = std::to_string(coll->id());

  if (ServerState::instance()->isCoordinator()) {
    return revisionOnCoordinator(databaseName, cid, rid);
  }

  auto ctx = transaction::V8Context::CreateWhenRequired(vocbase, true);
  SingleCollectionTransaction trx(ctx, coll->id(), AccessMode::Type::READ);
  Result res = trx.begin();

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  rid = coll->revision(&trx);

  return TRI_ERROR_NO_ERROR;
}

/// @brief Helper implementation similar to ArangoCollection.all() in v8
/*static*/ arangodb::Result Collections::all(
    TRI_vocbase_t& vocbase,
    std::string const& cname,
    DocCallback cb
) {
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
    auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;
    TRI_ASSERT(queryRegistry != nullptr);
    aql::QueryResult queryResult = query.execute(queryRegistry);
    Result res = queryResult.code;
    if (queryResult.code == TRI_ERROR_NO_ERROR) {
      VPackSlice array = queryResult.result->slice();
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
    std::unique_ptr<OperationCursor> opCursor =
    trx.indexScan(cname, transaction::Methods::CursorType::ALL);

    if (!opCursor->hasMore()) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    opCursor->allDocuments([&](LocalDocumentId const& token, VPackSlice doc) {
      cb(doc.resolveExternal());
    }, 1000);

    return trx.finish(res);
  }
}
