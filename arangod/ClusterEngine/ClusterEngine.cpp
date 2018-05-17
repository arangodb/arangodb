////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "ClusterEngine.h"
#include "ApplicationFeatures/RocksDBOptionFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/RocksDBLogger.h"
#include "Basics/StaticStrings.h"
#include "Basics/Thread.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/build.h"
#include "ClusterEngine/ClusterAqlFunctions.h"
#include "ClusterEngine/ClusterCollection.h"
#include "ClusterEngine/ClusterIndexFactory.h"
#include "ClusterEngine/ClusterRestHandlers.h"
#include "ClusterEngine/ClusterTransactionCollection.h"
#include "ClusterEngine/ClusterTransactionContextData.h"
#include "ClusterEngine/ClusterTransactionManager.h"
#include "ClusterEngine/ClusterTransactionState.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesEngine.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/Version.h"
#include "RestHandler/RestHandlerCreator.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Transaction/Context.h"
#include "Transaction/Options.h"
#include "VocBase/ticks.h"
#include "VocBase/LogicalView.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {


// create the storage engine
ClusterEngine::ClusterEngine(application_features::ApplicationServer* server)
    : StorageEngine(server, "Cluster", "ClusterEngine", new ClusterIndexFactory()) {
}

ClusterEngine::~ClusterEngine() { }
  
bool ClusterEngine::isRocksDB() const {
  return _actualEngine->name() == RocksDBEngine::FeatureName;
}

bool ClusterEngine::isMMFiles() const {
  return _actualEngine->name() == MMFilesEngine::FeatureName;
}
  
// inherited from ApplicationFeature
// ---------------------------------

// add the storage engine's specifc options to the global list of options
void ClusterEngine::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  /*options->addSection("rocksdb", "RocksDB engine specific configuration");

  // control transaction size for RocksDB engine
  options->addOption("--rocksdb.max-transaction-size",
                     "transaction size limit (in bytes)",
                     new UInt64Parameter(&_maxTransactionSize));

  options->addOption("--rocksdb.intermediate-commit-size",
                     "an intermediate commit will be performed automatically "
                     "when a transaction "
                     "has accumulated operations of this size (in bytes)",
                     new UInt64Parameter(&_intermediateCommitSize));

  options->addOption("--rocksdb.intermediate-commit-count",
                     "an intermediate commit will be performed automatically "
                     "when this number of "
                     "operations is reached in a transaction",
                     new UInt64Parameter(&_intermediateCommitCount));*/
}

// validate the storage engine's specific options
void ClusterEngine::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
}

// preparation phase for storage engine. can be used for internal setup.
// the storage engine must not start any threads here or write any files
void ClusterEngine::prepare() {
  // get base path from DatabaseServerFeature
  auto databasePathFeature =
      application_features::ApplicationServer::getFeature<DatabasePathFeature>(
          "DatabasePath");
  _basePath = databasePathFeature->directory();

  TRI_ASSERT(!_basePath.empty());

}

void ClusterEngine::start() {
  // it is already decided that rocksdb is used
  if (!isEnabled()) {
    return;
  }
  
  // set the database sub-directory for RocksDB
  /*auto* databasePathFeature =
      ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  _path = databasePathFeature->subdirectoryName("engine-rocksdb");

  if (!basics::FileUtils::isDirectory(_path)) {
    std::string systemErrorStr;
    long errorNo;

    int res = TRI_CreateRecursiveDirectory(_path.c_str(), errorNo,
                                           systemErrorStr);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "created RocksDB data directory '" << _path << "'";
    } else {
      LOG_TOPIC(FATAL, arangodb::Logger::ENGINES) << "unable to create RocksDB data directory '" << _path << "': " << systemErrorStr;
      FATAL_ERROR_EXIT();
    }
  }*/
}

void ClusterEngine::beginShutdown() {
}

void ClusterEngine::stop() {

}

void ClusterEngine::unprepare() {

}

TransactionManager* ClusterEngine::createTransactionManager() {
  return new ClusterTransactionManager();
}

transaction::ContextData* ClusterEngine::createTransactionContextData() {
  return new ClusterTransactionContextData();
}

std::unique_ptr<TransactionState> ClusterEngine::createTransactionState(
    TRI_vocbase_t& vocbase, transaction::Options const& options) {
  return std::make_unique<ClusterTransactionState>(vocbase, TRI_NewTickServer(), options);
}

TransactionCollection* ClusterEngine::createTransactionCollection(
    TransactionState* state, TRI_voc_cid_t cid, AccessMode::Type accessType,
    int nestingLevel) {
  return new ClusterTransactionCollection(state, cid, accessType, nestingLevel);
}

