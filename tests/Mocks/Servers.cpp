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
#include "ApplicationFeatures/ApplicationFeature.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Sharding/ShardingFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/vocbase.h"
#include "utils/log.hpp"

#include "../IResearch/common.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::mocks;

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();

MockServer::MockServer() : _server(nullptr, nullptr), _engine(_server) {
  arangodb::EngineSelectorFeature::ENGINE = &_engine;
  init();
}

MockServer::~MockServer() {
  _system.reset(); // destroy before reseting the 'ENGINE'
  arangodb::application_features::ApplicationServer::server = nullptr;
  arangodb::EngineSelectorFeature::ENGINE = nullptr;
  stopFeatures();
}

void MockServer::init() {
  arangodb::transaction::Methods::clearDataSourceRegistrationCallbacks();
}

void MockServer::startFeatures() {
  for (auto& f : _features) {
    arangodb::application_features::ApplicationServer::server->addFeature(f.first);
  }

  for (auto& f : _features) {
    f.first->prepare();
  }

  for (auto& f : _features) {
    if (f.second) {
      f.first->start();
    }
  }
}

void MockServer::stopFeatures() {
  // destroy application features
  for (auto& f : _features) {
    if (f.second) {
      f.first->stop();
    }
  }

  for (auto& f : _features) {
    f.first->unprepare();
  }
}

TRI_vocbase_t& MockServer::getSystemDatabase() const {
  TRI_ASSERT(_system != nullptr);
  return *(_system.get());
}

MockAqlServer::MockAqlServer() : MockServer() {
  // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
  arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);
  // suppress log messages since tests check error conditions
  arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR); // suppress WARNING DefaultCustomTypeHandler called
  arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
  irs::logger::output_le(::iresearch::logger::IRL_FATAL, stderr);

  // setup required application features
  _features.emplace_back(new arangodb::ViewTypesFeature(_server), true);
  _features.emplace_back(new arangodb::AuthenticationFeature(_server), true);
  _features.emplace_back(new arangodb::DatabasePathFeature(_server), false);
  _features.emplace_back(new arangodb::DatabaseFeature(_server), false);
  _features.emplace_back(new arangodb::ShardingFeature(_server), true);
  _features.emplace_back(new arangodb::QueryRegistryFeature(_server), false); // must be first
  arangodb::application_features::ApplicationServer::server->addFeature(_features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
  _system = std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, systemDatabaseArgs);
  _features.emplace_back(new arangodb::SystemDatabaseFeature(_server, _system.get()), false); // required for IResearchAnalyzerFeature
  _features.emplace_back(new arangodb::TraverserEngineRegistryFeature(_server), false); // must be before AqlFeature
  _features.emplace_back(new arangodb::AqlFeature(_server), true);
  _features.emplace_back(new arangodb::aql::OptimizerRulesFeature(_server), true);
  _features.emplace_back(new arangodb::aql::AqlFunctionFeature(_server), true); // required for IResearchAnalyzerFeature
  _features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(_server), true);
  _features.emplace_back(new arangodb::iresearch::IResearchFeature(_server), true);

  #if USE_ENTERPRISE
    _features.emplace_back(new arangodb::LdapFeature(_server), false); // required for AuthenticationFeature with USE_ENTERPRISE
  #endif

  startFeatures();
}

MockAqlServer::~MockAqlServer() {
  arangodb::AqlFeature(_server).stop(); // unset singleton instance
  arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
  arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
  arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
}

std::shared_ptr<arangodb::transaction::Methods> MockAqlServer::createFakeTransaction() const {
  std::vector<std::string> noCollections{};
  transaction::Options opts;
  auto ctx = transaction::StandaloneContext::Create(getSystemDatabase());
  return std::make_shared<arangodb::transaction::Methods>(ctx,
                                                          noCollections, noCollections,
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
  auto query = std::make_unique<arangodb::aql::Query>(
      false, getSystemDatabase(), fakeQueryString, bindParams, queryOptions,
      arangodb::aql::QueryPart::PART_DEPENDENT);
  query->injectTransaction(createFakeTransaction());
  return query;
}
