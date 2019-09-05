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
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "ClusterEngine/ClusterEngine.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesEngine.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/CollectionCreationInfo.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/vocbase.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::methods;
using application_features::ApplicationServer;
using basics::VelocyPackHelper;

// Note: this entire file should run with superuser rights

namespace {

arangodb::Result recreateGeoIndex(TRI_vocbase_t& vocbase,
                                  arangodb::LogicalCollection& collection,
                                  arangodb::RocksDBIndex* oldIndex) {
  arangodb::Result res;
  TRI_idx_iid_t iid = oldIndex->id();

  VPackBuilder oldDesc;
  oldIndex->toVelocyPack(oldDesc, Index::makeFlags());
  VPackBuilder overw;

  overw.openObject();
  overw.add(arangodb::StaticStrings::IndexType,
            arangodb::velocypack::Value(
                arangodb::Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)));
  overw.close();

  VPackBuilder newDesc = VPackCollection::merge(oldDesc.slice(), overw.slice(), false);
  bool dropped = collection.dropIndex(iid);

  if (!dropped) {
    res.reset(TRI_ERROR_INTERNAL);

    return res;
  }

  bool created = false;
  auto newIndex =
      collection.getPhysical()->createIndex(newDesc.slice(), /*restore*/ true, created);

  if (!created) {
    res.reset(TRI_ERROR_INTERNAL);
  }

  TRI_ASSERT(newIndex->id() == iid);  // will break cluster otherwise
  TRI_ASSERT(newIndex->type() == Index::TRI_IDX_TYPE_GEO_INDEX);

  return res;
}

Result upgradeGeoIndexes(TRI_vocbase_t& vocbase) {
  if (EngineSelectorFeature::engineName() != RocksDBEngine::EngineName) {
    LOG_TOPIC("2cb46", DEBUG, Logger::STARTUP)
        << "No need to upgrade geo indexes!";
    return {};
  }

  auto collections = vocbase.collections(false);

  for (auto collection : collections) {
    auto indexes = collection->getIndexes();
    for (auto index : indexes) {
      RocksDBIndex* rIndex = static_cast<RocksDBIndex*>(index.get());
      if (index->type() == Index::TRI_IDX_TYPE_GEO1_INDEX ||
          index->type() == Index::TRI_IDX_TYPE_GEO2_INDEX) {
        LOG_TOPIC("5e53d", INFO, Logger::STARTUP)
            << "Upgrading legacy geo index '" << rIndex->id() << "'";

        auto res = ::recreateGeoIndex(vocbase, *collection, rIndex);

        if (res.fail()) {
          LOG_TOPIC("5550a", ERR, Logger::STARTUP)
              << "Error upgrading geo indexes " << res.errorMessage();
          return res;
        }
      }
    }
  }
  return {};
}