void ClusterEngine::addParametersForNewCollection(VPackBuilder& builder,
                                                  VPackSlice info) {
  if (isRocksDB()) {
    // deliberately not add objectId
    if (!info.hasKey("cacheEnabled") || !info.get("cacheEnabled").isBool()) {
      builder.add("cacheEnabled", VPackValue(false));
    }
  }
}

void ClusterEngine::addParametersForNewIndex(VPackBuilder& builder,
                                             VPackSlice info) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

// create storage-engine specific collection
PhysicalCollection* ClusterEngine::createPhysicalCollection(
    LogicalCollection* collection, VPackSlice const& info) {
  return new ClusterCollection(collection, info);
}
  
void ClusterEngine::getStatistics(velocypack::Builder& builder) const {
  builder.openObject();
  builder.close();
}

// inventory functionality
// -----------------------

void ClusterEngine::getDatabases(arangodb::velocypack::Builder& result) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "getting existing databases";
  // we should only ever need system here
  VPackArrayBuilder arr(&result);
  VPackObjectBuilder obj(&result);
  obj->add(StaticStrings::DataSourceId, VPackValue("1")); // always pick 1
  obj->add(StaticStrings::DataSourceDeleted, VPackValue(false));
  obj->add(StaticStrings::DataSourceName, VPackValue(StaticStrings::SystemDatabase));
}

void ClusterEngine::getCollectionInfo(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t cid,
    arangodb::velocypack::Builder& builder,
    bool includeIndexes,
    TRI_voc_tick_t maxTick
) {
  
  /*builder.openObject();

  // read collection info from database
  RocksDBKey key;

  key.constructCollection(vocbase.id(), cid);

  rocksdb::PinnableSlice value;
  rocksdb::ReadOptions options;
  rocksdb::Status res = _db->Get(options, RocksDBColumnFamily::definitions(),
                                 key.string(), &value);
  auto result = rocksutils::convertStatus(res);

  if (result.errorNumber() != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(result);
  }

  VPackSlice fullParameters = RocksDBValue::data(value);

  builder.add("parameters", fullParameters);

  if (includeIndexes) {
    // dump index information
    VPackSlice indexes = fullParameters.get("indexes");
    builder.add(VPackValue("indexes"));
    builder.openArray();
    if (indexes.isArray()) {
      for (auto const idx : VPackArrayIterator(indexes)) {
        // This is only allowed to contain user-defined indexes.
        // So we have to exclude Primary + Edge Types
        VPackSlice type = idx.get("type");
        TRI_ASSERT(type.isString());
        if (!type.isEqualString("primary") && !type.isEqualString("edge")) {
          builder.add(idx);
        }
      }
    }
    builder.close();
  }

  builder.close();*/
}

int ClusterEngine::getCollectionsAndIndexes(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Builder& result,
    bool wasCleanShutdown,
    bool isUpgrade
) {
  /*rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(
      _db->NewIterator(readOptions, RocksDBColumnFamily::definitions()));

  result.openArray();

  auto rSlice = rocksDBSlice(RocksDBEntryType::Collection);

  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    if (vocbase.id() != RocksDBKey::databaseId(iter->key())) {
      continue;
    }

    auto slice = VPackSlice(iter->value().data());

    if (arangodb::basics::VelocyPackHelper::readBooleanValue(slice, "deleted",
                                                             false)) {
      continue;
    }
    result.add(slice);
  }

  result.close();*/

  return TRI_ERROR_NO_ERROR;
}

int ClusterEngine::getViews(
    TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result
) {
  /*rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(
      _db->NewIterator(readOptions, RocksDBColumnFamily::definitions()));

  result.openArray();

  auto bounds = RocksDBKeyBounds::DatabaseViews(vocbase.id());

  for (iter->Seek(bounds.start());
       iter->Valid() && iter->key().compare(bounds.end()) < 0; iter->Next()) {
    auto slice = VPackSlice(iter->value().data());

    LOG_TOPIC(TRACE, Logger::FIXME) << "got view slice: " << slice.toJson();

    if (arangodb::basics::VelocyPackHelper::readBooleanValue(slice, "deleted",
                                                             false)) {
      continue;
    }

    result.add(slice);
  }

  result.close();*/

  return TRI_ERROR_NO_ERROR;
}

std::string ClusterEngine::versionFilename(TRI_voc_tick_t id) const {
  return _basePath + TRI_DIR_SEPARATOR_CHAR + "VERSION-" + std::to_string(id);
}

