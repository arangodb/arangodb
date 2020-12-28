////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>

#include "Agency/AsyncAgencyComm.h"
#include "Agency/AgencyStrings.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Basics/files.h"
#include "Cluster/ActionDescription.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/CreateDatabase.h"
#include "Cluster/DropDatabase.h"
#include "Cluster/Maintenance.h"
#include "ClusterEngine/ClusterEngine.h"
#include "FeaturePhases/AqlFeaturePhase.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "FeaturePhases/V8FeaturePhase.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "Logger/LogMacros.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "Network/NetworkFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Servers.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/vocbase.h"
#include "utils/log.hpp"

#include "Servers.h"
#include "TemplateSpecializer.h"

#include "IResearch/AgencyMock.h"
#include "IResearch/common.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#include "Enterprise/StorageEngine/HotBackupFeature.h"
#endif

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <boost/core/demangle.hpp>
using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::tests;
using namespace arangodb::tests::mocks;

static void SetupGreetingsPhase(MockServer& server) {
  server.addFeature<arangodb::application_features::GreetingsFeaturePhase>(false, false);
  server.addFeature<arangodb::MetricsFeature>(false);
  // We do not need any features from this phase
}

static void SetupBasicFeaturePhase(MockServer& server) {
  SetupGreetingsPhase(server);
  server.addFeature<arangodb::application_features::BasicFeaturePhaseServer>(false);
  server.addFeature<arangodb::ShardingFeature>(false);
  server.addFeature<arangodb::DatabasePathFeature>(false);
}

static void SetupDatabaseFeaturePhase(MockServer& server) {
  SetupBasicFeaturePhase(server);
  server.addFeature<arangodb::application_features::DatabaseFeaturePhase>(false);  // true ??
  server.addFeature<arangodb::AuthenticationFeature>(true);
  server.addFeature<arangodb::DatabaseFeature>(false);
  server.addFeature<arangodb::EngineSelectorFeature>(false);
  server.addFeature<arangodb::StorageEngineFeature>(false);
  server.addFeature<arangodb::SystemDatabaseFeature>(true);
  server.addFeature<arangodb::InitDatabaseFeature>(true, std::vector<std::type_index>{});
  server.addFeature<arangodb::ViewTypesFeature>(false);  // true ??

#if USE_ENTERPRISE
  // required for AuthenticationFeature with USE_ENTERPRISE
  server.addFeature<arangodb::LdapFeature>(false);
#endif

  arangodb::DatabaseFeature::DATABASE = &server.getFeature<arangodb::DatabaseFeature>();
}

static void SetupClusterFeaturePhase(MockServer& server) {
  SetupDatabaseFeaturePhase(server);
  server.addFeature<arangodb::application_features::ClusterFeaturePhase>(false);
  server.addFeature<arangodb::ClusterFeature>(false);
 
  // fake the exit code with which unresolved futures are returned on
  // shutdown. if we don't do this lots of places in ClusterInfo will
  // report failures during testing
  server.getFeature<ClusterFeature>().setSyncerShutdownCode(TRI_ERROR_NO_ERROR);
}

static void SetupCommunicationFeaturePhase(MockServer& server) {
  SetupClusterFeaturePhase(server);
  server.addFeature<arangodb::application_features::CommunicationFeaturePhase>(false);
  // This phase is empty...
}

static void SetupV8Phase(MockServer& server) {
  SetupCommunicationFeaturePhase(server);
  server.addFeature<arangodb::application_features::V8FeaturePhase>(false);
  server.addFeature<arangodb::V8DealerFeature>(false);
}

static void SetupAqlPhase(MockServer& server) {
  SetupV8Phase(server);
  server.addFeature<arangodb::application_features::AqlFeaturePhase>(false);
  server.addFeature<arangodb::QueryRegistryFeature>(false);

  server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>(true);
  {
    auto& feature = server.addFeature<arangodb::iresearch::IResearchFeature>(true);
    feature.collectOptions(server.server().options());
    feature.validateOptions(server.server().options());
  }

  server.addFeature<arangodb::aql::AqlFunctionFeature>(true);
  server.addFeature<arangodb::aql::OptimizerRulesFeature>(true);
  server.addFeature<arangodb::AqlFeature>(true);

#ifdef USE_ENTERPRISE
  server.addFeature<arangodb::HotBackupFeature>(false);
#endif
}

