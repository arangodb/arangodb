////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for transaction Manager
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

#include "Aql/Query.h"
#include "Aql/OptimizerRulesFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/SmartContext.h"
#include "Transaction/Status.h"
#include "Utils/ExecContext.h"
#include "Utils/SingleCollectionTransaction.h"
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
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new transaction::ManagerFeature(server), true);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(server), true);
    
    
//#ifdef USE_ENTERPRISE
//    features.emplace_back(new arangodb::LdapFeature(server), false);
//#endif
//    features.emplace_back(new arangodb::ShardingFeature(server), false); //
//    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
//    features.emplace_back(new arangodb::AuthenticationFeature(server), true); // required for FeatureCacheFeature
//    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
//    features.emplace_back(new arangodb::DatabaseFeature(server), false); // required for FeatureCacheFeature
//    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // must be first
//    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
//    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
//    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
//    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false); // must be before AqlFeature
//       features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
//    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);
//    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), true);

    
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

arangodb::aql::QueryResult executeQuery(TRI_vocbase_t& vocbase,
                                        std::string const& queryString,
                                        std::shared_ptr<transaction::Context> ctx) {
  auto options = std::make_shared<VPackBuilder>();
  options->openObject();
  options->close();
  std::shared_ptr<arangodb::velocypack::Builder> bindVars;
  
  arangodb::aql::Query query(false,
                             vocbase,
                             arangodb::aql::QueryString(queryString),
                             bindVars,
                             options,
                             arangodb::aql::PART_MAIN);
  query.setTransactionContext(std::move(ctx));
  
  std::shared_ptr<arangodb::aql::SharedQueryState> ss = query.sharedState();
  arangodb::aql::QueryResult result;
  while (true) {
    auto state = query.execute(arangodb::QueryRegistryFeature::registry(), result);
    if (state == arangodb::aql::ExecutionState::WAITING) {
      ss->waitForAsyncResponse();
    } else {
      break;
    }
  }
  
  return result;
}


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
  scopeGuard([&] {
    mgr->garbageCollect(true);
  });
  
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
  
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json = VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  REQUIRE(coll != nullptr);
  
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
  
  SECTION("Simple Transaction & Abort") {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"write\": [\"42\"]}}");
    Result res = mgr->createManagedTrx(vocbase, tid, json->slice());
    REQUIRE(res.ok());
    
    auto doc = arangodb::velocypack::Parser::fromJson("{ \"_key\": \"1\"}");
    {
      auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
      REQUIRE(ctx.get() != nullptr);

      SingleCollectionTransaction trx(ctx, "testCollection", AccessMode::Type::WRITE);
      REQUIRE(trx.state()->isEmbeddedTransaction());
      
      OperationOptions opts;
      auto opRes = trx.insert(coll->name(), doc->slice(), opts);
      REQUIRE(opRes.ok());
      REQUIRE(trx.finish(opRes.result).ok());
    }
    
    REQUIRE((mgr->getManagedTrxStatus(tid) == transaction::Status::RUNNING));
    
    { // lease again
      auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
      REQUIRE(ctx.get() != nullptr);

      SingleCollectionTransaction trx(ctx, "testCollection", AccessMode::Type::READ);
      REQUIRE(trx.state()->isEmbeddedTransaction());
      
      OperationOptions opts;
      auto opRes = trx.document(coll->name(), doc->slice(), opts);
      REQUIRE(opRes.ok());
      REQUIRE(trx.finish(opRes.result).ok());
    }
    REQUIRE((mgr->getManagedTrxStatus(tid) == transaction::Status::RUNNING));
    REQUIRE(mgr->abortManagedTrx(tid).ok());
    // perform same operation
    REQUIRE(mgr->abortManagedTrx(tid).ok());
    // cannot commit aborted transaction
    REQUIRE(mgr->commitManagedTrx(tid).is(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION));
    
    REQUIRE((mgr->getManagedTrxStatus(tid) == transaction::Status::ABORTED));
  }

  
  SECTION("Simple Transaction & Commit") {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"write\": [\"42\"]}}");
    Result res = mgr->createManagedTrx(vocbase, tid, json->slice());
    REQUIRE(res.ok());
    
    {
      auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
      REQUIRE(ctx.get() != nullptr);

      SingleCollectionTransaction trx(ctx, "testCollection", AccessMode::Type::WRITE);
      REQUIRE(trx.state()->isEmbeddedTransaction());
      
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": 1}");
      
      OperationOptions opts;
      auto opRes = trx.insert(coll->name(), doc->slice(), opts);
      REQUIRE(opRes.ok());
      REQUIRE(trx.finish(opRes.result).ok());
    }
    REQUIRE((mgr->getManagedTrxStatus(tid) == transaction::Status::RUNNING));
    REQUIRE(mgr->commitManagedTrx(tid).ok());
    // perform same operation
    REQUIRE(mgr->commitManagedTrx(tid).ok());
    // cannot commit aborted transaction
    REQUIRE(mgr->abortManagedTrx(tid).is(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION));
    
    REQUIRE((mgr->getManagedTrxStatus(tid) == transaction::Status::COMMITTED));
  }
  
  
  SECTION("Simple Transaction & Commit while in use") {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"write\": [\"42\"]}}");
    Result res = mgr->createManagedTrx(vocbase, tid, json->slice());
    REQUIRE(res.ok());
    
    {
      auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
      REQUIRE(ctx.get() != nullptr);
      
      SingleCollectionTransaction trx(ctx, "testCollection", AccessMode::Type::WRITE);
      REQUIRE(trx.state()->isEmbeddedTransaction());
      
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": 1}");
      
      OperationOptions opts;
      auto opRes = trx.insert(coll->name(), doc->slice(), opts);
      REQUIRE(opRes.ok());
      REQUIRE(mgr->commitManagedTrx(tid).is(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION));
      REQUIRE(trx.finish(opRes.result).ok());
    }
    REQUIRE((mgr->getManagedTrxStatus(tid) == transaction::Status::RUNNING));

    REQUIRE(mgr->commitManagedTrx(tid).ok());
    // perform same operation
    REQUIRE(mgr->commitManagedTrx(tid).ok());
    // cannot abort committed transaction
    REQUIRE(mgr->abortManagedTrx(tid).is(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION));
    REQUIRE((mgr->getManagedTrxStatus(tid) == transaction::Status::COMMITTED));
  }
  
  SECTION("Leasing multiple read-only transactions") {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"read\": [\"42\"]}}");
    Result res = mgr->createManagedTrx(vocbase, tid, json->slice());
    REQUIRE(res.ok());
    
    {
      auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::READ);
      REQUIRE(ctx.get() != nullptr);
      REQUIRE(ctx->getParentTransaction() != nullptr);
      
      auto ctx2 = mgr->leaseManagedTrx(tid, AccessMode::Type::READ);
      REQUIRE(ctx2.get() != nullptr);
      CHECK(ctx->getParentTransaction() == ctx2->getParentTransaction());
      
      auto ctx3 = mgr->leaseManagedTrx(tid, AccessMode::Type::READ);
      REQUIRE(ctx3.get() != nullptr);
      CHECK(ctx->getParentTransaction() == ctx3->getParentTransaction());
    }
    REQUIRE(mgr->abortManagedTrx(tid).ok());
    REQUIRE((mgr->getManagedTrxStatus(tid) == transaction::Status::ABORTED));
  }

  SECTION("Lock conflict") {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"write\": [\"42\"]}}");
    Result res = mgr->createManagedTrx(vocbase, tid, json->slice());
    REQUIRE(res.ok());
    {
      auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
      REQUIRE(ctx.get() != nullptr);
      REQUIRE(ctx->getParentTransaction() != nullptr);
      REQUIRE_THROWS(mgr->leaseManagedTrx(tid, AccessMode::Type::READ));
    }
    REQUIRE(mgr->abortManagedTrx(tid).ok());
    REQUIRE((mgr->getManagedTrxStatus(tid) == transaction::Status::ABORTED));
  }
  
  SECTION("Garbage Collection shutdown") {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"write\": [\"42\"]}}");
    Result res = mgr->createManagedTrx(vocbase, tid, json->slice());
    REQUIRE(res.ok());
    {
      auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
      REQUIRE(ctx.get() != nullptr);
      REQUIRE(ctx->getParentTransaction() != nullptr);
    }
    REQUIRE((mgr->getManagedTrxStatus(tid) == transaction::Status::RUNNING));
    REQUIRE(mgr->garbageCollect(/*abortAll*/true));
    REQUIRE((mgr->getManagedTrxStatus(tid) == transaction::Status::ABORTED));
  }
  