Result createSystemCollections(TRI_vocbase_t& vocbase,
                               std::vector<std::shared_ptr<LogicalCollection>>& createdCollections) {
  typedef std::function<void(std::shared_ptr<LogicalCollection> const&)> FuncCallback;
  FuncCallback const noop = [](std::shared_ptr<LogicalCollection> const&) -> void {};

  std::vector<CollectionCreationInfo> systemCollectionsToCreate;
  // the order of systemCollections is important. If we're in _system db, the
  // UsersCollection needs to be first, otherwise, the GraphsCollection must be first.
  std::vector<std::string> systemCollections;
  std::shared_ptr<LogicalCollection> colToDistributeShardsLike;
  Result res;

  if (vocbase.isSystem()) {
    // check for legacy sharding, could still be graphs.
    res = methods::Collections::lookup(
        vocbase, StaticStrings::GraphsCollection,
        [&colToDistributeShardsLike](std::shared_ptr<LogicalCollection> const& col) -> void {
          if (col && col.get()->distributeShardsLike().empty()) {
            // We have a graphs collection, and this is not sharded by something else.
            colToDistributeShardsLike = col;
          }
        });

    if (colToDistributeShardsLike == nullptr) {
      // otherwise, we will use UsersCollection for distributeShardsLike
      std::tie(res, colToDistributeShardsLike) =
          methods::Collections::createSystem(vocbase, StaticStrings::UsersCollection, true);
      if (!res.ok()) {
        return res;
      }
    } else {
      systemCollections.push_back(StaticStrings::UsersCollection);
    }

    createdCollections.push_back(colToDistributeShardsLike);
    systemCollections.push_back(StaticStrings::GraphsCollection);
    if (StatisticsFeature::enabled()) {
      systemCollections.push_back(StaticStrings::StatisticsCollection);
      systemCollections.push_back(StaticStrings::Statistics15Collection);
      systemCollections.push_back(StaticStrings::StatisticsRawCollection);
    }
  } else {
    // we will use GraphsCollection for distributeShardsLike
    // this is equal to older versions
    std::tie(res, colToDistributeShardsLike) =
        methods::Collections::createSystem(vocbase, StaticStrings::GraphsCollection, true);
    createdCollections.push_back(colToDistributeShardsLike);
    if (!res.ok()) {
      return res;
    }
  }

  TRI_ASSERT(colToDistributeShardsLike != nullptr);

  systemCollections.push_back(StaticStrings::AnalyzersCollection);
  systemCollections.push_back(StaticStrings::AqlFunctionsCollection);
  systemCollections.push_back(StaticStrings::QueuesCollection);
  systemCollections.push_back(StaticStrings::JobsCollection);
  systemCollections.push_back(StaticStrings::AppsCollection);
  systemCollections.push_back(StaticStrings::AppBundlesCollection);
  systemCollections.push_back(StaticStrings::FrontendCollection);
  systemCollections.push_back(StaticStrings::ModulesCollection);

  TRI_IF_FAILURE("UpgradeTasks::CreateCollectionsExistsGraphAqlFunctions") {
    VPackBuilder testOptions;
    std::vector<std::shared_ptr<VPackBuffer<uint8_t>>> testBuffers;
    std::vector<CollectionCreationInfo> testSystemCollectionsToCreate;
    std::vector<std::string> testSystemCollections = {StaticStrings::GraphsCollection,
                                                      StaticStrings::AqlFunctionsCollection};

    for (auto const& collection : testSystemCollections) {
      VPackBuilder options;
      methods::Collections::createSystemCollectionProperties(collection, options,
                                                             vocbase.isSystem());

      testSystemCollectionsToCreate.emplace_back(
          CollectionCreationInfo{collection, TRI_COL_TYPE_DOCUMENT, options.slice()});
      testBuffers.emplace_back(options.steal());
    }

    methods::Collections::create(
        vocbase, testSystemCollectionsToCreate, true, true, true, colToDistributeShardsLike,
        [&createdCollections](std::vector<std::shared_ptr<LogicalCollection>> const& cols) -> void {
          // capture created collection vector
          createdCollections.insert(std::end(createdCollections),
                                    std::begin(cols), std::end(cols));
        });
  }

  // check wether we need fishbowl collection, or not.
  ServerSecurityFeature* security =
      application_features::ApplicationServer::getFeature<ServerSecurityFeature>(
          "ServerSecurity");
  if (!security->isFoxxStoreDisabled()) {
    systemCollections.push_back(StaticStrings::FishbowlCollection);
  }

  std::vector<std::shared_ptr<VPackBuffer<uint8_t>>> buffers;

  for (auto const& collection : systemCollections) {
    res = methods::Collections::lookup(
        vocbase, collection,
        [&createdCollections](std::shared_ptr<LogicalCollection> const& col) -> void {
          if (col) {
            createdCollections.emplace_back(col);
          }
        });
    if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
      // if not found, create it
      VPackBuilder options;
      methods::Collections::createSystemCollectionProperties(collection, options,
                                                             vocbase.isSystem());

      systemCollectionsToCreate.emplace_back(
          CollectionCreationInfo{collection, TRI_COL_TYPE_DOCUMENT, options.slice()});
      buffers.emplace_back(options.steal());
    }
  }

  // We capture the vector of created LogicalCollections here
  // to use it to create indices later.
  if (systemCollectionsToCreate.size() > 0) {
    res = methods::Collections::create(
        vocbase, systemCollectionsToCreate, true, true, true, colToDistributeShardsLike,
        [&createdCollections](std::vector<std::shared_ptr<LogicalCollection>> const& cols) -> void {
          // capture created collection vector
          createdCollections.insert(std::end(createdCollections),
                                    std::begin(cols), std::end(cols));
        });
    if (res.fail()) {
      return res;
    }
  }

  return {TRI_ERROR_NO_ERROR};
}

