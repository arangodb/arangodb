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

#include "UpgradeTasks.h"

#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Auth/UserManager.h"
#include "Basics/DownCast.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Containers/SmallVector.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/CollectionCreationInfo.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/Properties/CreateCollectionBody.h"
#include "VocBase/Properties/DatabaseConfiguration.h"
#include "VocBase/vocbase.h"

#include <velocypack/Collection.h>

using namespace arangodb;
using namespace arangodb::methods;
using application_features::ApplicationServer;
using basics::VelocyPackHelper;

// Note: this entire file should run with superuser rights

namespace {

arangodb::Result recreateGeoIndex(TRI_vocbase_t& vocbase,
                                  arangodb::LogicalCollection& collection,
                                  arangodb::RocksDBIndex* oldIndex) {
  IndexId iid = oldIndex->id();

  VPackBuilder oldDesc;
  oldIndex->toVelocyPack(oldDesc, Index::makeFlags());
  VPackBuilder overw;

  overw.openObject();
  overw.add(arangodb::StaticStrings::IndexType,
            arangodb::velocypack::Value(
                arangodb::Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)));
  overw.close();

  VPackBuilder newDesc =
      VPackCollection::merge(oldDesc.slice(), overw.slice(), false);
  arangodb::Result res = collection.dropIndex(iid);

  if (res.fail()) {
    return res;
  }

  bool created = false;
  auto newIndex = collection.getPhysical()
                      ->createIndex(newDesc.slice(), /*restore*/ true, created)
                      .waitAndGet();

  if (!created) {
    res.reset(TRI_ERROR_INTERNAL);
  }

  TRI_ASSERT(newIndex->id() == iid);  // will break cluster otherwise
  TRI_ASSERT(newIndex->type() == Index::TRI_IDX_TYPE_GEO_INDEX);

  return res;
}