MockServer::MockServer()
    : _server(std::make_shared<arangodb::options::ProgramOptions>("", "", "", nullptr), nullptr),
      _engine(_server),
      _started(false) {
  init();
}

MockServer::~MockServer() {
  stopFeatures();
  _server.setStateUnsafe(_oldApplicationServerState);
}

application_features::ApplicationServer& MockServer::server() {
  return _server;
}

void MockServer::init() {
  _oldApplicationServerState = _server.state();

  _server.setStateUnsafe(arangodb::application_features::ApplicationServer::State::IN_WAIT);
  arangodb::transaction::Methods::clearDataSourceRegistrationCallbacks();
}

void MockServer::startFeatures() {
  using arangodb::application_features::ApplicationFeature;

  // user can no longer add features with addFeature, must add them directly
  // to underlying server()
  _started = true;

  _server.setupDependencies(false);
  auto orderedFeatures = _server.getOrderedFeatures();

  _server.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(&_engine);

  if (_server.hasFeature<arangodb::SchedulerFeature>()) {
    auto& sched = _server.getFeature<arangodb::SchedulerFeature>();
    // Needed to set nrMaximalThreads
    sched.validateOptions(
        std::make_shared<arangodb::options::ProgramOptions>("", "", "", nullptr));
  }

  for (ApplicationFeature& f : orderedFeatures) {
    if (f.name() == "Endpoint") {
      // We need this feature to be there but do not use it.
      continue;
    }
    try {
      f.prepare();
    } catch (...) {
      LOG_DEVEL << "unexpected exception in "
                << boost::core::demangle(typeid(f).name()) << "::prepare";
    }
  }

  if (_server.hasFeature<arangodb::DatabaseFeature>()) {
    auto& dbFeature = _server.getFeature<arangodb::DatabaseFeature>();
    // Only add a database if we have the feature.
    auto const databases = arangodb::velocypack::Parser::fromJson(
        R"([{"name": ")" + arangodb::StaticStrings::SystemDatabase + R"("}])");
    dbFeature.loadDatabases(databases->slice());
  }

  for (ApplicationFeature& f : orderedFeatures) {
    auto info = _features.find(&f);
    // We only start those features...
    TRI_ASSERT(info != _features.end());
    if (info->second) {
      try {
        f.start();
      } catch (...) {
        LOG_DEVEL << "unexpected exception in "
                  << boost::core::demangle(typeid(f).name()) << "::start";
      }
    }
  }

  if (_server.hasFeature<arangodb::DatabasePathFeature>()) {
    auto& dbPathFeature = _server.getFeature<arangodb::DatabasePathFeature>();
    // Inject a testFileSystemPath
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
    _testFilesystemPath = dbPathFeature.directory();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(_testFilesystemPath.c_str(), systemError, systemErrorStr);
  }
}

void MockServer::stopFeatures() {
  using arangodb::application_features::ApplicationFeature;

  if (!_testFilesystemPath.empty()) {
    TRI_RemoveDirectory(_testFilesystemPath.c_str());
  }

  // need to shut down in reverse order
  auto orderedFeatures = _server.getOrderedFeatures();
  std::reverse(orderedFeatures.begin(), orderedFeatures.end());

  // destroy application features
  for (ApplicationFeature& f : orderedFeatures) {
    auto info = _features.find(&f);
    // We only start those features...
    TRI_ASSERT(info != _features.end());
    if (info->second) {
      try {
        f.stop();
      } catch (...) {
        LOG_DEVEL << "unexpected exception in "
                  << boost::core::demangle(typeid(f).name()) << "::stop";
      }
    }
  }

  for (ApplicationFeature& f : orderedFeatures) {
    try {
      f.unprepare();
    } catch (...) {
      LOG_DEVEL << "unexpected exception in "
                << boost::core::demangle(typeid(f).name()) << "::unprepare";
    }
  }
}