Result createSystemStatisticsCollections(TRI_vocbase_t& vocbase,
                                         std::vector<std::shared_ptr<LogicalCollection>>& createdCollections) {
  if (vocbase.isSystem() && StatisticsFeature::enabled()) {
    typedef std::function<void(std::shared_ptr<LogicalCollection> const&)> FuncCallback;
    FuncCallback const noop = [](std::shared_ptr<LogicalCollection> const&) -> void {};

    std::vector<CollectionCreationInfo> systemCollectionsToCreate;
    // the order of systemCollections is important. If we're in _system db, the
    // UsersCollection needs to be first, otherwise, the GraphsCollection must be first.
    std::vector<std::string> systemCollections;

    Result res;
    systemCollections.push_back(StaticStrings::StatisticsCollection);
    systemCollections.push_back(StaticStrings::Statistics15Collection);
    systemCollections.push_back(StaticStrings::StatisticsRawCollection);

    std::vector<std::shared_ptr<VPackBuffer<uint8_t>>> buffers;

    for (auto const& collection : systemCollections) {
      res = methods::Collections::lookup(
          vocbase, collection,
          [&createdCollections](std::shared_ptr<LogicalCollection> const& col) -> void {
            if (col) {
              createdCollections.emplace_back(col);
            }
          });
      if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
        // if not found, create it
        VPackBuilder options;
        options.openObject();
        options.add("isSystem", VPackSlice::trueSlice());
        options.add("waitForSync", VPackSlice::falseSlice());
        options.add("journalSize", VPackValue(1024 * 1024));
        options.close();

        systemCollectionsToCreate.emplace_back(
            CollectionCreationInfo{collection, TRI_COL_TYPE_DOCUMENT, options.slice()});
        buffers.emplace_back(options.steal());
      }
    }

    // We capture the vector of created LogicalCollections here
    // to use it to create indices later.
    if (systemCollectionsToCreate.size() > 0) {
      res = methods::Collections::create(
          vocbase, systemCollectionsToCreate, true, false, false, nullptr,
          [&createdCollections](std::vector<std::shared_ptr<LogicalCollection>> const& cols) -> void {
            // capture created collection vector
            createdCollections.insert(std::end(createdCollections),
                                      std::begin(cols), std::end(cols));
          });
      if (res.fail()) {
        return res;
      }
    }
  }
  return {TRI_ERROR_NO_ERROR};
}

static Result createIndex(std::string const name, Index::IndexType type,
                          std::vector<std::string> const& fields, bool unique, bool sparse,
                          std::vector<std::shared_ptr<LogicalCollection>>& collections) {
  // Static helper function that wraps creating an index. If we fail to
  // create an index with some indices created, we clean up by removing all
  // collections later on. Find the collection by name
  auto colIt = find_if(collections.begin(), collections.end(),
                       [name](std::shared_ptr<LogicalCollection> col) {
                         TRI_ASSERT(col != nullptr);
                         return col->name() == name;
                       });
  if (colIt == collections.end()) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "Collection " + name + " not found");
  }
  return methods::Indexes::createIndex(colIt->get(), type, fields, unique, sparse);
}

Result createSystemStatisticsIndices(TRI_vocbase_t& vocbase,
                                     std::vector<std::shared_ptr<LogicalCollection>>& collections) {
  Result res;
  if (vocbase.isSystem() && StatisticsFeature::enabled()) {
    res = ::createIndex(StaticStrings::StatisticsCollection,
                        arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX, {"time"},
                        false, false, collections);
    if (!res.ok()) {
      return res;
    }
    res = ::createIndex(StaticStrings::Statistics15Collection,
                        arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX, {"time"},
                        false, false, collections);
    if (!res.ok()) {
      return res;
    }
    res = ::createIndex(StaticStrings::StatisticsRawCollection,
                        arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX, {"time"},
                        false, false, collections);
    if (!res.ok()) {
      return res;
    }
  }
  return {TRI_ERROR_NO_ERROR};
}