VPackBuilder ClusterEngine::getReplicationApplierConfiguration(
    TRI_vocbase_t* vocbase, int& status) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  return VPackBuilder();
}

VPackBuilder ClusterEngine::getReplicationApplierConfiguration(int& status) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  return VPackBuilder();
}

int ClusterEngine::removeReplicationApplierConfiguration(
    TRI_vocbase_t* vocbase) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

int ClusterEngine::removeReplicationApplierConfiguration() {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

int ClusterEngine::saveReplicationApplierConfiguration(
    TRI_vocbase_t* vocbase, arangodb::velocypack::Slice slice, bool doSync) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

int ClusterEngine::saveReplicationApplierConfiguration(
    arangodb::velocypack::Slice slice, bool doSync) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}
  
Result ClusterEngine::handleSyncKeys(arangodb::DatabaseInitialSyncer&,
                                     arangodb::LogicalCollection*,
                                     std::string const&)  {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

// database, collection and index management
// -----------------------------------------

TRI_vocbase_t* ClusterEngine::openDatabase(
    arangodb::velocypack::Slice const& args, bool isUpgrade, int& status) {
  VPackSlice idSlice = args.get("id");
  TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(
      basics::StringUtils::uint64(idSlice.copyString()));
  std::string const name = args.get("name").copyString();

  status = TRI_ERROR_NO_ERROR;

  return openExistingDatabase(id, name, true, isUpgrade);
}

TRI_vocbase_t* ClusterEngine::createDatabase(
    TRI_voc_tick_t id, arangodb::velocypack::Slice const& args, int& status) {
  status = TRI_ERROR_NO_ERROR;
  auto vocbase = std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_NORMAL, id,
                                                 args.get("name").copyString());
  return vocbase.release();
}

int ClusterEngine::writeCreateDatabaseMarker(TRI_voc_tick_t id,
                                             VPackSlice const& slice) {
  return id == 1 ? TRI_ERROR_NO_ERROR : TRI_ERROR_NOT_IMPLEMENTED;
}

void ClusterEngine::prepareDropDatabase(TRI_vocbase_t* vocbase,
                                        bool useWriteMarker, int& status) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result ClusterEngine::dropDatabase(TRI_vocbase_t* database) {
  TRI_ASSERT(false);
  return TRI_ERROR_NOT_IMPLEMENTED;
}

void ClusterEngine::waitUntilDeletion(TRI_voc_tick_t /* id */, bool /* force */,
                                      int& status) {
  // can delete databases instantly
  status = TRI_ERROR_NO_ERROR;
}

// wal in recovery
bool ClusterEngine::inRecovery() {
  return false;//RocksDBRecoveryManager::instance()->inRecovery();
}

void ClusterEngine::recoveryDone(TRI_vocbase_t& vocbase) {
  // nothing to do here
}

std::string ClusterEngine::createCollection(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t cid,
    arangodb::LogicalCollection const* collection
) {
  TRI_ASSERT(cid != 0);
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(cid));
  return std::string();  // no need to return a path
}

arangodb::Result ClusterEngine::persistCollection(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalCollection const* collection
) {
  return {};
}

arangodb::Result ClusterEngine::dropCollection(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalCollection* collection
) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

void ClusterEngine::destroyCollection(
    TRI_vocbase_t& /*vocbase*/,
    arangodb::LogicalCollection* /*collection*/
) {
  // not required
}

void ClusterEngine::changeCollection(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t id,
    arangodb::LogicalCollection const* parameters,
    bool doSync
) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
  
arangodb::Result ClusterEngine::renameCollection(
                                  TRI_vocbase_t& vocbase,
                                  arangodb::LogicalCollection const* collection,
                                  std::string const& oldName
                                  ) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

void ClusterEngine::createIndex(
    TRI_vocbase_t& /*vocbase*/,
    TRI_voc_cid_t /*collectionId*/,
    TRI_idx_iid_t /*indexId*/,
    arangodb::velocypack::Slice const& /*data*/
) {
}

void ClusterEngine::unloadCollection(
    TRI_vocbase_t& /*vocbase*/,
    arangodb::LogicalCollection* collection
) {
  collection->setStatus(TRI_VOC_COL_STATUS_UNLOADED);
}

void ClusterEngine::createView(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t id,
    arangodb::LogicalView const& /*view*/
) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

// asks the storage engine to persist renaming of a view
// This will write a renameMarker if not in recovery
Result ClusterEngine::renameView(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView const& view,
    std::string const& /*oldName*/
) {
  return persistView(vocbase, view);
}

arangodb::Result ClusterEngine::persistView(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView const& view
) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