Result upgradeGeoIndexes(TRI_vocbase_t& vocbase) {
  auto collections = vocbase.collections(false);

  for (auto const& collection : collections) {
    auto indexes = collection->getPhysical()->getReadyIndexes();
    for (auto const& index : indexes) {
      auto* rIndex = basics::downCast<RocksDBIndex>(index.get());
      if (index->type() == Index::TRI_IDX_TYPE_GEO1_INDEX ||
          index->type() == Index::TRI_IDX_TYPE_GEO2_INDEX) {
        LOG_TOPIC("5e53d", INFO, Logger::STARTUP)
            << "Upgrading legacy geo index '" << rIndex->id().id() << "'";

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

Result createSystemCollections(
    TRI_vocbase_t& vocbase,
    std::vector<std::shared_ptr<LogicalCollection>>& createdCollections) {
  OperationOptions options(ExecContext::current());

  std::vector<CreateCollectionBody> systemCollectionsToCreate;
  // the order of systemCollections is important. If we're in _system db, the
  // UsersCollection needs to be first, otherwise, the GraphsCollection must be
  // first.
  containers::SmallVector<std::string, 12> systemCollections;
  std::shared_ptr<LogicalCollection> colToDistributeShardsLike;
  Result res;

  // The Legacy mode is to stay compatible with a _system database that was
  // created somewhere in the 2.X series and upgraded to latest.
  // This still has old-style sharding following _graphs.
  // This is now twice obsolete. First: We follow _users. Second we introduced
  // Collection Groups.
  bool legacyMode = false;
  if (vocbase.isSystem()) {
    // LegacyMode can only show up on system database.
    // We have been sharded by _graphs back then
    std::shared_ptr<LogicalCollection> coll;
    res = methods::Collections::lookup(vocbase, StaticStrings::GraphsCollection,
                                       coll);
    if (res.ok()) {
      TRI_ASSERT(coll);
      if (coll && coll->distributeShardsLike().empty()) {
        // We have a graphs collection, and this is not sharded by something
        // else. Turn on legacyMode
        legacyMode = true;
      }
    }
    // NOTE: We could hard-code this on compile-time
    // List of _system database only collections
    systemCollections.push_back(StaticStrings::UsersCollection);
    systemCollections.push_back(StaticStrings::StatisticsCollection);
    systemCollections.push_back(StaticStrings::Statistics15Collection);
    systemCollections.push_back(StaticStrings::StatisticsRawCollection);
    // All others are available in all other Databases as well.
  }

  systemCollections.push_back(StaticStrings::GraphsCollection);
  systemCollections.push_back(StaticStrings::AnalyzersCollection);
  systemCollections.push_back(StaticStrings::AqlFunctionsCollection);
  systemCollections.push_back(StaticStrings::QueuesCollection);
  systemCollections.push_back(StaticStrings::JobsCollection);
  systemCollections.push_back(StaticStrings::AppsCollection);
  systemCollections.push_back(StaticStrings::AppBundlesCollection);
  systemCollections.push_back(StaticStrings::FrontendCollection);

  TRI_IF_FAILURE("UpgradeTasks::CreateCollectionsExistsGraphAqlFunctions") {
    std::vector<CreateCollectionBody> testSystemCollectionsToCreate;
    std::vector<std::string> testSystemCollections = {
        StaticStrings::GraphsCollection, StaticStrings::AqlFunctionsCollection};

    auto config = vocbase.getDatabaseConfiguration();
    // Override lookup for leading CollectionName
    config.getCollectionGroupSharding =
        [&testSystemCollectionsToCreate, &createdCollections, &vocbase](
            std::string const& name) -> ResultT<UserInputCollectionProperties> {
      // For the time being the leading collection is created as standalone
      // before adding the others. So it has to be part of createdCollections.
      // So let us scan there
      for (auto const& c : testSystemCollectionsToCreate) {
        if (c.name == name) {
          // On new databases the leading collection is in the first position.
          // So we will quickly loop here.
          // During upgrades there may be some collections before, however
          // it is not performance critical.
          return c;
        }
      }

      for (auto const& c : createdCollections) {
        if (c->name() == name) {
          // On new databases the leading collection is in the first position.
          // So we will quickly loop here.
          // During upgrades there may be some collections before, however
          // it is not performance critical.
          return c->getCollectionProperties();
        }
      }
      return Result{
          TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE,
          "Collection not found: " + name + " in database " + vocbase.name()};
    };

    for (auto const& cname : testSystemCollections) {
      CreateCollectionBody newCollection;
      newCollection.name = cname;
      methods::Collections::applySystemCollectionProperties(
          newCollection, vocbase, config, legacyMode);
      testSystemCollectionsToCreate.emplace_back(std::move(newCollection));
    }

    auto cols = methods::Collections::create(
        vocbase, options, testSystemCollectionsToCreate, true, true, true,

        false /* allow system collection creation */);
    if (cols.fail()) {
      return cols.result();
    }
    // capture created collection vector
    createdCollections.insert(std::end(createdCollections),
                              std::begin(cols.get()), std::end(cols.get()));
  }

  auto config = vocbase.getDatabaseConfiguration();
  // Override lookup for leading CollectionName
  config.getCollectionGroupSharding =
      [&systemCollectionsToCreate, &createdCollections, &vocbase](
          std::string const& name) -> ResultT<UserInputCollectionProperties> {
    // For the time being the leading collection is created as standalone
    // before adding the others. So it has to be part of createdCollections.
    // So let us scan there
    for (auto const& c : systemCollectionsToCreate) {
      if (c.name == name) {
        // On new databases the leading collection is in the first position.
        // So we will quickly loop here.
        // During upgrades there may be some collections before, however
        // it is not performance critical.
        return c;
      }
    }

    for (auto const& c : createdCollections) {
      if (c->name() == name) {
        // On new databases the leading collection is in the first position.
        // So we will quickly loop here.
        // During upgrades there may be some collections before, however
        // it is not performance critical.
        return c->getCollectionProperties();
      }
    }
    return Result{
        TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE,
        "Collection not found: " + name + " in database " + vocbase.name()};
  };
  // Now split all collections to be created into two groups:
  // a) already created, we can return those
  // b) to be created, we add them in the systemCollectionsToCreate vector
  for (auto const& cname : systemCollections) {
    std::shared_ptr<LogicalCollection> col;
    res = methods::Collections::lookup(vocbase, cname, col);
    if (col) {
      createdCollections.emplace_back(col);
    }

    if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
      CreateCollectionBody newCollection;
      newCollection.name = cname;
      methods::Collections::applySystemCollectionProperties(
          newCollection, vocbase, config, legacyMode);
      systemCollectionsToCreate.emplace_back(std::move(newCollection));
    }
  }

  // We capture the vector of created LogicalCollections here
  // to use it to create indices later.
  if (!systemCollectionsToCreate.empty()) {
    auto cols = methods::Collections::create(
        vocbase, options, systemCollectionsToCreate, true, true, true,

        false /* allow system collection creation */);
    if (cols.fail()) {
      return cols.result();
    }
    createdCollections.insert(std::end(createdCollections),
                              std::begin(cols.get()), std::end(cols.get()));
  }

  return {TRI_ERROR_NO_ERROR};
}

Result createSystemStatisticsCollections(
    TRI_vocbase_t& vocbase,
    std::vector<std::shared_ptr<LogicalCollection>>& createdCollections) {
  if (vocbase.isSystem()) {
    std::vector<CollectionCreationInfo> systemCollectionsToCreate;
    // the order of systemCollections is important. If we're in _system db, the
    // UsersCollection needs to be first, otherwise, the GraphsCollection must
    // be first.
    std::array<std::string, 3> systemCollections{
        StaticStrings::StatisticsCollection,
        StaticStrings::Statistics15Collection,
        StaticStrings::StatisticsRawCollection,
    };
    std::vector<std::shared_ptr<VPackBuffer<uint8_t>>> buffers;
    Result res;
    OperationOptions options{};
    for (auto const& collection : systemCollections) {
      // No need to batch this.
      // Fresh databases will have a batch run for those collections already.
      // We only hit this on databases that do not have statistics collections
      // yet. Which have to be somewhere from the 2.X series, and never had an
      // upgrade task.
      std::shared_ptr<LogicalCollection> col;
      res = methods::Collections::createSystem(vocbase, options, collection,
                                               false, col);
      if (res.fail()) {
        return res;
      }
      TRI_ASSERT(col) << "Create system collection did not fail but also did "
                         "not create a collection.";
      createdCollections.emplace_back(std::move(col));
    }
  }
  return {TRI_ERROR_NO_ERROR};
}

Result createIndex(
    std::string const& name, Index::IndexType type,
    std::vector<std::string> const& fields, bool unique, bool sparse,
    std::vector<std::shared_ptr<LogicalCollection>> const& collections) {
  // Static helper function that wraps creating an index. If we fail to
  // create an index with some indices created, we clean up by removing all
  // collections later on. Find the collection by name
  auto colIt =
      std::find_if(collections.begin(), collections.end(),
                   [&name](std::shared_ptr<LogicalCollection> const& col) {
                     TRI_ASSERT(col != nullptr);
                     return col->name() == name;
                   });
  if (colIt == collections.end()) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            "Collection " + name + " not found"};
  }
  return methods::Indexes::createIndex(*(*colIt), type, fields, unique, sparse,
                                       false /*estimates*/)
      .waitAndGet();
}

Result createSystemStatisticsIndices(
    TRI_vocbase_t& vocbase,
    std::vector<std::shared_ptr<LogicalCollection>>& collections) {
  Result res;
  if (vocbase.isSystem()) {
    res = ::createIndex(StaticStrings::StatisticsCollection,
                        arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX, {"time"},
                        false, false, collections);
    if (!res.ok() && !res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
      return res;
    }
    res = ::createIndex(StaticStrings::Statistics15Collection,
                        arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX, {"time"},
                        false, false, collections);
    if (!res.ok() && !res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
      return res;
    }
    res = ::createIndex(StaticStrings::StatisticsRawCollection,
                        arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX, {"time"},
                        false, false, collections);
    if (!res.ok() && !res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
      return res;
    }
  }
  return res;
}

Result createSystemCollectionsIndices(
    TRI_vocbase_t& vocbase,
    std::vector<std::shared_ptr<LogicalCollection>>& collections) {
  Result res;
  if (vocbase.isSystem()) {
    res = ::createIndex(StaticStrings::UsersCollection,
                        arangodb::Index::TRI_IDX_TYPE_HASH_INDEX, {"user"},
                        true, true, collections);
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

  res = ::createIndex(StaticStrings::AppsCollection,
                      arangodb::Index::TRI_IDX_TYPE_HASH_INDEX, {"mount"}, true,
                      true, collections);
  if (!res.ok()) {
    return res;
  }
  res = ::createIndex(StaticStrings::JobsCollection,
                      arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX,
                      {"queue", "status", "delayUntil"}, false, false,
                      collections);
  if (!res.ok()) {
    return res;
  }
  res = ::createIndex(StaticStrings::JobsCollection,
                      arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX,
                      {"status", "queue", "delayUntil"}, false, false,
                      collections);
  if (!res.ok()) {
    return res;
  }

  return res;
}

}  // namespace

Result UpgradeTasks::createSystemCollectionsAndIndices(
    TRI_vocbase_t& vocbase, velocypack::Slice slice) {
  // after the call to ::createSystemCollections this vector should contain
  // a LogicalCollection for *every* (required) system collection.
  std::vector<std::shared_ptr<LogicalCollection>> presentSystemCollections;
  Result res = ::createSystemCollections(vocbase, presentSystemCollections);

  // TODO: Maybe check or assert that all collections are present (i.e. were
  //       present or created), raise an error if not?

  if (res.fail()) {
    LOG_TOPIC("94824", ERR, Logger::STARTUP)
        << "could not create system collections"
        << ": error: " << res.errorMessage();
    return res;
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
    LOG_TOPIC("fedc0", ERR, Logger::STARTUP)
        << "could not create indices for system collections"
        << ": error: " << res.errorMessage();
    return res;
  }

  return {};
}

Result UpgradeTasks::createStatisticsCollectionsAndIndices(
    TRI_vocbase_t& vocbase, velocypack::Slice slice) {
  // This vector should after the call to ::createSystemCollections contain
  // a LogicalCollection for *every* (required) system collection.
  std::vector<std::shared_ptr<LogicalCollection>> presentSystemCollections;
  Result res =
      ::createSystemStatisticsCollections(vocbase, presentSystemCollections);

  if (res.fail()) {
    LOG_TOPIC("2824e", ERR, Logger::STARTUP)
        << "could not create system collections"
        << ": error: " << res.errorMessage();
    return res;
  }

  res = ::createSystemStatisticsIndices(vocbase, presentSystemCollections);
  if (res.fail()) {
    LOG_TOPIC("dffbd", ERR, Logger::STARTUP)
        << "could not create indices for system collections"
        << ": error: " << res.errorMessage();
    return res;
  }

  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops '_iresearch_analyzers' collection
////////////////////////////////////////////////////////////////////////////////
Result UpgradeTasks::dropLegacyAnalyzersCollection(
    TRI_vocbase_t& vocbase, velocypack::Slice /*upgradeParams*/) {
  // drop legacy collection if upgrading the system vocbase and collection found
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (!vocbase.server().hasFeature<arangodb::SystemDatabaseFeature>()) {
    LOG_TOPIC("8783e", WARN, Logger::STARTUP)
        << "failure to find '" << arangodb::SystemDatabaseFeature::name()
        << "' feature while registering legacy static analyzers with vocbase '"
        << vocbase.name() << "'";
    return {TRI_ERROR_INTERNAL, "unable to find system database"};
  }
  auto& sysDatabase =
      vocbase.server().getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();
  TRI_ASSERT(sysVocbase.get() == &vocbase ||
             sysVocbase->name() == vocbase.name());
#endif

  // find legacy analyzer collection
  std::shared_ptr<arangodb::LogicalCollection> col;
  auto res = arangodb::methods::Collections::lookup(
      vocbase, StaticStrings::LegacyAnalyzersCollection, col);
  if (col) {
    CollectionDropOptions dropOptions{.allowDropSystem = true,
                                      .allowDropGraphCollection = true};
    res = arangodb::methods::Collections::drop(*col, dropOptions);
  }
  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    // this type of error is expected
    res.reset();
  }
  return res;
}

Result UpgradeTasks::addDefaultUserOther(TRI_vocbase_t& vocbase,
                                         velocypack::Slice params) {
  TRI_ASSERT(!vocbase.isSystem());
  TRI_ASSERT(params.isObject());

  VPackSlice users = params.get("users");

  if (users.isNone()) {
    return {};  // exit, no users were specified
  }
  if (!users.isArray()) {
    LOG_TOPIC("44623", ERR, Logger::STARTUP)
        << "addDefaultUserOther: users is invalid";
    return {TRI_ERROR_INTERNAL, "invalid users array"};
  }
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    return {};  // server does not support users
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
      LOG_TOPIC("b5b8a", WARN, Logger::STARTUP)
          << "could not add database user " << user << ": "
          << res.errorMessage();
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
      // this failure does not abort the operation or make it return an error.
    }
  }
  return {};
}

