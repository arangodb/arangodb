////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBIndexFactory.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionContextData.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "RocksDBEngine/RocksDBView.h"
#include "VocBase/ticks.h"

#include <rocksdb/convenience.h>
#include <rocksdb/db.h>
#include <rocksdb/env.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/slice_transform.h>
#include <rocksdb/table.h>
#include <rocksdb/write_batch.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

std::string const RocksDBEngine::EngineName("rocksdb");
std::string const RocksDBEngine::FeatureName("RocksDBEngine");

// create the storage engine
RocksDBEngine::RocksDBEngine(application_features::ApplicationServer* server)
    : StorageEngine(server, EngineName, FeatureName, new RocksDBIndexFactory()),
      _db(nullptr),
      _cmp(new RocksDBComparator()) {
  // inherits order from StorageEngine
}

RocksDBEngine::~RocksDBEngine() { delete _db; }

// inherited from ApplicationFeature
// ---------------------------------

// add the storage engine's specifc options to the global list of options
void RocksDBEngine::collectOptions(std::shared_ptr<options::ProgramOptions>) {}

// validate the storage engine's specific options
void RocksDBEngine::validateOptions(std::shared_ptr<options::ProgramOptions>) {}

// preparation phase for storage engine. can be used for internal setup.
// the storage engine must not start any threads here or write any files
void RocksDBEngine::prepare() {}

void RocksDBEngine::start() {
  // it is already decided that rocksdb is used
  if (!isEnabled()) {
    return;
  }

  // set the database sub-directory for RocksDB
  auto database =
      ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  _path = database->subdirectoryName("engine-rocksdb");

  LOG_TOPIC(TRACE, arangodb::Logger::STARTUP) << "initializing rocksdb, path: "
                                              << _path;

  rocksdb::TransactionDBOptions transactionOptions;

  _options.create_if_missing = true;
  _options.max_open_files = -1;
  _options.comparator = _cmp.get();

  rocksdb::Status status =
      rocksdb::TransactionDB::Open(_options, transactionOptions, _path, &_db);

  if (!status.ok()) {
    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
        << "unable to initialize RocksDB engine: " << status.ToString();
    FATAL_ERROR_EXIT();
  }

  TRI_ASSERT(_db != nullptr);

  if (!systemDatabaseExists()) {
    addSystemDatabase();
  }
}

void RocksDBEngine::stop() {}

void RocksDBEngine::unprepare() {
  if (!isEnabled()) {
    return;
  }

  if (_db) {
    delete _db;
    _db = nullptr;
  }
}

transaction::ContextData* RocksDBEngine::createTransactionContextData() {
  return new RocksDBTransactionContextData;
}

TransactionState* RocksDBEngine::createTransactionState(
    TRI_vocbase_t* vocbase) {
  return new RocksDBTransactionState(vocbase);
}

TransactionCollection* RocksDBEngine::createTransactionCollection(
    TransactionState* state, TRI_voc_cid_t cid, AccessMode::Type accessType,
    int /*nestingLevel*/) {
  return new RocksDBTransactionCollection(state, cid, accessType);
}

void RocksDBEngine::addParametersForNewCollection(VPackBuilder& builder,
                                                  VPackSlice info) {
  if (!info.hasKey("objectId")) {
    builder.add("objectId", VPackValue(std::to_string(TRI_NewTickServer())));
  }
}

void RocksDBEngine::addParametersForNewIndex(VPackBuilder& builder,
                                             VPackSlice info) {
  if (!info.hasKey("objectId")) {
    builder.add("objectId", VPackValue(std::to_string(TRI_NewTickServer())));
  }
}

// create storage-engine specific collection
PhysicalCollection* RocksDBEngine::createPhysicalCollection(
    LogicalCollection* collection, VPackSlice const& info) {
  return new RocksDBCollection(collection, info);
}

// create storage-engine specific view
PhysicalView* RocksDBEngine::createPhysicalView(LogicalView*,
                                                VPackSlice const&) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return nullptr;
}

// inventory functionality
// -----------------------

