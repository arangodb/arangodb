////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for transaction MAanger
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/SmartContext.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"

#include "../Mocks/StorageEngineMock.h"

#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "catch.hpp"

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct TransactionManagerSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  
  TransactionManagerSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    
    // setup required application features
    features.emplace_back(new arangodb::DatabaseFeature(server), false); // required for TRI_vocbase_t::dropCollection(...)
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required for TRI_vocbase_t instantiation
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new transaction::ManagerFeature(server), true);
    
    for (auto& f: features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }
    
    for (auto& f: features) {
      f.first->prepare();
    }
    
    for (auto& f: features) {
      if (f.second) {
        f.first->start();
      }
    }
  }
  
  ~TransactionManagerSetup() {
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
    
    // destroy application features
    for (auto& f: features) {
      if (f.second) {
        f.first->stop();
      }
    }
    
    for (auto& f: features) {
      f.first->unprepare();
    }
  }
};


// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/// @brief test transaction::Manager
TEST_CASE("TransactionManagerTest", "[transaction]") {
  TransactionManagerSetup setup;
  TRI_ASSERT(transaction::ManagerFeature::manager());
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  
  auto* mgr = transaction::ManagerFeature::manager();
  REQUIRE(mgr != nullptr);
  TRI_voc_tid_t tid = TRI_NewTickServer();

  
  SECTION("Parsing errors") {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"write\": [33] }");
    Result res = mgr->createManagedTrx(vocbase, tid, json->slice());
    CHECK(res.is(TRI_ERROR_BAD_PARAMETER));
    
    json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"write\": \"33\"}, \"lockTimeout\": -1 }");
    res = mgr->createManagedTrx(vocbase, tid, json->slice());
    CHECK(res.is(TRI_ERROR_BAD_PARAMETER));
  }
  
  SECTION("Collection Not found") {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"read\": [\"33\"]}}");
    Result res = mgr->createManagedTrx(vocbase, tid, json->slice());
    CHECK(res.errorNumber() == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    
    json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"write\": [\"33\"]}}");
    res = mgr->createManagedTrx(vocbase, tid, json->slice());
    CHECK(res.errorNumber() == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    
    json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"exclusive\": [\"33\"]}}");
    res = mgr->createManagedTrx(vocbase, tid, json->slice());
    CHECK(res.errorNumber() == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
  
  std::shared_ptr<LogicalCollection> collection;
  {
    auto json = VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    collection = vocbase.createCollection(json->slice());
  }
  REQUIRE(collection != nullptr);
  
  SECTION("Transaction ID reuse") {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"read\": [\"42\"]}}");
    Result res = mgr->createManagedTrx(vocbase, tid, json->slice());
    REQUIRE(res.ok());
    
    json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"write\": [\"33\"]}}");
    res = mgr->createManagedTrx(vocbase, tid, json->slice());
    CHECK(res.errorNumber() == TRI_ERROR_TRANSACTION_INTERNAL);
    
    res = mgr->abortManagedTrx(tid);
    REQUIRE(res.ok());
  }

//  SECTION("Permission denied") {
//    ExecContext exe(ExecContext::Type, "dummy",
//                    vocbase.name(), auth::Level::NONE, auth::Level::NONE);
//    ExecContextScope scope();
//
//    auto json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"read\": [\"42\"]}}");
//    Result res = mgr->createManagedTrx(vocbase, tid, json->slice());
//    REQUIRE(res.ok());
//
//    json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"write\": [\"33\"]}}");
//    res = mgr->createManagedTrx(vocbase, tid, json->slice());
//    REQUIRE(res.errorNumber() == TRI_ERROR_TRANSACTION_INTERNAL);
//  }
//
//  SECTION("Acquire transaction") {
//    transaction::ManagedContext ctx();
//
//  }
  
}
