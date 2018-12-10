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

#include "UpgradeTasks.h"
#include "Agency/AgencyComm.h"
#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesEngine.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::methods;
using application_features::ApplicationServer;
using basics::VelocyPackHelper;

// Note: this entire file should run with superuser rights

namespace {

/// create a collection if it does not exists.
bool createSystemCollection(TRI_vocbase_t* vocbase,
                            std::string const& name) {
  auto res = methods::Collections::lookup(
    vocbase, name, [](std::shared_ptr<LogicalCollection> const&)->void {}
  );

  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    uint32_t defaultReplFactor = 1;
    ClusterFeature* cl =
        ApplicationServer::getFeature<ClusterFeature>("Cluster");

    if (cl != nullptr) {
      defaultReplFactor = cl->systemReplicationFactor();
    }

    VPackBuilder bb;

    bb.openObject();
    bb.add("isSystem", VPackSlice::trueSlice());
    bb.add("waitForSync", VPackSlice::falseSlice());
    bb.add("journalSize", VPackValue(1024 * 1024));
    bb.add("replicationFactor", VPackValue(defaultReplFactor));

    if (name != "_graphs") {
      bb.add("distributeShardsLike", VPackValue("_graphs"));
    }

    bb.close();
    res = Collections::create(
      vocbase,
      name,
      TRI_COL_TYPE_DOCUMENT,
      bb.slice(),
      /*waitsForSyncReplication*/ true,
      /*enforceReplicationFactor*/ true,
      [](std::shared_ptr<LogicalCollection> const&)->void {}
    );
  }

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return true;
}

/// create an index if it does not exist
bool createIndex(TRI_vocbase_t* vocbase, std::string const& name,
                        Index::IndexType type,
                        std::vector<std::string> const& fields, bool unique,
                        bool sparse) {
  VPackBuilder output;
  Result res1, res2;

  res1 = methods::Collections::lookup(
    vocbase,
    name,
    [&](std::shared_ptr<LogicalCollection> const& coll)->void {
      TRI_ASSERT(coll);
      res2 =
        methods::Indexes::createIndex(coll.get(), type, fields, unique, sparse);
    }
  );

  if (res1.fail() || res2.fail()) {
    THROW_ARANGO_EXCEPTION(res1.fail() ? res1 : res2);
  }

  return true;
}

arangodb::Result recreateGeoIndex(TRI_vocbase_t& vocbase,
                                  arangodb::LogicalCollection& collection,
                                  arangodb::RocksDBIndex* oldIndex) {
  arangodb::Result res;
  TRI_idx_iid_t iid = oldIndex->id();

  VPackBuilder oldDesc;
  oldIndex->toVelocyPack(oldDesc, Index::makeFlags());
  VPackBuilder overw;

  overw.openObject();
  overw.add(
    arangodb::StaticStrings::IndexType,
    arangodb::velocypack::Value(
      arangodb::Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)
    )
  );
  overw.close();

  VPackBuilder newDesc = VPackCollection::merge(oldDesc.slice(), overw.slice(), false);
  bool dropped = collection.dropIndex(iid);

  if (!dropped) {
    res.reset(TRI_ERROR_INTERNAL);

    return res;
  }

  bool created = false;
  auto newIndex = collection.getPhysical()->createIndex(newDesc.slice(), /*restore*/true, created);

  if (!created) {
    res.reset(TRI_ERROR_INTERNAL);
  }

  TRI_ASSERT(newIndex->id() == iid); // will break cluster otherwise
  TRI_ASSERT(newIndex->type() == Index::TRI_IDX_TYPE_GEO_INDEX);

  return res;
}
}  // namespace

bool UpgradeTasks::upgradeGeoIndexes(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  if (EngineSelectorFeature::engineName() != "rocksdb") {
    LOG_TOPIC(DEBUG, Logger::STARTUP) << "No need to upgrade geo indexes!";
    return true;
  }

  auto collections = vocbase.collections(false);

  for (auto collection : collections) {
    auto indexes = collection->getIndexes();
    for (auto index : indexes) {
      RocksDBIndex* rIndex = static_cast<RocksDBIndex*>(index.get());
      if (index->type() == Index::TRI_IDX_TYPE_GEO1_INDEX ||
          index->type() == Index::TRI_IDX_TYPE_GEO2_INDEX) {
        LOG_TOPIC(INFO, Logger::STARTUP)
            << "Upgrading legacy geo index '" << rIndex->id() << "'";

        auto res = ::recreateGeoIndex(vocbase, *collection, rIndex);

        if (res.fail()) {
          LOG_TOPIC(ERR, Logger::STARTUP) << "Error upgrading geo indexes " << res.errorMessage();
          return false;
        }
      }
    }
  }

  return true;
}

