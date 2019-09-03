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
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/vocbase.h"
#include "utils/log.hpp"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::mocks;

MockServer::MockServer() : _server(nullptr, nullptr), _engine(_server) {
  arangodb::EngineSelectorFeature::ENGINE = &_engine;
  init();
}

MockServer::~MockServer() {
  _system.reset();  // destroy before reseting the 'ENGINE'
  arangodb::EngineSelectorFeature::ENGINE = nullptr;
  stopFeatures();
}

void MockServer::init() {
  arangodb::transaction::Methods::clearDataSourceRegistrationCallbacks();
}

void MockServer::startFeatures() {
  for (auto& f : _features) {
    f.first.prepare();
  }

  for (auto& f : _features) {
    if (f.second) {
      f.first.start();
    }
  }
}

void MockServer::stopFeatures() {
  // destroy application _features
  for (auto& f : _features) {
    if (f.second) {
      f.first.stop();
    }
  }

  for (auto& f : _features) {
    f.first.unprepare();
  }
}

TRI_vocbase_t& MockServer::getSystemDatabase() const {
  TRI_ASSERT(_system != nullptr);
  return *(_system.get());
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

  // setup required application _features
  _server.addFeature<arangodb::ViewTypesFeature>(
      std::make_unique<arangodb::ViewTypesFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::ViewTypesFeature>(), true);

  _server.addFeature<arangodb::AuthenticationFeature>(
      std::make_unique<arangodb::AuthenticationFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::AuthenticationFeature>(), true);

  _server.addFeature<arangodb::DatabasePathFeature>(
      std::make_unique<arangodb::DatabasePathFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::DatabasePathFeature>(), false);

  _server.addFeature<arangodb::DatabaseFeature>(
      std::make_unique<arangodb::DatabaseFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::DatabaseFeature>(), false);

  _server.addFeature<arangodb::ShardingFeature>(
      std::make_unique<arangodb::ShardingFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::ShardingFeature>(), true);

  _server.addFeature<arangodb::QueryRegistryFeature>(
      std::make_unique<arangodb::QueryRegistryFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::QueryRegistryFeature>(), false);  // must be first

  _system = std::make_unique<TRI_vocbase_t>(_server, TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                            0, TRI_VOC_SYSTEM_DATABASE);
  _server.addFeature<arangodb::SystemDatabaseFeature>(
      std::make_unique<arangodb::SystemDatabaseFeature>(_server, _system.get()));
  _features.emplace_back(_server.getFeature<arangodb::SystemDatabaseFeature>(),
                         false);  // required for IResearchAnalyzerFeature

  _server.addFeature<arangodb::TraverserEngineRegistryFeature>(
      std::make_unique<arangodb::TraverserEngineRegistryFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::TraverserEngineRegistryFeature>(),
                         false);  // must be before AqlFeature

  _server.addFeature<arangodb::AqlFeature>(std::make_unique<arangodb::AqlFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::AqlFeature>(), true);

  _server.addFeature<arangodb::aql::OptimizerRulesFeature>(
      std::make_unique<arangodb::aql::OptimizerRulesFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::aql::OptimizerRulesFeature>(), true);

  _server.addFeature<arangodb::aql::AqlFunctionFeature>(
      std::make_unique<arangodb::aql::AqlFunctionFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::aql::AqlFunctionFeature>(),
                         true);  // required for IResearchAnalyzerFeature

  _server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>(
      std::make_unique<arangodb::iresearch::IResearchAnalyzerFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>(),
                         true);

  _server.addFeature<arangodb::iresearch::IResearchFeature>(
      std::make_unique<arangodb::iresearch::IResearchFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::iresearch::IResearchFeature>(), true);

#if USE_ENTERPRISE
  _server.addFeature<arangodb::LdapFeature>(std::make_unique<arangodb::LdapFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::LdapFeature>(), false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

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

  _server.addFeature<arangodb::AuthenticationFeature>(
      std::make_unique<arangodb::AuthenticationFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::AuthenticationFeature>(), false);

  _server.addFeature<arangodb::DatabaseFeature>(
      std::make_unique<arangodb::DatabaseFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::DatabaseFeature>(), false);

  _server.addFeature<arangodb::QueryRegistryFeature>(
      std::make_unique<arangodb::QueryRegistryFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::QueryRegistryFeature>(), false);

  _system = std::make_unique<TRI_vocbase_t>(_server, TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                            0, TRI_VOC_SYSTEM_DATABASE);
  _server.addFeature<arangodb::SystemDatabaseFeature>(
      std::make_unique<arangodb::SystemDatabaseFeature>(_server, _system.get()));
  _features.emplace_back(_server.getFeature<arangodb::SystemDatabaseFeature>(), false);

#if USE_ENTERPRISE
  _server.addFeature<arangodb::LdapFeature>(std::make_unique<arangodb::LdapFeature>(_server));
  _features.emplace_back(_server.getFeature<arangodb::LdapFeature>(), false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

  startFeatures();
}

MockRestServer::~MockRestServer() {
  arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                  arangodb::LogLevel::DEFAULT);
  arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
  arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                  arangodb::LogLevel::DEFAULT);
}

MockEmptyServer::MockEmptyServer() {
  startFeatures();
}

MockEmptyServer::~MockEmptyServer() = default;