arangodb::Result ClusterEngine::dropView(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView* view) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

void ClusterEngine::destroyView(
    TRI_vocbase_t& /*vocbase*/,
    arangodb::LogicalView* /*view*/) noexcept {
  // nothing to do here
}

void ClusterEngine::changeView(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t /*id*/,
    arangodb::LogicalView const& view,
    bool /*doSync*/
) {
  if (inRecovery()) {
    // nothing to do
    return;
  }

  auto const res = persistView(vocbase, view);

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      res.errorNumber(),
      "could not save view properties"
    );
  }
}

void ClusterEngine::signalCleanup(TRI_vocbase_t*) {
  // nothing to do here
}

int ClusterEngine::shutdownDatabase(TRI_vocbase_t* vocbase) {
  return TRI_ERROR_NO_ERROR;
}

/// @brief Add engine-specific AQL functions.
void ClusterEngine::addAqlFunctions() {
  ClusterAqlFunctions::registerResources();
}

/// @brief Add engine-specific optimizer rules
void ClusterEngine::addOptimizerRules() {
  //RocksDBOptimizerRules::registerResources();
}

/// @brief Add engine-specific V8 functions
void ClusterEngine::addV8Functions() {
  // there are no specific V8 functions here
  //RocksDBV8Functions::registerResources();
}

/// @brief Add engine-specific REST handlers
void ClusterEngine::addRestHandlers(rest::RestHandlerFactory* handlerFactory) {
  ClusterRestHandlers::registerResources(handlerFactory);
}

Result ClusterEngine::flushWal(bool waitForSync, bool waitForCollector,
                               bool /*writeShutdownFile*/) {
  return TRI_ERROR_NO_ERROR;
}
  
void ClusterEngine::waitForEstimatorSync(std::chrono::milliseconds maxWaitTime) {
  // noop
}

/// @brief open an existing database. internal function
TRI_vocbase_t* ClusterEngine::openExistingDatabase(TRI_voc_tick_t id,
                                                   std::string const& name,
                                                   bool wasCleanShutdown,
                                                   bool isUpgrade) {
  auto vocbase =
      std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_NORMAL, id, name);
  
  return vocbase.release();

  // scan the database path for views
  /*try {
    VPackBuilder builder;
    int res = getViews(*vocbase, builder);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackSlice const slice = builder.slice();
    TRI_ASSERT(slice.isArray());

    for (auto const& it : VPackArrayIterator(slice)) {
      // we found a view that is still active

      TRI_ASSERT(!it.get("id").isNone());

      auto const view = LogicalView::create(*vocbase, it, false);

      if (!view) {
        auto const message = "failed to instantiate view '" + name + "'";

        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message.c_str());
      }

      StorageEngine::registerView(*vocbase, view);

      view->open();
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
    int res =
      getCollectionsAndIndexes(*vocbase, builder, wasCleanShutdown, isUpgrade);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackSlice slice = builder.slice();
    TRI_ASSERT(slice.isArray());

    for (VPackSlice it : VPackArrayIterator(slice)) {
      // we found a collection that is still active
      TRI_ASSERT(!it.get("id").isNone() || !it.get("cid").isNone());
      auto uniqCol =
        std::make_shared<arangodb::LogicalCollection>(*vocbase, it, false);
      auto collection = uniqCol.get();
      TRI_ASSERT(collection != nullptr);
      StorageEngine::registerCollection(*vocbase, uniqCol);
      auto physical =
          static_cast<RocksDBCollection*>(collection->getPhysical());
      TRI_ASSERT(physical != nullptr);

      physical->deserializeIndexEstimates(settingsManager());
      physical->deserializeKeyGenerator(settingsManager());
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
  }*/
}
  
Result ClusterEngine::createLoggerState(TRI_vocbase_t* vocbase,
                                        VPackBuilder& builder) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result ClusterEngine::createTickRanges(VPackBuilder& builder) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result ClusterEngine::firstTick(uint64_t& tick) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);

}

Result ClusterEngine::lastLogger(
    TRI_vocbase_t* vocbase,
    std::shared_ptr<transaction::Context> transactionContext,
    uint64_t tickStart, uint64_t tickEnd,
    std::shared_ptr<VPackBuilder>& builderSPtr) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

WalAccess const* ClusterEngine::walAccess() const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}


// management methods for synchronizing with external persistent stores
TRI_voc_tick_t ClusterEngine::currentTick() const {
  return 0;
}

TRI_voc_tick_t ClusterEngine::releasedTick() const {
  return 0;
}

void ClusterEngine::releaseTick(TRI_voc_tick_t tick) {}

}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