TRI_vocbase_t& MockServer::getSystemDatabase() const {
  TRI_ASSERT(_server.hasFeature<arangodb::DatabaseFeature>());
  auto& database = _server.getFeature<arangodb::DatabaseFeature>();
  auto system = database.useDatabase(arangodb::StaticStrings::SystemDatabase);
  TRI_ASSERT(system != nullptr);
  return *system;
}

MockV8Server::MockV8Server(bool start) : MockServer() {
  // setup required application features
  SetupV8Phase(*this);

  if (start) {
    startFeatures();
  }
}

MockAqlServer::MockAqlServer(bool start) : MockServer() {
  // setup required application features
  SetupAqlPhase(*this);

  if (start) {
    startFeatures();
  }
}

MockAqlServer::~MockAqlServer() {
  arangodb::AqlFeature(_server).stop();  // unset singleton instance
}

std::shared_ptr<arangodb::transaction::Methods> MockAqlServer::createFakeTransaction() const {
  std::vector<std::string> noCollections{};
  transaction::Options opts;
  auto ctx = transaction::StandaloneContext::Create(getSystemDatabase());
  return std::make_shared<arangodb::transaction::Methods>(ctx, noCollections, noCollections,
                                                          noCollections, opts);
}

std::unique_ptr<arangodb::aql::Query> MockAqlServer::createFakeQuery(bool activateTracing,
                                                                     std::string queryString) const {
  auto bindParams = std::make_shared<VPackBuilder>();
  bindParams->openObject();
  bindParams->close();
  auto queryOptions = std::make_shared<VPackBuilder>();
  queryOptions->openObject();
  if (activateTracing) {
    queryOptions->add("profile", VPackValue(aql::PROFILE_LEVEL_TRACE_2));
  }
  queryOptions->close();
  if (queryString.empty()) {
    queryString = "RETURN 1";
  }

  aql::QueryString fakeQueryString(queryString);
  auto query =
      std::make_unique<arangodb::aql::Query>(arangodb::transaction::StandaloneContext::Create(getSystemDatabase()),
                                             fakeQueryString, bindParams, queryOptions);
  query->prepareQuery(aql::SerializationFormat::SHADOWROWS);

//  auto engine =
//      std::make_unique<aql::ExecutionEngine>(*query, aql::SerializationFormat::SHADOWROWS);
//  query->setEngine(std::move(engine));

  return query;
}

MockRestServer::MockRestServer(bool start) : MockServer() {
  SetupV8Phase(*this);
  addFeature<arangodb::QueryRegistryFeature>(false);
  if (start) {
    startFeatures();
  }
}

std::pair<std::vector<consensus::apply_ret_t>, consensus::index_t> AgencyCache::applyTestTransaction(
  consensus::query_t const& trxs) {

  std::unordered_set<uint64_t> uniq;
  std::vector<uint64_t> toCall;
  std::unordered_set<std::string> pc, cc;
  std::pair<std::vector<consensus::apply_ret_t>,consensus::index_t> res;

  {
    std::lock_guard g(_storeLock);
    ++_commitIndex;
    res = std::pair<std::vector<consensus::apply_ret_t>,consensus::index_t>{
      _readDB.applyTransactions(trxs), _commitIndex}; // apply logs
  }
  {
    std::lock_guard g(_callbacksLock);
    for (auto const& trx : VPackArrayIterator(trxs->slice())) {
      handleCallbacksNoLock(trx[0], uniq, toCall, pc, cc);
    }
    {
      std::lock_guard g(_storeLock);
      for (auto const& i : pc) {
        _planChanges.emplace(_commitIndex, i);
      }
      for (auto const& i : cc) {
        _currentChanges.emplace(_commitIndex, i);
      }
    }
  }
  invokeCallbacks(toCall);
  return res;
}

