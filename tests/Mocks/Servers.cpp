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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Servers.h"
#include "ApplicationFeatures/AQLPhase.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/BasicPhase.h"
#include "ApplicationFeatures/ClusterPhase.h"
#include "ApplicationFeatures/CommunicationPhase.h"
#include "ApplicationFeatures/DatabasePhase.h"
#include "ApplicationFeatures/GreetingsPhase.h"
#include "ApplicationFeatures/V8Phase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Basics/files.h"
#include "Cluster/ActionDescription.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/CreateDatabase.h"
#include "Cluster/DropDatabase.h"
#include "Cluster/Maintenance.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "Logger/LogTopic.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/vocbase.h"
#include "utils/log.hpp"

#include "../IResearch/AgencyMock.h"
#include "../IResearch/common.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::mocks;

namespace {
struct ClusterCommResetter : public arangodb::ClusterComm {
  static void reset() { arangodb::ClusterComm::_theInstanceInit.store(0); }
};

char const* plan_dbs_string =
#include "plan_dbs_db.h"
    ;

char const* plan_colls_string =
#include "plan_colls_db.h"
    ;

char const* current_dbs_string =
#include "current_dbs_db.h"
    ;

char const* current_colls_string =
#include "current_colls_db.h"
    ;

class TemplateSpecializer {
  std::unordered_map<std::string, std::string> _replacements;
  int _nextServerNumber;
  std::string _dbName;

  enum ReplacementCase { Not, Number, Shard, DBServer, DBName };

 public:
  TemplateSpecializer(std::string const& dbName)
      : _nextServerNumber(1), _dbName(dbName) {}

  std::string specialize(char const* templ) {
    size_t len = strlen(templ);

    size_t pos = 0;
    std::string result;
    result.reserve(len);

    while (pos < len) {
      if (templ[pos] != '"') {
        result.push_back(templ[pos++]);
      } else {
        std::string st;
        pos = parseString(templ, pos, len, st);
        ReplacementCase c = whichCase(st);
        if (c != ReplacementCase::Not) {
          auto it = _replacements.find(st);
          if (it != _replacements.end()) {
            st = it->second;
          } else {
            std::string newSt;
            switch (c) {
              case ReplacementCase::Number:
                newSt = std::to_string(TRI_NewTickServer());
                break;
              case ReplacementCase::Shard:
                newSt = std::string("s") + std::to_string(TRI_NewTickServer());
                break;
              case ReplacementCase::DBServer:
                newSt = std::string("PRMR_000") + std::to_string(_nextServerNumber++);
                break;
              case ReplacementCase::DBName:
                newSt = _dbName;
                break;
              case ReplacementCase::Not:
                newSt = st;
                // never happens, just to please compilers
            }
            _replacements[st] = newSt;
            st = newSt;
          }
        }
        result.push_back('"');
        result.append(st);
        result.push_back('"');
      }
    }
    return result;
  }

 private:
  size_t parseString(char const* templ, size_t pos, size_t const len, std::string& st) {
    // This must be called when templ[pos] == '"'. It parses the string
    // and // puts it into st (not including the quotes around it).
    // The return value is pos advanced to behind the closing quote of
    // the string.
    ++pos;  // skip quotes
    size_t startPos = pos;
    while (pos < len && templ[pos] != '"') {
      ++pos;
    }
    // Now the string in question is between startPos and pos.
    // Extract string as it is:
    st = std::string(templ + startPos, pos - startPos);
    // Skip final quotes if they are there:
    if (pos < len && templ[pos] == '"') {
      ++pos;
    }
    return pos;
  }

  ReplacementCase whichCase(std::string& st) {
    bool onlyNumbers = true;
    size_t pos = 0;
    if (st == "db") {
      return ReplacementCase::DBName;
    }
    ReplacementCase c = ReplacementCase::Number;
    if (pos < st.size() && st[pos] == 's') {
      c = ReplacementCase::Shard;
      ++pos;
    } else if (pos + 5 <= st.size() && st.substr(0, 5) == "PRMR-") {
      return ReplacementCase::DBServer;
    }
    for (; pos < st.size(); ++pos) {
      if (st[pos] < '0' || st[pos] > '9') {
        onlyNumbers = false;
        break;
      }
    }
    if (!onlyNumbers) {
      return ReplacementCase::Not;
    }
    return c;
  }
};

}  // namespace