bool UpgradeTasks::setupGraphs(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  return ::createSystemCollection(&vocbase, "_graphs");
}

bool UpgradeTasks::setupUsers(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  return ::createSystemCollection(&vocbase, "_users");
}

bool UpgradeTasks::createUsersIndex(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  TRI_ASSERT(vocbase.isSystem());

  return ::createIndex(
    &vocbase,
    "_users",
    Index::TRI_IDX_TYPE_HASH_INDEX,
    {"user"},
    /*unique*/ true,
    /*sparse*/ true
  );
}

bool UpgradeTasks::addDefaultUserOther(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& params
) {
  TRI_ASSERT(!vocbase.isSystem());
  TRI_ASSERT(params.isObject());

  VPackSlice users = params.get("users");

  if (users.isNone()) {
    return true;  // exit, no users were specified
  } else if (!users.isArray()) {
    LOG_TOPIC(ERR, Logger::STARTUP) << "addDefaultUserOther: users is invalid";
    return false;
  }
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    return true;  // server does not support users
  }

  for (VPackSlice slice : VPackArrayIterator(users)) {
    std::string user = VelocyPackHelper::getStringValue(slice, "username",
                                                        StaticStrings::Empty);
    if (user.empty()) {
      continue;
    }
    std::string passwd = VelocyPackHelper::getStringValue(slice, "passwd", "");
    bool active = VelocyPackHelper::getBooleanValue(slice, "active", true);
    VPackSlice extra = slice.get("extra");
    Result res =
        um->storeUser(false, user, passwd, active, VPackSlice::noneSlice());
    if (res.fail() && !res.is(TRI_ERROR_USER_DUPLICATE)) {
      LOG_TOPIC(WARN, Logger::STARTUP)
          << "could not add database user " << user;
    } else if (extra.isObject() && !extra.isEmptyObject()) {
      um->updateUser(user, [&](auth::User& user) {
        user.setUserData(VPackBuilder(extra));
        return TRI_ERROR_NO_ERROR;
      });
    }
  }
  return true;
}

bool UpgradeTasks::setupAnalyzers(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  return ::createSystemCollection(&vocbase, "_iresearch_analyzers");
}

bool UpgradeTasks::setupAqlFunctions(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  return ::createSystemCollection(&vocbase, "_aqlfunctions");
}

bool UpgradeTasks::createFrontend(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  return ::createSystemCollection(&vocbase, "_frontend");
}

bool UpgradeTasks::setupQueues(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  return ::createSystemCollection(&vocbase, "_queues");
}

bool UpgradeTasks::setupJobs(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  return ::createSystemCollection(&vocbase, "_jobs");
}

bool UpgradeTasks::createJobsIndex(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  ::createSystemCollection(&vocbase, "_jobs");
  ::createIndex(
    &vocbase,
    "_jobs",
    Index::TRI_IDX_TYPE_SKIPLIST_INDEX,
    {"queue", "status", "delayUntil"},
    /*unique*/ true,
    /*sparse*/ true
  );
  ::createIndex(
    &vocbase,
    "_jobs",
    Index::TRI_IDX_TYPE_SKIPLIST_INDEX,
    {"status", "queue", "delayUntil"},
    /*unique*/ true,
    /*sparse*/ true
  );

  return true;
}

bool UpgradeTasks::setupApps(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  return ::createSystemCollection(&vocbase, "_apps");
}

bool UpgradeTasks::createAppsIndex(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  return ::createIndex(
    &vocbase,
    "_apps",
    Index::TRI_IDX_TYPE_HASH_INDEX,
    {"mount"},
    /*unique*/ true,
    /*sparse*/ true
  );
}

bool UpgradeTasks::setupAppBundles(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  return ::createSystemCollection(&vocbase, "_appbundles");
}

bool UpgradeTasks::persistLocalDocumentIds(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& slice
) {
  if (EngineSelectorFeature::engineName() == MMFilesEngine::EngineName) {
    Result res = basics::catchToResult([&vocbase]() -> Result {
      MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
      return engine->persistLocalDocumentIds(vocbase);
    });
    return res.ok();
  }
  return true;
}