void RocksDBEngine::getDatabases(arangodb::velocypack::Builder& result) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "getting existing databases";

  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter (_db->NewIterator(readOptions));

  result.openArray();
  auto rSlice = rocksDBSlice(RocksDBEntryType::Database);
  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    auto slice = VPackSlice(iter->value().data());

    //// check format
    // id
    VPackSlice idSlice = slice.get("id");
    if (!idSlice.isString()) {
      LOG_TOPIC(ERR, arangodb::Logger::STARTUP)
          << "found invalid database declaration with non-string id: "
          << slice.toJson();
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }

    // deleted
    if (arangodb::basics::VelocyPackHelper::getBooleanValue(slice, "deleted",
                                                            false)) {
      TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(
          basics::StringUtils::uint64(idSlice.copyString()));

      // database is deleted, skip it!
      LOG_TOPIC(DEBUG, arangodb::Logger::STARTUP) << "found dropped database "
                                                  << id;

      dropDatabase(id);
      continue;
    }

    // name
    VPackSlice nameSlice = slice.get("name");
    if (!nameSlice.isString()) {
      LOG_TOPIC(ERR, arangodb::Logger::STARTUP)
          << "found invalid database declaration with non-string name: "
          << slice.toJson();
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }

    result.add(slice);
  }
  result.close();
}

void RocksDBEngine::getCollectionInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t cid,
                                      arangodb::velocypack::Builder& builder,
                                      bool includeIndexes,
                                      TRI_voc_tick_t maxTick) {
  builder.openObject();
  
  // read collection info from database
  auto key = RocksDBKey::Collection(vocbase->id(), cid);
  auto value = RocksDBValue::Empty(RocksDBEntryType::Collection);
  rocksdb::ReadOptions options;
  rocksdb::Status res = _db->Get(options, key.string(), value.string());
  auto result = rocksutils::convertStatus(res);
  
  if (result.errorNumber() != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(result.errorNumber());
  }

  builder.add("parameters", VPackSlice(value.string()->data()));

  if (includeIndexes) {
    // dump index information
    builder.add("indexes", VPackValue(VPackValueType::Array));
    // TODO
    builder.close();
  }

  builder.close();
}

