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
#include "ClusterEngine/ClusterV8Functions.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesEngine.h"
#include "MMFiles/MMFilesOptimizerRules.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/Version.h"
#include "RestHandler/RestHandlerCreator.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBOptimizerRules.h"
#include "Transaction/Context.h"
#include "Transaction/Options.h"
#include "VocBase/ticks.h"
#include "VocBase/LogicalView.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

// create the storage engine
ClusterEngine::ClusterEngine(application_features::ApplicationServer* server)
  : StorageEngine(server, "Cluster", "ClusterEngine", new ClusterIndexFactory()),
    _actualEngine(nullptr) {
  setOptional(true);
}

ClusterEngine::~ClusterEngine() { }

bool ClusterEngine::isRocksDB() const {
  return _actualEngine && _actualEngine->name() == RocksDBEngine::FeatureName;
}

bool ClusterEngine::isMMFiles() const {
  return _actualEngine && _actualEngine->name() == MMFilesEngine::FeatureName;
}

ClusterEngineType ClusterEngine::engineType() const {
  TRI_ASSERT(_actualEngine != nullptr);

  if (_actualEngine->name() == MMFilesEngine::FeatureName) {
    return ClusterEngineType::MMFilesEngine;
  } else if (_actualEngine->name() == RocksDBEngine::FeatureName) {
    return ClusterEngineType::RocksDBEngine;
  }

  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

// inherited from ApplicationFeature
// ---------------------------------

// preparation phase for storage engine. can be used for internal setup.
// the storage engine must not start any threads here or write any files
void ClusterEngine::prepare() {
  // get base path from DatabaseServerFeature
  auto databasePathFeature =
      application_features::ApplicationServer::getFeature<DatabasePathFeature>(
          "DatabasePath");
  _basePath = databasePathFeature->directory();

  TRI_ASSERT(!_basePath.empty());
  if (!ServerState::instance()->isCoordinator()) {
    setEnabled(false);
  }
}

void ClusterEngine::start() {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
}

TransactionManager* ClusterEngine::createTransactionManager() {
  return new ClusterTransactionManager();
}

transaction::ContextData* ClusterEngine::createTransactionContextData() {
  return new ClusterTransactionContextData();
}

std::unique_ptr<TransactionState> ClusterEngine::createTransactionState(
    CollectionNameResolver const& resolver,
    transaction::Options const& options
) {
  return std::make_unique<ClusterTransactionState>(
    resolver, TRI_NewTickServer(), options
  );
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
  return new ClusterCollection(collection, engineType(), info);
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
  
}

int ClusterEngine::getCollectionsAndIndexes(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Builder& result,
    bool wasCleanShutdown,
    bool isUpgrade
) {

  return TRI_ERROR_NO_ERROR;
}

int ClusterEngine::getViews(
    TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result
) {

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
  return false;
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
  if (engineType() == ClusterEngineType::MMFilesEngine) {
    MMFilesOptimizerRules::registerResources();
  } else if (engineType() == ClusterEngineType::RocksDBEngine) {
    RocksDBOptimizerRules::registerResources();
  } else {
    TRI_ASSERT(false);
  }
}

/// @brief Add engine-specific V8 functions
void ClusterEngine::addV8Functions() {
  ClusterV8Functions::registerResources();
}

/// @brief Add engine-specific REST handlers
void ClusterEngine::addRestHandlers(rest::RestHandlerFactory* handlerFactory) {
  ClusterRestHandlers::registerResources(handlerFactory);
}
  
void ClusterEngine::waitForEstimatorSync(std::chrono::milliseconds maxWaitTime) {
  // fixes tests by allowing us to reload the cluster selectivity estimates
  // If test `shell-cluster-collection-selectivity.js` fails consider increasing timeout
  std::this_thread::sleep_for(std::chrono::milliseconds(2500));
}

/// @brief open an existing database. internal function
TRI_vocbase_t* ClusterEngine::openExistingDatabase(TRI_voc_tick_t id,
                                                   std::string const& name,
                                                   bool wasCleanShutdown,
                                                   bool isUpgrade) {
  // TODO make this a coordinator type vocbase
  auto vocbase =
      std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_NORMAL, id, name);
  return vocbase.release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------