static void SetupGreetingsPhase(
    std::unordered_map<arangodb::application_features::ApplicationFeature*, bool>& features,
    arangodb::application_features::ApplicationServer& server) {
  features.emplace(new arangodb::application_features::GreetingsFeaturePhase(server, false),
                   false);
  // We do not need any features from this phase
}

static void SetupBasicFeaturePhase(
    std::unordered_map<arangodb::application_features::ApplicationFeature*, bool>& features,
    arangodb::application_features::ApplicationServer& server) {
  SetupGreetingsPhase(features, server);
  features.emplace(new arangodb::application_features::BasicFeaturePhase(server, false), false);
  features.emplace(new arangodb::ShardingFeature(server), false);
  features.emplace(new arangodb::DatabasePathFeature(server), false);
}

static void SetupDatabaseFeaturePhase(
    std::unordered_map<arangodb::application_features::ApplicationFeature*, bool>& features,
    arangodb::application_features::ApplicationServer& server) {
  SetupBasicFeaturePhase(features, server);
  features.emplace(new arangodb::application_features::DatabaseFeaturePhase(server),
                   false);  // true ??
  features.emplace(new arangodb::AuthenticationFeature(server), false);  // true ??
  features.emplace(arangodb::DatabaseFeature::DATABASE = new arangodb::DatabaseFeature(server),
                   false);
  features.emplace(new arangodb::SystemDatabaseFeature(server), true);
  features.emplace(new arangodb::InitDatabaseFeature(server, {}), true);
  features.emplace(new arangodb::ViewTypesFeature(server), false);  // true ??

#if USE_ENTERPRISE
  // required for AuthenticationFeature with USE_ENTERPRISE
  features.emplace(new arangodb::LdapFeature(server), false);
#endif
}

static void SetupClusterFeaturePhase(
    std::unordered_map<arangodb::application_features::ApplicationFeature*, bool>& features,
    arangodb::application_features::ApplicationServer& server) {
  SetupDatabaseFeaturePhase(features, server);
  features.emplace(new arangodb::application_features::ClusterFeaturePhase(server), false);
  features.emplace(new arangodb::ClusterFeature(server), false);
}

static void SetupCommunicationFeaturePhase(
    std::unordered_map<arangodb::application_features::ApplicationFeature*, bool>& features,
    arangodb::application_features::ApplicationServer& server) {
  SetupClusterFeaturePhase(features, server);
  features.emplace(new arangodb::application_features::CommunicationFeaturePhase(server), false);
  // This phase is empty...
}

static void SetupV8Phase(
    std::unordered_map<arangodb::application_features::ApplicationFeature*, bool>& features,
    arangodb::application_features::ApplicationServer& server) {
  SetupCommunicationFeaturePhase(features, server);
  features.emplace(new arangodb::application_features::V8FeaturePhase(server), false);
  features.emplace(new arangodb::V8DealerFeature(server), false);
}

static void SetupAqlPhase(
    std::unordered_map<arangodb::application_features::ApplicationFeature*, bool>& features,
    arangodb::application_features::ApplicationServer& server) {
  SetupV8Phase(features, server);
  features.emplace(new arangodb::application_features::AQLFeaturePhase(server), false);
  features.emplace(new arangodb::QueryRegistryFeature(server), false);

  features.emplace(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);
  features.emplace(new arangodb::iresearch::IResearchFeature(server), true);
  features.emplace(new arangodb::aql::AqlFunctionFeature(server), true);
  features.emplace(new arangodb::aql::OptimizerRulesFeature(server), true);
  features.emplace(new arangodb::AqlFeature(server), true);
  features.emplace(new arangodb::TraverserEngineRegistryFeature(server), false);
}

MockServer::MockServer()
    : _server(std::make_shared<arangodb::options::ProgramOptions>("", "", "", nullptr), nullptr),
      _engine(_server) {
  arangodb::EngineSelectorFeature::ENGINE = &_engine;
  init();
}