int RocksDBEngine::getCollectionsAndIndexes(
    TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& result,
    bool wasCleanShutdown, bool isUpgrade) {
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter (_db->NewIterator(readOptions));

  result.openArray();
  auto rSlice = rocksDBSlice(RocksDBEntryType::Collection);
  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    if (vocbase->id() != RocksDBKey::databaseId(iter->key())) {
      continue;
    }

    auto slice = VPackSlice(iter->value().data());

    LOG_TOPIC(TRACE, Logger::FIXME) << "got collection slice: "
                                    << slice.toJson();

    if (arangodb::basics::VelocyPackHelper::readBooleanValue(slice, "deleted",
                                                             false)) {
      continue;
    }
    result.add(slice);
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

int RocksDBEngine::getViews(TRI_vocbase_t* vocbase,
                            arangodb::velocypack::Builder& result) {
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter (_db->NewIterator(readOptions));

  result.openArray();
  auto rSlice = rocksDBSlice(RocksDBEntryType::View);
  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    if (vocbase->id() != !RocksDBKey::databaseId(iter->key())) {
      continue;
    }

    auto slice = VPackSlice(iter->value().data());

    LOG_TOPIC(TRACE, Logger::FIXME) << "got view slice: " << slice.toJson();

    if (arangodb::basics::VelocyPackHelper::readBooleanValue(slice, "deleted",
                                                             false)) {
      continue;
    }
    result.add(slice);
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

std::string RocksDBEngine::databasePath(TRI_vocbase_t const* vocbase) const {
  return std::string();  // no path to be returned here!
}

std::string RocksDBEngine::collectionPath(TRI_vocbase_t const* vocbase,
                                          TRI_voc_cid_t id) const {
  return std::string();  // no path to be returned here
}

void RocksDBEngine::waitForSync(TRI_voc_tick_t tick) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

std::shared_ptr<arangodb::velocypack::Builder>
RocksDBEngine::getReplicationApplierConfiguration(TRI_vocbase_t* vocbase,
                                                  int& status) {
  // TODO!
  status = TRI_ERROR_FILE_NOT_FOUND;
  return std::shared_ptr<arangodb::velocypack::Builder>();
}

int RocksDBEngine::removeReplicationApplierConfiguration(
    TRI_vocbase_t* vocbase) {
  // TODO!
  return TRI_ERROR_NO_ERROR;
}

int RocksDBEngine::saveReplicationApplierConfiguration(
    TRI_vocbase_t* vocbase, arangodb::velocypack::Slice slice, bool doSync) {
  // TODO!
  return TRI_ERROR_NO_ERROR;
}

// database, collection and index management
// -----------------------------------------

TRI_vocbase_t* RocksDBEngine::openDatabase(
    arangodb::velocypack::Slice const& args, bool isUpgrade, int& status) {
  VPackSlice idSlice = args.get("id");
  TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(
      basics::StringUtils::uint64(idSlice.copyString()));
  std::string const name = args.get("name").copyString();

  status = TRI_ERROR_NO_ERROR;

  return openExistingDatabase(id, name, true, isUpgrade);
}

RocksDBEngine::Database* RocksDBEngine::createDatabase(
    TRI_voc_tick_t id, arangodb::velocypack::Slice const& args, int& status) {
  status = TRI_ERROR_NO_ERROR;
  auto vocbase = std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_NORMAL, id,
                                                 args.get("name").copyString());
  return vocbase.release();
}

int RocksDBEngine::writeCreateDatabaseMarker(TRI_voc_tick_t id,
                                             VPackSlice const& slice) {
  auto key = RocksDBKey::Database(id);
  auto value = RocksDBValue::Database(slice);
  rocksdb::WriteOptions options;  // TODO: check which options would make sense

  rocksdb::Status res = _db->Put(options, key.string(), *value.string());
  auto result = rocksutils::convertStatus(res);
  return result.errorNumber();
}

int RocksDBEngine::writeCreateCollectionMarker(TRI_voc_tick_t databaseId,
                                               TRI_voc_cid_t id,
                                               VPackSlice const& slice) {
  auto key = RocksDBKey::Collection(databaseId, id);
  auto value = RocksDBValue::Collection(slice);
  rocksdb::WriteOptions options;  // TODO: check which options would make sense

  rocksdb::Status res = _db->Put(options, key.string(), *value.string());
  auto result = rocksutils::convertStatus(res);
  return result.errorNumber();
}

void RocksDBEngine::prepareDropDatabase(TRI_vocbase_t* vocbase,
                                        bool useWriteMarker, int& status) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

void RocksDBEngine::dropDatabase(Database* database, int& status) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

void RocksDBEngine::waitUntilDeletion(TRI_voc_tick_t /* id */, bool /* force */,
                                      int& status) {
  // can delete databases instantly
  status = TRI_ERROR_NO_ERROR;
}

// wal in recovery
bool RocksDBEngine::inRecovery() {
  // recovery is handled outside of this engine
  return false;
}

void RocksDBEngine::recoveryDone(TRI_vocbase_t* vocbase) {
  // nothing to do here
}

std::string RocksDBEngine::createCollection(
    TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
    arangodb::LogicalCollection const* parameters) {
  VPackBuilder builder =
      parameters->toVelocyPackIgnore({"path", "statusString"}, true);
  int res = writeCreateCollectionMarker(vocbase->id(), id, builder.slice());

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return std::string();  // no need to return a path
}

arangodb::Result RocksDBEngine::persistCollection(
    TRI_vocbase_t* vocbase, arangodb::LogicalCollection const* collection) {
  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(vocbase != nullptr);
  arangodb::Result result;
  if (inRecovery()) {
    // Nothing to do. In recovery we do not write markers.
    return result;
  }
  VPackBuilder builder =
      collection->toVelocyPackIgnore({"path", "statusString"}, true);
  VPackSlice const slice = builder.slice();

  auto cid = collection->cid();
  TRI_ASSERT(cid != 0);
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(cid));

  int res = writeCreateCollectionMarker(vocbase->id(), cid, slice);
  result.reset(res);
  return result;
}

arangodb::Result RocksDBEngine::dropCollection(TRI_vocbase_t* vocbase,
                                               arangodb::LogicalCollection* collection) {
  rocksdb::WriteOptions options;  // TODO: check which options would make sense
  auto key = RocksDBKey::Collection(vocbase->id(), collection->cid());

  rocksdb::Status res = _db->Delete(options, key.string());
  return rocksutils::convertStatus(res);
}

void RocksDBEngine::destroyCollection(TRI_vocbase_t* vocbase,
                                      arangodb::LogicalCollection*) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

void RocksDBEngine::changeCollection(
    TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
    arangodb::LogicalCollection const* parameters, bool doSync) {
  createCollection(vocbase, id, parameters);
}

