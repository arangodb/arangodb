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
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Containers/SmallVector.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/Properties/CreateCollectionBody.h"
#include "VocBase/Properties/DatabaseConfiguration.h"
#include "VocBase/vocbase.h"

#include <format>
#include <optional>
#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::methods;
using application_features::ApplicationServer;
using basics::VelocyPackHelper;

// Note: this entire file should run with superuser rights

namespace {

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
    // All others are available in all other Databases as well.
  }

  systemCollections.push_back(StaticStrings::GraphsCollection);
  systemCollections.push_back(StaticStrings::AnalyzersCollection);

  TRI_IF_FAILURE("UpgradeTasks::CreateCollectionsExistsGraphAqlFunctions") {
    std::vector<CreateCollectionBody> testSystemCollectionsToCreate;
    std::vector<std::string> testSystemCollections = {
        StaticStrings::GraphsCollection};

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

Result createSystemCollectionsIndices(
    TRI_vocbase_t& vocbase,
    std::vector<std::shared_ptr<LogicalCollection>>& collections) {
  Result res;
  if (vocbase.isSystem()) {
    res = ::createIndex(StaticStrings::UsersCollection,
                        arangodb::Index::TRI_IDX_TYPE_PERSISTENT_INDEX,
                        {"user"}, true, true, collections);
    if (!res.ok()) {
      return res;
    }
  }

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

  // The UserManager has to work for the following task
  um->loadUserCacheAndStartUpdateThread();

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
      um->updateUser(
          user,
          [&](auth::User& user) {
            user.setUserData(VPackBuilder(extra));
            return TRI_ERROR_NO_ERROR;
          },
          auth::UserManager::RetryOnConflict::Yes);
    }

    res = um->updateUser(
        user,
        [&](auth::User& entry) {
          entry.grantDatabase(vocbase.name(), auth::Level::RW);
          entry.grantCollection(vocbase.name(), "*", auth::Level::RW);
          return TRI_ERROR_NO_ERROR;
        },
        auth::UserManager::RetryOnConflict::Yes);
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
    // Drop collection will revoke the rights of all the users that had rights
    // on it, so we need a working UserManager here.
    auth::UserManager* um = AuthenticationFeature::instance()->userManager();
    if (um != nullptr) {
      um->loadUserCacheAndStartUpdateThread();
    }

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

////////////////////////////////////////////////////////////////////////////////
/// @brief drops old statistics collections: '_statistics', '_statistics15',
/// '_statisticsRaw'
////////////////////////////////////////////////////////////////////////////////

Result UpgradeTasks::dropOldStatisticsCollections(
    TRI_vocbase_t& vocbase, velocypack::Slice /*upgradeParams*/) {
  // Drop collection will revoke the rights of all the users that had rights
  // on it, so we need a working UserManager here.
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um != nullptr) {
    um->loadUserCacheAndStartUpdateThread();
  }

  CollectionDropOptions dropOptions{.allowDropSystem = true,
                                    .allowDropGraphCollection = true};

  // List of old statistics collections to drop
  std::vector<std::string> collectionsToDrop = {"_statistics", "_statistics15",
                                                "_statisticsRaw"};

  Result res;
  for (auto const& collectionName : collectionsToDrop) {
    std::shared_ptr<arangodb::LogicalCollection> col;
    auto lookupRes =
        arangodb::methods::Collections::lookup(vocbase, collectionName, col);
    if (col) {
      auto dropRes = arangodb::methods::Collections::drop(*col, dropOptions);
      if (dropRes.fail()) {
        if (!dropRes.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
          // Only propagate non-expected errors
          res = dropRes;
        }
      }
    }
    // If collection doesn't exist (col == nullptr), that's fine, just continue
    // TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND from lookup is also expected
  }

  // TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND is expected if collections don't
  // exist
  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    res.reset();
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops all fulltext indexes (no longer supported since 4.0)
////////////////////////////////////////////////////////////////////////////////

Result UpgradeTasks::dropFulltextIndexes(TRI_vocbase_t& vocbase,
                                         velocypack::Slice /*upgradeParams*/) {
  // On a coordinator, vocbase.collections() returns empty because collections
  // live in ClusterInfo. Use methods::Collections which handles both cases.
  auto collections = methods::Collections::sorted(vocbase);

  // Drop all fulltext indexes from all collections.
  // Uses methods::Indexes::drop which on a coordinator propagates the
  // drop through the agency so that DBServers pick up the change.
  for (auto const& collection : collections) {
    auto indexes = collection->getPhysical()->getReadyIndexes();
    for (auto const& index : indexes) {
      if (index->type() == Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
        LOG_TOPIC("d4e3f", WARN, Logger::STARTUP)
            << "Dropping obsolete fulltext index '" << index->id().id()
            << "' from collection '" << collection->name()
            << "' - fulltext indexes are no longer supported";

        auto res =
            methods::Indexes::drop(*collection, index->id()).waitAndGet();

        if (res.fail()) {
          LOG_TOPIC("d4e40", ERR, Logger::STARTUP)
              << "Error dropping fulltext index: " << res.errorMessage();
          return res;
        }
      }
    }
  }

  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief On DB/single server: rewrite collection definitions in the RocksDB
/// definitions column family to change hash/skiplist index types to persistent.
/// On coordinator: update agency plan to change hash/skiplist index types to
/// persistent in a single transaction.
////////////////////////////////////////////////////////////////////////////////

namespace {

/// Rewrites a single VPack index array, changing any "hash"/"skiplist" type
/// entries to "persistent" and appending "_migrated" to their name.
/// Returns std::nullopt if no changes were needed.
std::optional<velocypack::Builder> rewriteIndexTypesInArray(
    velocypack::Slice indexesSlice) {
  TRI_ASSERT(indexesSlice.isArray());
  bool hasChange = false;
  velocypack::Builder out;
  {
    VPackArrayBuilder arrayGuard(&out);
    for (velocypack::Slice idx : VPackArrayIterator(indexesSlice)) {
      if (!idx.isObject()) {
        out.add(idx);
        continue;
      }
      auto typeSlice = idx.get(StaticStrings::IndexType);
      if (!typeSlice.isString()) {
        out.add(idx);
        continue;
      }
      std::string_view typeStr = typeSlice.stringView();
      if (typeStr != "hash" && typeStr != "skiplist") {
        out.add(idx);
        continue;
      }
      hasChange = true;
      {
        VPackObjectBuilder objectGuard(&out);
        for (auto it : VPackObjectIterator(idx)) {
          std::string_view key = it.key.stringView();
          if (key == StaticStrings::IndexType) {
            out.add(key, velocypack::Value("persistent"));
          } else if (key == StaticStrings::IndexName) {
            std::string newName = it.value.copyString() + "_migrated";
            out.add(key, velocypack::Value(newName));
          } else {
            out.add(key, it.value);
          }
        }
      }
    }
  }
  if (!hasChange) {
    return std::nullopt;
  }
  return out;
}

Result convertHashSkiplistInDefinitionsColumnFamily(TRI_vocbase_t& vocbase) {
  auto& selectorFeature = vocbase.server().getFeature<EngineSelectorFeature>();
  if (!selectorFeature.isRocksDB()) {
    return {};
  }
  auto& engine = selectorFeature.engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();
  auto* cf = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::Definitions);

  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(db->NewIterator(readOptions, cf));

  auto rSlice = rocksDBSlice(RocksDBEntryType::Collection);
  rocksdb::WriteBatch batch;
  bool hasBatchEntries = false;

  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    auto collSlice =
        VPackSlice(reinterpret_cast<uint8_t const*>(iter->value().data()));

    if (basics::VelocyPackHelper::getBooleanValue(
            collSlice, StaticStrings::DataSourceDeleted, false)) {
      continue;
    }

    velocypack::Slice indexesSlice = collSlice.get("indexes");
    if (!indexesSlice.isArray()) {
      continue;
    }

    auto newIndexes = rewriteIndexTypesInArray(indexesSlice);
    if (!newIndexes) {
      continue;
    }

    std::string collName = VelocyPackHelper::getStringValue(
        collSlice, StaticStrings::DataSourceName, "");
    LOG_TOPIC("a1b2c", INFO, Logger::STARTUP) << std::format(
        "Rewriting hash/skiplist index types to persistent in definitions "
        "for collection {}",
        collName);

    velocypack::Builder overwrite;
    overwrite.openObject();
    overwrite.add("indexes", newIndexes->slice());
    overwrite.close();

    velocypack::Builder newCollDef =
        VPackCollection::merge(collSlice, overwrite.slice(), false);

    auto value = RocksDBValue::Collection(newCollDef.slice());
    batch.Put(cf, iter->key(), value.string());
    hasBatchEntries = true;
  }

  if (hasBatchEntries) {
    rocksdb::WriteOptions wo;
    rocksdb::Status s = db->GetRootDB()->Write(wo, &batch);
    if (!s.ok()) {
      return Result(
          TRI_ERROR_INTERNAL,
          std::format("failed to rewrite hash/skiplist index definitions: {}",
                      s.ToString()));
    }
  }

  return {};
}

Result migrateCollectionIndexesInPlan(AgencyComm& ac, AgencyCache& agencyCache,
                                      std::string const& dbName,
                                      std::string const& collId) {
  constexpr static size_t kMaxRetries = 10;
  auto const indexesPath =
      std::format("Plan/Collections/{}/{}/indexes", dbName, collId);

  for (size_t attempt = 0; attempt < kMaxRetries; ++attempt) {
    auto const [acb, readIdx] = agencyCache.read(
        std::vector<std::string>{AgencyCommHelper::path(indexesPath)});

    velocypack::Slice indexesSlice =
        acb->slice()[0].get(std::initializer_list<std::string_view>{
            AgencyCommHelper::path(), "Plan", "Collections", dbName, collId,
            "indexes"});

    if (!indexesSlice.isArray()) {
      return {};
    }

    auto newIndexes = rewriteIndexTypesInArray(indexesSlice);
    if (!newIndexes) {
      return {};
    }

    AgencyOperation setIndexes(indexesPath, AgencyValueOperationType::SET,
                               newIndexes->slice());
    AgencyOperation incrVersion("Plan/Version",
                                AgencySimpleOperationType::INCREMENT_OP);
    AgencyPrecondition pre(indexesPath, AgencyPrecondition::Type::VALUE,
                           indexesSlice);
    AgencyWriteTransaction trx({std::move(setIndexes), std::move(incrVersion)},
                               std::move(pre));
    AgencyCommResult result = ac.sendTransactionWithFailover(trx, 0.0);

    if (result.successful()) {
      return {};
    }

    LOG_TOPIC("b3c4d", WARN, Logger::STARTUP) << std::format(
        "Failed to migrate indexes for collection {} in database {} "
        "(attempt {}/{}): {}",
        collId, dbName, attempt + 1, kMaxRetries, result.errorMessage());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Wait for cache update
    agencyCache.waitForLatestCommitIndex().wait();
  }

  return Result(TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED,
                std::format("failed to migrate hash/skiplist indexes for "
                            "collection {} in database {} after {} attempts",
                            collId, dbName, kMaxRetries));
}

Result convertHashSkiplistIndexesInPlanCoordinator(
    TRI_vocbase_t& vocbase, arangodb::ClusterFeature& clusterFeature) {
  std::string const dbName = vocbase.name();
  std::string const path = std::format("Plan/Collections/{}", dbName);
  auto& agencyCache = clusterFeature.agencyCache();
  auto const [acb, idx] =
      agencyCache.read(std::vector<std::string>{AgencyCommHelper::path(path)});

  velocypack::Slice collectionsSlice =
      acb->slice()[0].get(std::initializer_list<std::string_view>{
          AgencyCommHelper::path(), "Plan", "Collections", dbName});

  if (!collectionsSlice.isObject()) {
    return {};
  }

  AgencyComm ac(vocbase.server());

  for (auto const collEntry : VPackObjectIterator(collectionsSlice)) {
    auto const collId = collEntry.key.copyString();
    auto const res =
        migrateCollectionIndexesInPlan(ac, agencyCache, dbName, collId);
    if (res.fail()) {
      return res;
    }
  }

  return {};
}

}  // namespace