MockServer::~MockServer() {
  stopFeatures();
  ClusterCommResetter::reset();
  arangodb::application_features::ApplicationServer::setStateUnsafe(_oldApplicationServerState);
  arangodb::EngineSelectorFeature::ENGINE = nullptr;
  arangodb::application_features::ApplicationServer::server = nullptr;
  arangodb::AgencyCommManager::MANAGER.reset();
}

void MockServer::init() {
  _oldApplicationServerState = arangodb::application_features::ApplicationServer::server->state();

  arangodb::application_features::ApplicationServer::setStateUnsafe(
      arangodb::application_features::ApplicationServer::State::IN_WAIT);
  arangodb::transaction::Methods::clearDataSourceRegistrationCallbacks();
}

void MockServer::startFeatures() {
  for (auto& f : _features) {
    arangodb::application_features::ApplicationServer::server->addFeature(f.first);
  }

  arangodb::application_features::ApplicationServer::server->setupDependencies(false);
  auto orderedFeatures =
      arangodb::application_features::ApplicationServer::server->getOrderedFeatures();

  auto* sched =
    arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SchedulerFeature>(
      "Scheduler");
  if (sched != nullptr) {
    // Needed to set nrMaximalThreads
    sched->validateOptions(std::make_shared<arangodb::options::ProgramOptions>("", "", "", nullptr));
  }

  for (auto& f : orderedFeatures) {
    if (f->name() == "Endpoint") {
      // We need this feature to be there but do not use it.
      continue;
    }
    f->prepare();
  }

  auto* dbFeature =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
          "Database");
  if (dbFeature != nullptr) {
    // Only add a database if we have the feature.
    auto const databases = arangodb::velocypack::Parser::fromJson(
        R"([{"name": ")" + arangodb::StaticStrings::SystemDatabase + R"("}])");
    dbFeature->loadDatabases(databases->slice());
  }

  for (auto& f : orderedFeatures) {
    auto info = _features.find(f);
    // We only start those features...
    TRI_ASSERT(info != _features.end());
    if (info->second) {
      f->start();
    }
  }

  auto* dbPathFeature =
      arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>(
          "DatabasePath");
  if (dbPathFeature != nullptr) {
    // Inject a testFileSystemPath
    arangodb::tests::setDatabasePath(*dbPathFeature);  // ensure test data is stored in a unique directory
    _testFilesystemPath = dbPathFeature->directory();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(_testFilesystemPath.c_str(), systemError, systemErrorStr);
  }
}

void MockServer::stopFeatures() {
  if (!_testFilesystemPath.empty()) {
    TRI_RemoveDirectory(_testFilesystemPath.c_str());
  }
  auto orderedFeatures =
      arangodb::application_features::ApplicationServer::server->getOrderedFeatures();
  // destroy application features
  for (auto& f : orderedFeatures) {
    auto info = _features.find(f);
    // We only start those features...
    TRI_ASSERT(info != _features.end());
    if (info->second) {
      f->stop();
    }
  }

  for (auto& f : orderedFeatures) {
    f->unprepare();
  }
}

TRI_vocbase_t& MockServer::getSystemDatabase() const {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  TRI_ASSERT(database != nullptr);
  auto system = database->useDatabase(arangodb::StaticStrings::SystemDatabase);
  TRI_ASSERT(system != nullptr);
  return *system;
}

MockAqlServer::MockAqlServer() : MockServer() {
  // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
  arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                  arangodb::LogLevel::WARN);
  // suppress log messages since tests check error conditions
  arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR);  // suppress WARNING DefaultCustomTypeHandler called
  arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                  arangodb::LogLevel::FATAL);
  irs::logger::output_le(::iresearch::logger::IRL_FATAL, stderr);

  // setup required application features
  SetupAqlPhase(_features, _server);

  startFeatures();
}

MockAqlServer::~MockAqlServer() {
  arangodb::AqlFeature(_server).stop();  // unset singleton instance
  arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                  arangodb::LogLevel::DEFAULT);
  arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
  arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                  arangodb::LogLevel::DEFAULT);
}