arangodb::Result RocksDBEngine::renameCollection(
    TRI_vocbase_t* vocbase, arangodb::LogicalCollection const*,
    std::string const& oldName) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return arangodb::Result{};
}

void RocksDBEngine::createIndex(TRI_vocbase_t* vocbase,
                                TRI_voc_cid_t collectionId,
                                TRI_idx_iid_t indexId,
                                arangodb::velocypack::Slice const& data) {
  rocksdb::WriteOptions options;  // TODO: check which options would make sense
  auto key = RocksDBKey::Index(vocbase->id(), collectionId, indexId);
  auto value = RocksDBValue::Index(data);

  rocksdb::Status res = _db->Put(options, key.string(), *value.string());
  auto result = rocksutils::convertStatus(res);
  if (!result.ok()) {
    THROW_ARANGO_EXCEPTION(result.errorNumber());
  }
}

void RocksDBEngine::dropIndex(TRI_vocbase_t* vocbase,
                              TRI_voc_cid_t collectionId, TRI_idx_iid_t id) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

void RocksDBEngine::dropIndexWalMarker(TRI_vocbase_t* vocbase,
                                       TRI_voc_cid_t collectionId,
                                       arangodb::velocypack::Slice const& data,
                                       bool writeMarker, int&) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

void RocksDBEngine::unloadCollection(TRI_vocbase_t* vocbase,
                                     arangodb::LogicalCollection* collection) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

void RocksDBEngine::createView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                               arangodb::LogicalView const*) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

arangodb::Result RocksDBEngine::persistView(TRI_vocbase_t* vocbase,
                                            arangodb::LogicalView const*) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return arangodb::Result{};
}

arangodb::Result RocksDBEngine::dropView(TRI_vocbase_t* vocbase,
                                         arangodb::LogicalView*) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return arangodb::Result{};
}

void RocksDBEngine::destroyView(TRI_vocbase_t* vocbase,
                                arangodb::LogicalView*) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

void RocksDBEngine::changeView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                               arangodb::LogicalView const*, bool doSync) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

std::string RocksDBEngine::createViewDirectoryName(std::string const& basePath,
                                                   TRI_voc_cid_t id) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return "not implemented";
}

void RocksDBEngine::signalCleanup(TRI_vocbase_t*) {
  // nothing to do here
}

// document operations
// -------------------
void RocksDBEngine::iterateDocuments(
    TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
    std::function<void(arangodb::velocypack::Slice const&)> const& cb) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

void RocksDBEngine::addDocumentRevision(
    TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
    arangodb::velocypack::Slice const& document) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

void RocksDBEngine::removeDocumentRevision(
    TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
    arangodb::velocypack::Slice const& document) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

/// @brief remove data of expired compaction blockers
bool RocksDBEngine::cleanupCompactionBlockers(TRI_vocbase_t* vocbase) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return true;
}

/// @brief insert a compaction blocker
int RocksDBEngine::insertCompactionBlocker(TRI_vocbase_t* vocbase, double ttl,
                                           TRI_voc_tick_t& id) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return true;
}

/// @brief touch an existing compaction blocker
int RocksDBEngine::extendCompactionBlocker(TRI_vocbase_t* vocbase,
                                           TRI_voc_tick_t id, double ttl) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return true;
}

/// @brief remove an existing compaction blocker
int RocksDBEngine::removeCompactionBlocker(TRI_vocbase_t* vocbase,
                                           TRI_voc_tick_t id) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return true;
}

/// @brief a callback function that is run while it is guaranteed that there
/// is no compaction ongoing
void RocksDBEngine::preventCompaction(
    TRI_vocbase_t* vocbase,
    std::function<void(TRI_vocbase_t*)> const& callback) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

/// @brief a callback function that is run there is no compaction ongoing
bool RocksDBEngine::tryPreventCompaction(
    TRI_vocbase_t* vocbase, std::function<void(TRI_vocbase_t*)> const& callback,
    bool checkForActiveBlockers) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return true;
}

int RocksDBEngine::shutdownDatabase(TRI_vocbase_t* vocbase) {
  return TRI_ERROR_NO_ERROR;
}

int RocksDBEngine::openCollection(TRI_vocbase_t* vocbase,
                                  LogicalCollection* collection,
                                  bool ignoreErrors) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