Result UpgradeTasks::migrateHashSkiplistToPersistent(
    TRI_vocbase_t& vocbase, velocypack::Slice /*upgradeParams*/) {
  if (ServerState::instance()->isCoordinator()) {
    auto& clusterFeature = vocbase.server().getFeature<ClusterFeature>();
    return convertHashSkiplistIndexesInPlanCoordinator(vocbase, clusterFeature);
  }
  return convertHashSkiplistInDefinitionsColumnFamily(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops legacy geo1/geo2 indexes
////////////////////////////////////////////////////////////////////////////////

Result UpgradeTasks::dropLegacyGeoIndexes(TRI_vocbase_t& vocbase,
                                          velocypack::Slice /*slice*/) {
  // On a coordinator, vocbase.collections() returns empty because collections
  // live in ClusterInfo. Use methods::Collections which handles both cases.
  auto collections = methods::Collections::sorted(vocbase);

  // Drop all geo1/geo2 indexes from all collections.
  // Uses methods::Indexes::drop which on a coordinator propagates the
  // drop through the agency so that DBServers pick up the change.
  for (auto const& collection : collections) {
    auto indexes = collection->getPhysical()->getReadyIndexes();
    for (auto const& index : indexes) {
      if (index->type() == Index::TRI_IDX_TYPE_GEO1_INDEX ||
          index->type() == Index::TRI_IDX_TYPE_GEO2_INDEX) {
        LOG_TOPIC("d4e3f", WARN, Logger::STARTUP)
            << "Dropping obsolete geo1/geo2 index '" << index->id().id()
            << "' from collection '" << collection->name()
            << "' - geo1/geo2 indexes are no longer supported";

        auto res =
            methods::Indexes::drop(*collection, index->id()).waitAndGet();

        if (res.fail()) {
          LOG_TOPIC("d4e40", ERR, Logger::STARTUP)
              << "Error dropping obsolete geo1/geo2 index: "
              << res.errorMessage();
          return res;
        }
      }
    }
  }
  return {};
}