Result UpgradeTasks::renameReplicationApplierStateFiles(
    TRI_vocbase_t& vocbase, velocypack::Slice slice) {
  std::string const path = vocbase.engine().databasePath();

  std::string const source = arangodb::basics::FileUtils::buildFilename(
      path, "REPLICATION-APPLIER-STATE");

  if (!basics::FileUtils::isRegularFile(source)) {
    // source file does not exist
    return {};
  }

  // copy file REPLICATION-APPLIER-STATE to REPLICATION-APPLIER-STATE-<id>
  return basics::catchToResult([&vocbase, &path, &source]() -> Result {
    std::string const dest = arangodb::basics::FileUtils::buildFilename(
        path, "REPLICATION-APPLIER-STATE-" + std::to_string(vocbase.id()));

    LOG_TOPIC("75337", TRACE, Logger::STARTUP)
        << "copying replication applier file '" << source << "' to '" << dest
        << "'";

    std::string error;
    if (!TRI_CopyFile(source, dest, error)) {
      auto msg = absl::StrCat("could not copy replication applier file '",
                              source, "' to '", dest, "'");
      LOG_TOPIC("6c90c", WARN, Logger::STARTUP) << msg;
      return {TRI_ERROR_INTERNAL, std::move(msg)};
    }
    return {};
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops '_pregel_queries' collection
////////////////////////////////////////////////////////////////////////////////

Result UpgradeTasks::dropPregelQueriesCollection(
    TRI_vocbase_t& vocbase, velocypack::Slice /*upgradeParams*/) {
  std::shared_ptr<arangodb::LogicalCollection> col;
  auto res =
      arangodb::methods::Collections::lookup(vocbase, "_pregel_queries", col);
  if (col) {
    CollectionDropOptions dropOptions{.allowDropSystem = true,
                                      .allowDropGraphCollection = true};
    res = arangodb::methods::Collections::drop(*col, dropOptions);
  }
  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    // this error is expected
    res.reset();
  }
  return res;
}