Result createSystemCollectionsIndices(TRI_vocbase_t& vocbase,
                                      std::vector<std::shared_ptr<LogicalCollection>>& collections) {
  Result res;
  if (vocbase.isSystem()) {
    res = ::createIndex(StaticStrings::UsersCollection, arangodb::Index::TRI_IDX_TYPE_HASH_INDEX,
                        {"user"}, true, true, collections);
    if (!res.ok()) {
      return res;
    }

    res = ::createSystemStatisticsIndices(vocbase, collections);
    if (!res.ok()) {
      return res;
    }
  }

  res = upgradeGeoIndexes(vocbase);
  if (!res.ok()) {
    return res;
  }

  res = ::createIndex(StaticStrings::AppsCollection, arangodb::Index::TRI_IDX_TYPE_HASH_INDEX,
                      {"mount"}, true, true, collections);
  if (!res.ok()) {
    return res;
  }
  res = ::createIndex(StaticStrings::JobsCollection, arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX,
                      {"queue", "status", "delayUntil"}, false, false, collections);
  if (!res.ok()) {
    return res;
  }
  res = ::createIndex(StaticStrings::JobsCollection, arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX,
                      {"status", "queue", "delayUntil"}, false, false, collections);
  if (!res.ok()) {
    return res;
  }

  return {TRI_ERROR_NO_ERROR};
}

}  // namespace

bool UpgradeTasks::createSystemCollectionsAndIndices(TRI_vocbase_t& vocbase,
                                                     arangodb::velocypack::Slice const& slice) {
  Result res;

  // This vector should after the call to ::createSystemCollections contain
  // a LogicalCollection for *every* (required) system collection.
  std::vector<std::shared_ptr<LogicalCollection>> presentSystemCollections;
  res = ::createSystemCollections(vocbase, presentSystemCollections);

  // TODO: Maybe check or assert that all collections are present (i.e. were
  //       present or created), raise an error if not?

  if (res.fail()) {
    LOG_TOPIC("e32fi", ERR, Logger::STARTUP)
        << "could not create system collections"
        << ": error: " << res.errorMessage();
    return false;
  }

  TRI_IF_FAILURE("UpgradeTasks::HideDatabaseUntilCreationIsFinished") {
    // just trigger a sleep here. The client test will create the db async
    // and directly fetch the state of creation. The DB is not allowed to be
    // visible to the outside world.
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  }

  TRI_IF_FAILURE("UpgradeTasks::FatalExitDuringDatabaseCreation") {
    FATAL_ERROR_EXIT();
  }

  res = ::createSystemCollectionsIndices(vocbase, presentSystemCollections);
  if (res.fail()) {
    LOG_TOPIC("e32fx", ERR, Logger::STARTUP)
        << "could not create indices for system collections"
        << ": error: " << res.errorMessage();
    return false;
  }

  return true;
}