consensus::Store& AgencyCache::store() {
  return _readDB;
}

MockClusterServer::MockClusterServer() : MockServer() {
  _oldRole = arangodb::ServerState::instance()->getRole();

  // Add features
  SetupAqlPhase(*this);

  _server.getFeature<ClusterFeature>().allocateMembers();

  addFeature<arangodb::UpgradeFeature>(false, &_dummy, std::vector<std::type_index>{});
  addFeature<arangodb::ServerSecurityFeature>(false);

  arangodb::network::ConnectionPool::Config config;
  config.numIOThreads = 1;
  config.maxOpenConnections = 8;
  config.verifyHosts = false;
  addFeature<arangodb::NetworkFeature>(true, config);
}

MockClusterServer::~MockClusterServer() {
  _server.getFeature<arangodb::ClusterFeature>().clusterInfo().shutdownSyncers();
  arangodb::ServerState::instance()->setRole(_oldRole);
}

void MockClusterServer::startFeatures() {
  MockServer::startFeatures();

  arangodb::network::ConnectionPool::Config poolConfig;
  poolConfig.clusterInfo = &getFeature<arangodb::ClusterFeature>().clusterInfo();
  poolConfig.numIOThreads = 1;
  poolConfig.maxOpenConnections = 3;
  poolConfig.verifyHosts = false;

  _pool = std::make_unique<AsyncAgencyStorePoolMock>(_server, poolConfig);

  arangodb::AgencyCommHelper::initialize("arango");
  AsyncAgencyCommManager::initialize(server());
  AsyncAgencyCommManager::INSTANCE->pool(_pool.get());
  AsyncAgencyCommManager::INSTANCE->updateEndpoints({"tcp://localhost:4000/"});
  arangodb::AgencyComm(server()).ensureStructureInitialized();

  std::string st = "{\"" + arangodb::ServerState::instance()->getId() + "\":{\"rebootId\":1}}";
  agencyTrx("/arango/Current/ServersKnown", st);
  arangodb::ServerState::instance()->setRebootId(arangodb::RebootId{1});

  // register factories & normalizers
  auto& indexFactory = const_cast<arangodb::IndexFactory&>(_engine.indexFactory());
  auto& factory =
      getFeature<arangodb::iresearch::IResearchFeature>().factory<arangodb::ClusterEngine>();
  indexFactory.emplace(arangodb::iresearch::DATA_SOURCE_TYPE.name(), factory);
  _server.getFeature<arangodb::ClusterFeature>().clusterInfo().startSyncers();

}

consensus::index_t MockClusterServer::agencyTrx(std::string const& key, std::string const& value) {
  // Build an agency transaction:
  auto b = std::make_shared<VPackBuilder>();
  { VPackArrayBuilder trxs(b.get());
    { VPackArrayBuilder trx(b.get());
      { VPackObjectBuilder op(b.get());
        auto b2 = VPackParser::fromJson(value);
        b->add(key, b2->slice());
      }}}
  return std::get<1>(_server.getFeature<ClusterFeature>().agencyCache().applyTestTransaction(b));
}

void MockClusterServer::agencyCreateDatabase(std::string const& name) {
  TemplateSpecializer ts(name);
  std::string st = ts.specialize(plan_dbs_string);

  agencyTrx("/arango/Plan/Databases/" + name, st);
  st = ts.specialize(plan_colls_string);
  agencyTrx("/arango/Plan/Collections/" + name, st);
  st = ts.specialize(current_dbs_string);
  agencyTrx("/arango/Current/Databases/" + name, st);
  st = ts.specialize(current_colls_string);
  agencyTrx("/arango/Current/Collections/" + name, st);

  _server.getFeature<arangodb::ClusterFeature>().clusterInfo().waitForPlan(
    agencyTrx("/arango/Plan/Version", R"=({"op":"increment"})=")).wait();
  _server.getFeature<arangodb::ClusterFeature>().clusterInfo().waitForCurrent(
    agencyTrx("/arango/Current/Version", R"=({"op":"increment"})=")).wait();
}