std::shared_ptr<arangodb::transaction::Methods> MockAqlServer::createFakeTransaction() const {
  std::vector<std::string> noCollections{};
  transaction::Options opts;
  auto ctx = transaction::StandaloneContext::Create(getSystemDatabase());
  return std::make_shared<arangodb::transaction::Methods>(ctx, noCollections, noCollections,
                                                          noCollections, opts);
}

std::unique_ptr<arangodb::aql::Query> MockAqlServer::createFakeQuery() const {
  auto bindParams = std::make_shared<VPackBuilder>();
  bindParams->openObject();
  bindParams->close();
  auto queryOptions = std::make_shared<VPackBuilder>();
  queryOptions->openObject();
  queryOptions->close();
  aql::QueryString fakeQueryString("");
  auto query =
      std::make_unique<arangodb::aql::Query>(false, getSystemDatabase(),
                                             fakeQueryString, bindParams, queryOptions,
                                             arangodb::aql::QueryPart::PART_DEPENDENT);
  query->injectTransaction(createFakeTransaction());
  return query;
}

MockRestServer::MockRestServer() : MockServer() {
  // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
  arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                  arangodb::LogLevel::WARN);
  // suppress log messages since tests check error conditions
  arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR);  // suppress WARNING DefaultCustomTypeHandler called
  arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                  arangodb::LogLevel::FATAL);
  irs::logger::output_le(::iresearch::logger::IRL_FATAL, stderr);

  _features.emplace(new arangodb::QueryRegistryFeature(_server), false);

  SetupV8Phase(_features, _server);
  startFeatures();
}

MockRestServer::~MockRestServer() {
  arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                  arangodb::LogLevel::DEFAULT);
  arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
  arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                  arangodb::LogLevel::DEFAULT);
}

MockClusterServer::MockClusterServer()
    : MockServer(), _agencyStore(nullptr, "arango") {
  auto* agencyCommManager = new AgencyCommManagerMock("arango");
  std::ignore =
      agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore);
  std::ignore = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(
      _agencyStore);  // need 2 connections or Agency callbacks will fail
  arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);
  _oldRole = arangodb::ServerState::instance()->getRole();

  // Handle logging
  // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
  // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
  arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                  arangodb::LogLevel::ERR);

  // suppress INFO {cluster} Starting up with role PRIMARY
  arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::WARN);

  // suppress log messages since tests check error conditions
  arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(), arangodb::LogLevel::FATAL);
  arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                  arangodb::LogLevel::FATAL);
  irs::logger::output_le(::iresearch::logger::IRL_FATAL, stderr);

  // Add features
  SetupAqlPhase(_features, _server);

  _features.emplace(new arangodb::UpgradeFeature(_server, nullptr, {}), false);
  _features.emplace(new arangodb::ServerSecurityFeature(_server), false);
}

MockClusterServer::~MockClusterServer() {
  arangodb::ServerState::instance()->setRole(_oldRole);
}

void MockClusterServer::startFeatures() {
  MockServer::startFeatures();
  arangodb::AgencyCommManager::MANAGER->start();  // initialize agency
  arangodb::ServerState::instance()->setRebootId(1);

  // register factories & normalizers
  auto& indexFactory = const_cast<arangodb::IndexFactory&>(_engine.indexFactory());
  indexFactory.emplace(arangodb::iresearch::DATA_SOURCE_TYPE.name(),
                       arangodb::iresearch::IResearchLinkCoordinator::factory());
}

void MockClusterServer::agencyTrx(std::string const& key, std::string const& value) {
  // Build an agency transaction:
  VPackBuilder b;
  {
    VPackArrayBuilder guard(&b);
    {
      VPackObjectBuilder guard2(&b);
      auto b2 = VPackParser::fromJson(value);
      b.add(key, b2->slice());
    }
  }
  _agencyStore.applyTransaction(b.slice());
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
  agencyTrx("/arango/Plan/Version", R"=({"op":"increment"})=");
  agencyTrx("/arango/Plan/Current", R"=({"op":"increment"})=");
}

void MockClusterServer::agencyDropDatabase(std::string const& name) {
  std::string st = R"=({"op":"delete"}))=";
  agencyTrx("/arango/Plan/Databases/" + name, st);
  agencyTrx("/arango/Plan/Collections/" + name, st);
  agencyTrx("/arango/Current/Databases/" + name, st);
  agencyTrx("/arango/Current/Collections/" + name, st);
  agencyTrx("/arango/Plan/Version", R"=({"op":"increment"})=");
  agencyTrx("/arango/Plan/Current", R"=({"op":"increment"})=");
}