//  SECTION("Garbage Collection normal") {
//    auto json = arangodb::velocypack::Parser::fromJson("{ \"collections\":{\"write\": [\"42\"]}}");
//    Result res = mgr->createManagedTrx(vocbase, tid, json->slice());
//    REQUIRE(res.ok());
//    {
//      auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
//      REQUIRE(ctx.get() != nullptr);
//      REQUIRE(ctx->getParentTransaction() != nullptr);
//    }
//    REQUIRE(mgr->getManagedTrxStatus(tid) == transaction::Status::RUNNING);
//    REQUIRE(mgr->garbageCollect(/*abortAll*/false));
//    REQUIRE(mgr->getManagedTrxStatus(tid) == transaction::Status::ABORTED);
//  }

  SECTION("AQL standalone transaction") {
    {
      auto ctx = transaction::StandaloneContext::Create(vocbase);
      SingleCollectionTransaction trx(ctx, "testCollection", AccessMode::Type::WRITE);
      REQUIRE(trx.begin().ok());
      
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": 1}");
      OperationOptions opts;
      auto opRes = trx.insert(coll->name(), doc->slice(), opts);
      REQUIRE(opRes.ok());
      REQUIRE(trx.finish(opRes.result).ok());
    }
    
    auto ctx = std::make_shared<transaction::AQLStandaloneContext>(vocbase, tid);
    auto qq = "FOR doc IN testCollection RETURN doc";
    arangodb::aql::QueryResult qres = executeQuery(vocbase, qq, ctx);
    REQUIRE(qres.ok());
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