/// @brief Add engine-specific AQL functions.
void RocksDBEngine::addAqlFunctions() {
  // there are no specific AQL functions here
  // TODO: potentially add NEAR, WITHIN?
}

/// @brief Add engine-specific optimizer rules
void RocksDBEngine::addOptimizerRules() {
  // there are no specific optimizer rules here
  // TODO: add geo index optimization once there is the geo index
}

/// @brief Add engine-specific V8 functions
void RocksDBEngine::addV8Functions() {
  // there are no specific V8 functions here
  // TODO: add WAL management functions here once they exist in the engine
}

/// @brief Add engine-specific REST handlers
void RocksDBEngine::addRestHandlers(rest::RestHandlerFactory*) {
  // TODO: add /_api/export and /_admin/wal later
}

Result RocksDBEngine::dropDatabase(TRI_voc_tick_t str) {
  LOG_TOPIC(WARN, Logger::STARTUP) << "rocksdb - dropping database: " << str;
  return Result{};
}

bool RocksDBEngine::systemDatabaseExists() {
  velocypack::Builder builder;
  getDatabases(builder);

  for (auto const& item : velocypack::ArrayIterator(builder.slice())) {
    if (item.get("name").copyString() == StaticStrings::SystemDatabase) {
      return true;
    }
  }
  return false;
}

void RocksDBEngine::addSystemDatabase() {
  // create system database entry
  TRI_voc_tick_t id = TRI_NewTickServer();
  VPackBuilder builder;
  builder.openObject();
  builder.add("id", VPackValue(std::to_string(id)));
  builder.add("name", VPackValue(StaticStrings::SystemDatabase));
  builder.add("deleted", VPackValue(false));
  builder.close();

  int res = writeCreateDatabaseMarker(id, builder.slice());

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
        << "unable to write database marker: " << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }
}

/// @brief open an existing database. internal function
TRI_vocbase_t* RocksDBEngine::openExistingDatabase(TRI_voc_tick_t id,
                                                   std::string const& name,
                                                   bool wasCleanShutdown,
                                                   bool isUpgrade) {
  auto vocbase =
      std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_NORMAL, id, name);

  // scan the database path for views
  try {
    VPackBuilder builder;
    int res = getViews(vocbase.get(), builder);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackSlice slice = builder.slice();
    TRI_ASSERT(slice.isArray());

    ViewTypesFeature* viewTypesFeature =
        application_features::ApplicationServer::getFeature<ViewTypesFeature>(
            "ViewTypes");

    for (auto const& it : VPackArrayIterator(slice)) {
      // we found a view that is still active

      std::string type = it.get("type").copyString();
      // will throw if type is invalid
      ViewCreator& creator = viewTypesFeature->creator(type);

      TRI_ASSERT(!it.get("id").isNone());

      std::shared_ptr<LogicalView> view =
          std::make_shared<arangodb::LogicalView>(vocbase.get(), it);

      StorageEngine::registerView(vocbase.get(), view);

      auto physical = static_cast<RocksDBView*>(view->getPhysical());
      TRI_ASSERT(physical != nullptr);

      view->spawnImplementation(creator, it, false);
      view->getImplementation()->open();
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error while opening database: "
                                            << ex.what();
    throw;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "error while opening database: unknown exception";
    throw;
  }

  // scan the database path for collections
  try {
    VPackBuilder builder;
    int res = getCollectionsAndIndexes(vocbase.get(), builder, wasCleanShutdown,
                                       isUpgrade);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackSlice slice = builder.slice();
    TRI_ASSERT(slice.isArray());

    for (auto const& it : VPackArrayIterator(slice)) {
      // we found a collection that is still active
      TRI_ASSERT(!it.get("id").isNone() || !it.get("cid").isNone());
      auto uniqCol =
          std::make_unique<arangodb::LogicalCollection>(vocbase.get(), it);
      auto collection = uniqCol.get();
      TRI_ASSERT(collection != nullptr);
      StorageEngine::registerCollection(vocbase.get(), uniqCol.get());
      // The vocbase has taken over control
      uniqCol.release();

      auto physical =
          static_cast<RocksDBCollection*>(collection->getPhysical());
      TRI_ASSERT(physical != nullptr);

      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "added document collection '"
                                                << collection->name() << "'";
    }

    return vocbase.release();
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error while opening database: "
                                            << ex.what();
    throw;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "error while opening database: unknown exception";
    throw;
  }
}

}  // namespace