MockDBServer::MockDBServer() : MockClusterServer() {
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::RoleEnum::ROLE_DBSERVER);
  _features.emplace(new arangodb::FlushFeature(_server), false);  // do not start the thread
  _features.emplace(new arangodb::MaintenanceFeature(_server), false); // do not start the thread
  startFeatures();
  createDatabase("_system");
}

MockDBServer::~MockDBServer() {}

TRI_vocbase_t* MockDBServer::createDatabase(std::string const& name) {
  agencyCreateDatabase(name);
  auto* ci = ClusterInfo::instance();
  TRI_ASSERT(ci != nullptr);
  ci->loadPlan();
  ci->loadCurrent();

  // Now we must run a maintenance action to create the database locally,
  // unless it is the system database, in which case this does not work:
  if (name != "_system") {
    maintenance::ActionDescription ad(
        {{std::string(maintenance::NAME), std::string(maintenance::CREATE_DATABASE)},
         {std::string(maintenance::DATABASE), std::string(name)}},
         maintenance::HIGHER_PRIORITY);
    auto* mf = arangodb::application_features::ApplicationServer::lookupFeature<arangodb::MaintenanceFeature>("Maintenance");
    maintenance::CreateDatabase cd(*mf, ad);
    cd.first();   // Does the job
  }

  auto* databaseFeature = arangodb::DatabaseFeature::DATABASE;
  TRI_ASSERT(databaseFeature != nullptr);
  auto vocbase = databaseFeature->lookupDatabase(name);
  TRI_ASSERT(vocbase != nullptr);
  return vocbase;
};

void MockDBServer::dropDatabase(std::string const& name) {
  agencyDropDatabase(name);
  auto* ci = ClusterInfo::instance();
  TRI_ASSERT(ci != nullptr);
  ci->loadPlan();
  ci->loadCurrent();
  auto* databaseFeature = arangodb::DatabaseFeature::DATABASE;
  TRI_ASSERT(databaseFeature != nullptr);
  auto vocbase = databaseFeature->lookupDatabase(name);
  TRI_ASSERT(vocbase == nullptr);

  // Now we must run a maintenance action to create the database locally:
  maintenance::ActionDescription ad(
      {{std::string(maintenance::NAME), std::string(maintenance::DROP_DATABASE)},
       {std::string(maintenance::DATABASE), std::string(name)}},
       maintenance::HIGHER_PRIORITY);
  auto* mf = arangodb::application_features::ApplicationServer::lookupFeature<arangodb::MaintenanceFeature>("Maintenance");
  maintenance::DropDatabase dd(*mf, ad);
  dd.first();   // Does the job
}

MockCoordinator::MockCoordinator() : MockClusterServer() {
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::RoleEnum::ROLE_COORDINATOR);
  startFeatures();
  createDatabase("_system");
}

MockCoordinator::~MockCoordinator() {}

TRI_vocbase_t* MockCoordinator::createDatabase(std::string const& name) {
  agencyCreateDatabase(name);
  auto* ci = ClusterInfo::instance();
  TRI_ASSERT(ci != nullptr);
  ci->loadPlan();
  ci->loadCurrent();
  auto* databaseFeature = arangodb::DatabaseFeature::DATABASE;
  TRI_ASSERT(databaseFeature != nullptr);
  auto vocbase = databaseFeature->lookupDatabase(name);
  TRI_ASSERT(vocbase != nullptr);
  return vocbase;
};

void MockCoordinator::dropDatabase(std::string const& name) {
  agencyDropDatabase(name);
  auto* ci = ClusterInfo::instance();
  TRI_ASSERT(ci != nullptr);
  ci->loadPlan();
  ci->loadCurrent();
  auto* databaseFeature = arangodb::DatabaseFeature::DATABASE;
  TRI_ASSERT(databaseFeature != nullptr);
  auto vocbase = databaseFeature->lookupDatabase(name);
  TRI_ASSERT(vocbase == nullptr);
}

