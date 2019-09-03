////////////////////////////////////////////////////////////////////////////////
/// @brief setup for transactions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef ARANGODB_MOCK_TRX_MANAGER
#define ARANGODB_MOCK_TRX_MANAGER

#include "Aql/OptimizerRulesFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/ManagerFeature.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "../Mocks/StorageEngineMock.h"

namespace arangodb {
namespace tests {
namespace mocks {
  
  // -----------------------------------------------------------------------------
  // --SECTION--                                                 setup / tear-down
  // -----------------------------------------------------------------------------
  
  struct TransactionManagerSetup {
    StorageEngineMock engine;
    arangodb::application_features::ApplicationServer server;
    std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;

    TransactionManagerSetup(): engine(server), server(nullptr, nullptr) {
      arangodb::EngineSelectorFeature::ENGINE = &engine;
      
      // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
      arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);
      
      // setup required application features
      server.addFeature<arangodb::DatabaseFeature>(
          std::make_unique<arangodb::DatabaseFeature>(server));
      features.emplace_back(server.getFeature<arangodb::DatabaseFeature>(), false);  // required for TRI_vocbase_t::dropCollection(...)

      server.addFeature<arangodb::ShardingFeature>(
          std::make_unique<arangodb::ShardingFeature>(server));
      features.emplace_back(server.getFeature<arangodb::ShardingFeature>(), false);

      server.addFeature<transaction::ManagerFeature>(
          std::make_unique<transaction::ManagerFeature>(server));
      features.emplace_back(server.getFeature<transaction::ManagerFeature>(), true);

      server.addFeature<arangodb::QueryRegistryFeature>(
          std::make_unique<arangodb::QueryRegistryFeature>(server));
      features.emplace_back(server.getFeature<arangodb::QueryRegistryFeature>(), false);  // must be first

      server.addFeature<arangodb::TraverserEngineRegistryFeature>(
          std::make_unique<arangodb::TraverserEngineRegistryFeature>(server));
      features.emplace_back(server.getFeature<arangodb::TraverserEngineRegistryFeature>(),
                            false);  // must be before AqlFeature

      server.addFeature<arangodb::AqlFeature>(std::make_unique<arangodb::AqlFeature>(server));
      features.emplace_back(server.getFeature<arangodb::AqlFeature>(), true);

      server.addFeature<arangodb::aql::OptimizerRulesFeature>(
          std::make_unique<arangodb::aql::OptimizerRulesFeature>(server));
      features.emplace_back(server.getFeature<arangodb::aql::OptimizerRulesFeature>(), true);

      server.addFeature<arangodb::AuthenticationFeature>(
          std::make_unique<arangodb::AuthenticationFeature>(server));
      features.emplace_back(server.getFeature<arangodb::AuthenticationFeature>(), true);

#if USE_ENTERPRISE
      server.addFeature<arangodb::LdapFeature>(
          std::make_unique<arangodb::LdapFeature>(server));
      features.emplace_back(server.getFeature<arangodb::LdapFeature>(), false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

      for (auto& f: features) {
        f.first.prepare();
      }
      
      for (auto& f: features) {
        if (f.second) {
          f.first.start();
        }
      }
    }
    
    ~TransactionManagerSetup() {
      arangodb::EngineSelectorFeature::ENGINE = nullptr;
      
      // destroy application features
      for (auto& f: features) {
        if (f.second) {
          f.first.stop();
        }
      }
      
      for (auto& f: features) {
        f.first.unprepare();
      }
    }
  };

} // namespace tests
} // namespace mocks
} // namespace arangodb
#endif