bool UpgradeTasks::createStatisticsCollectionsAndIndices(TRI_vocbase_t& vocbase,
                                                         arangodb::velocypack::Slice const& slice) {
  // This vector should after the call to ::createSystemCollections contain
  // a LogicalCollection for *every* (required) system collection.
  std::vector<std::shared_ptr<LogicalCollection>> presentSystemCollections;
  Result res;

  res = ::createSystemStatisticsCollections(vocbase, presentSystemCollections);

  if (res.fail()) {
    LOG_TOPIC("e32fy", ERR, Logger::STARTUP)
        << "could not create system collections"
        << ": error: " << res.errorMessage();
    return false;
  }

  res = ::createSystemStatisticsIndices(vocbase, presentSystemCollections);
  if (res.fail()) {
    LOG_TOPIC("e32fx", ERR, Logger::STARTUP)
        << "could not create indices for system collections"
        << ": error: " << res.errorMessage();
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops '_iresearch_analyzers' collection
////////////////////////////////////////////////////////////////////////////////
bool UpgradeTasks::dropLegacyAnalyzersCollection(TRI_vocbase_t& vocbase,
                                                 arangodb::velocypack::Slice const& /*upgradeParams*/) {
  // drop legacy collection if upgrading the system vocbase and collection found
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* sysDatabase = arangodb::application_features::ApplicationServer::lookupFeature<  // find feature
      arangodb::SystemDatabaseFeature  // feature type
      >();

  if (!sysDatabase) {
    LOG_TOPIC("8783e", WARN, Logger::STARTUP)
        << "failure to find '" << arangodb::SystemDatabaseFeature::name()
        << "' feature while registering legacy static analyzers with vocbase '"
        << vocbase.name() << "'";
    TRI_set_errno(TRI_ERROR_INTERNAL);

    return false;  // internal error
  }

  auto sysVocbase = sysDatabase->use();

  TRI_ASSERT(sysVocbase.get() == &vocbase || sysVocbase->name() == vocbase.name());
#endif

  // find legacy analyzer collection
  arangodb::Result dropRes;
  auto const lookupRes = arangodb::methods::Collections::lookup(
      vocbase, StaticStrings::LegacyAnalyzersCollection,
      [&dropRes](std::shared_ptr<arangodb::LogicalCollection> const& col) -> void {  // callback if found
        if (col) {
          dropRes = arangodb::methods::Collections::drop(*col, true, -1.0);  // -1.0 same as in RestCollectionHandler
        }
      });

  if (lookupRes.ok()) {
    return dropRes.ok();
  }

  return lookupRes.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

bool UpgradeTasks::addDefaultUserOther(TRI_vocbase_t& vocbase,
                                       arangodb::velocypack::Slice const& params) {
  TRI_ASSERT(!vocbase.isSystem());
  TRI_ASSERT(params.isObject());

  VPackSlice users = params.get("users");

  if (users.isNone()) {
    return true;  // exit, no users were specified
  } else if (!users.isArray()) {
    LOG_TOPIC("44623", ERR, Logger::STARTUP)
        << "addDefaultUserOther: users is invalid";
    return false;
  }
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    return true;  // server does not support users
  }

  for (VPackSlice slice : VPackArrayIterator(users)) {
    std::string user =
        VelocyPackHelper::getStringValue(slice, "username", StaticStrings::Empty);
    if (user.empty()) {
      continue;
    }
    std::string passwd = VelocyPackHelper::getStringValue(slice, "passwd", "");
    bool active = VelocyPackHelper::getBooleanValue(slice, "active", true);
    VPackSlice extra = slice.get("extra");
    Result res = um->storeUser(false, user, passwd, active, VPackSlice::noneSlice());
    if (res.fail() && !res.is(TRI_ERROR_USER_DUPLICATE)) {
      LOG_TOPIC("b5b8a", WARN, Logger::STARTUP)
          << "could not add database user " << user << ": " << res.errorMessage();
    } else if (extra.isObject() && !extra.isEmptyObject()) {
      um->updateUser(user, [&](auth::User& user) {
        user.setUserData(VPackBuilder(extra));
        return TRI_ERROR_NO_ERROR;
      });
    }

    res = um->updateUser(user, [&](auth::User& entry) {
      entry.grantDatabase(vocbase.name(), auth::Level::RW);
      entry.grantCollection(vocbase.name(), "*", auth::Level::RW);
      return TRI_ERROR_NO_ERROR;
    });
    if (res.fail()) {
      LOG_TOPIC("60019", WARN, Logger::STARTUP)
          << "could not set permissions for new user " << user << ": "
          << res.errorMessage();
    }
  }
  return true;
}

bool UpgradeTasks::persistLocalDocumentIds(TRI_vocbase_t& vocbase,
                                           arangodb::velocypack::Slice const& slice) {
  if (EngineSelectorFeature::engineName() != MMFilesEngine::EngineName) {
    return true;
  }
  Result res = basics::catchToResult([&vocbase]() -> Result {
    MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
    return engine->persistLocalDocumentIds(vocbase);
  });
  return res.ok();
}

bool UpgradeTasks::renameReplicationApplierStateFiles(TRI_vocbase_t& vocbase,
                                                      arangodb::velocypack::Slice const& slice) {
  if (EngineSelectorFeature::engineName() == MMFilesEngine::EngineName) {
    return true;
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  std::string const path = engine->databasePath(&vocbase);

  std::string const source =
      arangodb::basics::FileUtils::buildFilename(path,
                                                 "REPLICATION-APPLIER-STATE");

  if (!basics::FileUtils::isRegularFile(source)) {
    // source file does not exist
    return true;
  }

  bool result = true;

  // copy file REPLICATION-APPLIER-STATE to REPLICATION-APPLIER-STATE-<id>
  Result res = basics::catchToResult([&vocbase, &path, &source, &result]() -> Result {
    std::string const dest = arangodb::basics::FileUtils::buildFilename(
        path, "REPLICATION-APPLIER-STATE-" + std::to_string(vocbase.id()));

    LOG_TOPIC("75337", TRACE, Logger::STARTUP)
        << "copying replication applier file '" << source << "' to '" << dest << "'";

    std::string error;
    if (!TRI_CopyFile(source.c_str(), dest.c_str(), error)) {
      LOG_TOPIC("6c90c", WARN, Logger::STARTUP)
          << "could not copy replication applier file '" << source << "' to '"
          << dest << "'";
      result = false;
    }
    return Result();
  });
  if (res.fail()) {
    return false;
  }
  return result;
}
