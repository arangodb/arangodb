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

// fall back to the using the mock storage engine
bool ClusterEngine::Mocking = false;

// create the storage engine
ClusterEngine::ClusterEngine(application_features::ApplicationServer& server)
  : StorageEngine(
      server,
      "Cluster",
      "ClusterEngine",
      std::unique_ptr<IndexFactory>(new ClusterIndexFactory())
    ),
    _actualEngine(nullptr) {
  setOptional(true);
}

ClusterEngine::~ClusterEngine() { }

bool ClusterEngine::isRocksDB() const {
  return !ClusterEngine::Mocking && _actualEngine && _actualEngine->name() == RocksDBEngine::FeatureName;
}

bool ClusterEngine::isMMFiles() const {
  return !ClusterEngine::Mocking && _actualEngine && _actualEngine->name() == MMFilesEngine::FeatureName;
}

bool ClusterEngine::isMock() const {
  return ClusterEngine::Mocking || (_actualEngine && _actualEngine->name() == "Mock");
}

ClusterEngineType ClusterEngine::engineType() const {
  if (isMock()) {
    return ClusterEngineType::MockEngine;
  }
  TRI_ASSERT(_actualEngine != nullptr);

  if (isMMFiles()) {
    return ClusterEngineType::MMFilesEngine;
  } else if (isRocksDB()) {
    return ClusterEngineType::RocksDBEngine;
  }

  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid engine type");
}

// inherited from ApplicationFeature
// ---------------------------------

// preparation phase for storage engine. can be used for internal setup.
// the storage engine must not start any threads here or write any files
void ClusterEngine::prepare() {
  if (!ServerState::instance()->isCoordinator()) {
    setEnabled(false);
  }
}

void ClusterEngine::start() {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
}

std::unique_ptr<TransactionManager> ClusterEngine::createTransactionManager() {
  return std::unique_ptr<TransactionManager>(new ClusterTransactionManager());
}

std::unique_ptr<transaction::ContextData> ClusterEngine::createTransactionContextData() {
  return std::unique_ptr<transaction::ContextData>(
    new ClusterTransactionContextData()
  );
}

std::unique_ptr<TransactionState> ClusterEngine::createTransactionState(
    TRI_vocbase_t& vocbase,
    transaction::Options const& options
) {
  return std::make_unique<ClusterTransactionState>(
    vocbase, TRI_NewTickServer(), options
  );
}

std::unique_ptr<TransactionCollection> ClusterEngine::createTransactionCollection(
    TransactionState& state,
    TRI_voc_cid_t cid,
    AccessMode::Type accessType,
    int nestingLevel
) {
  return std::unique_ptr<TransactionCollection>(
    new ClusterTransactionCollection(&state, cid, accessType, nestingLevel)
  );
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

// create storage-engine specific collection
std::unique_ptr<PhysicalCollection> ClusterEngine::createPhysicalCollection(
    LogicalCollection& collection,
    VPackSlice const& info
) {
  return std::unique_ptr<PhysicalCollection>(
    new ClusterCollection(collection, engineType(), info)
  );
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

VPackBuilder ClusterEngine::getReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase,
    int& status
) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

VPackBuilder ClusterEngine::getReplicationApplierConfiguration(int& status) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

// database, collection and index management
// -----------------------------------------

std::unique_ptr<TRI_vocbase_t> ClusterEngine::openDatabase(
    arangodb::velocypack::Slice const& args, bool isUpgrade, int& status) {
  VPackSlice idSlice = args.get("id");
  TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(
      basics::StringUtils::uint64(idSlice.copyString()));
  std::string const name = args.get("name").copyString();

  status = TRI_ERROR_NO_ERROR;

  return openExistingDatabase(id, name, true, isUpgrade);
}

std::unique_ptr<TRI_vocbase_t> ClusterEngine::createDatabase(
    TRI_voc_tick_t id, arangodb::velocypack::Slice const& args, int& status) {
  status = TRI_ERROR_NO_ERROR;

  return std::make_unique<TRI_vocbase_t>(
    TRI_VOCBASE_TYPE_COORDINATOR, id, args.get("name").copyString()
  );
}

int ClusterEngine::writeCreateDatabaseMarker(TRI_voc_tick_t id,
                                             VPackSlice const& slice) {
  return TRI_ERROR_NO_ERROR;
}

void ClusterEngine::prepareDropDatabase(
    TRI_vocbase_t& vocbase,
    bool useWriteMarker,
    int& status
) {
  status = TRI_ERROR_NO_ERROR;
}

Result ClusterEngine::dropDatabase(TRI_vocbase_t& database) {
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
  return false; // never
}

void ClusterEngine::recoveryDone(TRI_vocbase_t& vocbase) {
  // nothing to do here
}

std::string ClusterEngine::createCollection(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t cid,
    LogicalCollection const& collection
) {
  TRI_ASSERT(cid != 0);
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(cid));
  return std::string();  // no need to return a path
}