void MockClusterServer::agencyDropDatabase(std::string const& name) {
  std::string st = R"=({"op":"delete"}))=";

  agencyTrx("/arango/Plan/Databases/" + name, st);
  agencyTrx("/arango/Plan/Collections/" + name, st);
  agencyTrx("/arango/Current/Databases/" + name, st);
  agencyTrx("/arango/Current/Collections/" + name, st);

  _server.getFeature<arangodb::ClusterFeature>().clusterInfo().waitForPlan(
    agencyTrx("/arango/Plan/Version", R"=({"op":"increment"})=")).wait();
  _server.getFeature<arangodb::ClusterFeature>().clusterInfo().waitForCurrent(
    agencyTrx("/arango/Current/Version", R"=({"op":"increment"})=")).wait();
}

MockDBServer::MockDBServer(bool start) : MockClusterServer() {
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::RoleEnum::ROLE_DBSERVER);
  addFeature<arangodb::FlushFeature>(false);        // do not start the thread
  addFeature<arangodb::MaintenanceFeature>(false);  // do not start the thread
  if (start) {
    startFeatures();
    createDatabase("_system");
  }
}

MockDBServer::~MockDBServer() = default;

TRI_vocbase_t* MockDBServer::createDatabase(std::string const& name) {
  agencyCreateDatabase(name);
  // Now we must run a maintenance action to create the database locally,
  // unless it is the system database, in which case this does not work:
  if (name != "_system") {
    maintenance::ActionDescription ad(
      {{std::string(maintenance::NAME), std::string(maintenance::CREATE_DATABASE)},
       {std::string(maintenance::DATABASE), std::string(name)}},
      maintenance::HIGHER_PRIORITY, false);
    auto& mf = _server.getFeature<arangodb::MaintenanceFeature>();
    maintenance::CreateDatabase cd(mf, ad);
    cd.first();  // Does the job
  }

  auto& databaseFeature = _server.getFeature<arangodb::DatabaseFeature>();
  auto vocbase = databaseFeature.lookupDatabase(name);
  TRI_ASSERT(vocbase != nullptr);
  return vocbase;
};

void MockDBServer::dropDatabase(std::string const& name) {
  agencyDropDatabase(name);
  auto& databaseFeature = _server.getFeature<arangodb::DatabaseFeature>();
  auto vocbase = databaseFeature.lookupDatabase(name);
  TRI_ASSERT(vocbase == nullptr);

  // Now we must run a maintenance action to create the database locally:
  maintenance::ActionDescription ad(
    {{std::string(maintenance::NAME), std::string(maintenance::DROP_DATABASE)},
     {std::string(maintenance::DATABASE), std::string(name)}},
    maintenance::HIGHER_PRIORITY, false);
  auto& mf = _server.getFeature<arangodb::MaintenanceFeature>();
  maintenance::DropDatabase dd(mf, ad);
  dd.first();  // Does the job
}

MockCoordinator::MockCoordinator(bool start) : MockClusterServer() {
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::RoleEnum::ROLE_COORDINATOR);
  if (start) {
    startFeatures();
    createDatabase("_system");
  }
}

MockCoordinator::~MockCoordinator() = default;

TRI_vocbase_t* MockCoordinator::createDatabase(std::string const& name) {
  agencyCreateDatabase(name);
  auto& databaseFeature = _server.getFeature<arangodb::DatabaseFeature>();
  auto vocbase = databaseFeature.lookupDatabase(name);
  TRI_ASSERT(vocbase != nullptr);
  return vocbase;
};

void MockCoordinator::dropDatabase(std::string const& name) {
  agencyDropDatabase(name);
  auto& databaseFeature = _server.getFeature<arangodb::DatabaseFeature>();
  auto vocbase = databaseFeature.lookupDatabase(name);
  TRI_ASSERT(vocbase == nullptr);
}