arangodb::Result ClusterEngine::persistCollection(
    TRI_vocbase_t& vocbase,
    LogicalCollection const& collection
) {
  return {};
}

arangodb::Result ClusterEngine::dropCollection(
    TRI_vocbase_t& vocbase,
    LogicalCollection& collection
) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

void ClusterEngine::destroyCollection(
    TRI_vocbase_t& /*vocbase*/,
    LogicalCollection& /*collection*/
) {
  // not required
}

void ClusterEngine::changeCollection(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t id,
    LogicalCollection const& collection,
    bool doSync
) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

arangodb::Result ClusterEngine::renameCollection(
  TRI_vocbase_t& vocbase,
  LogicalCollection const& collection,
  std::string const& oldName
) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

void ClusterEngine::unloadCollection(
    TRI_vocbase_t& /*vocbase*/,
    LogicalCollection& collection
) {
  collection.setStatus(TRI_VOC_COL_STATUS_UNLOADED);
}

Result ClusterEngine::createView(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t id,
    arangodb::LogicalView const& /*view*/
) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

arangodb::Result ClusterEngine::dropView(
    TRI_vocbase_t& vocbase,
    LogicalView& view
) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

void ClusterEngine::destroyView(
    TRI_vocbase_t& /*vocbase*/,
    LogicalView& /*view*/
) noexcept {
  // nothing to do here
}

Result ClusterEngine::changeView(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView const& view,
    bool /*doSync*/
) {
  if (inRecovery()) {
    // nothing to do
    return {};
  }
  return TRI_ERROR_NOT_IMPLEMENTED;
}

void ClusterEngine::signalCleanup(TRI_vocbase_t&) {
  // nothing to do here
}

int ClusterEngine::shutdownDatabase(TRI_vocbase_t& vocbase) {
  return TRI_ERROR_NO_ERROR;
}

/// @brief Add engine-specific optimizer rules
void ClusterEngine::addOptimizerRules() {
  if (engineType() == ClusterEngineType::MMFilesEngine) {
    MMFilesOptimizerRules::registerResources();
  } else if (engineType() == ClusterEngineType::RocksDBEngine) {
    RocksDBOptimizerRules::registerResources();
  } else if (engineType() != ClusterEngineType::MockEngine) {
    // invalid engine type...
    TRI_ASSERT(false);
  }
}

/// @brief Add engine-specific V8 functions
void ClusterEngine::addV8Functions() {
  ClusterV8Functions::registerResources();
}

/// @brief Add engine-specific REST handlers
void ClusterEngine::addRestHandlers(rest::RestHandlerFactory& handlerFactory) {
  ClusterRestHandlers::registerResources(&handlerFactory);
}

void ClusterEngine::waitForEstimatorSync(std::chrono::milliseconds maxWaitTime) {
  // fixes tests by allowing us to reload the cluster selectivity estimates
  // If test `shell-cluster-collection-selectivity.js` fails consider increasing timeout
  std::this_thread::sleep_for(std::chrono::seconds(5));
}

/// @brief open an existing database. internal function
std::unique_ptr<TRI_vocbase_t> ClusterEngine::openExistingDatabase(
    TRI_voc_tick_t id,
    std::string const& name,
    bool wasCleanShutdown,
    bool isUpgrade
) {
  return std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_COORDINATOR, id, name);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
