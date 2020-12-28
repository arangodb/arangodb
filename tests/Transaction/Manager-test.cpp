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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Query.h"

#include "Basics/ScopeGuard.h"
#include "Cluster/ServerState.h"
#include "Rest/GeneralResponse.h"
#include "Transaction/Manager.h"
#include "Transaction/SmartContext.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/Status.h"
#include "Utils/ExecContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "gtest/gtest.h"

#include "ManagerSetup.h"
#include "../IResearch/common.h"

using namespace arangodb;

static arangodb::aql::QueryResult executeQuery(TRI_vocbase_t& vocbase,
                                               std::string const& queryString,
                                               std::shared_ptr<transaction::Context> ctx) {
  arangodb::aql::Query query(ctx, arangodb::aql::QueryString(queryString), nullptr);

  arangodb::aql::QueryResult result;
  while (true) {
    auto state = query.execute(result);
    if (state == arangodb::aql::ExecutionState::WAITING) {
      query.sharedState()->waitForAsyncWakeup();
    } else {
      break;
    }
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class TransactionManagerTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::TransactionManagerSetup setup;
  TRI_vocbase_t vocbase;
  transaction::Manager* mgr;
  TransactionId tid;

  TransactionManagerTest()
      : vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(setup.server.server())),
        mgr(transaction::ManagerFeature::manager()),
        tid(TRI_NewTickServer()) {}

  ~TransactionManagerTest() { mgr->garbageCollect(true); }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(TransactionManagerTest, parsing_errors) {
  auto json = arangodb::velocypack::Parser::fromJson("{ \"write\": [33] }");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  EXPECT_TRUE(res.is(TRI_ERROR_BAD_PARAMETER));

  json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": \"33\"}, \"lockTimeout\": -1 }");
  res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  EXPECT_TRUE(res.is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(TransactionManagerTest, collection_not_found) {
  arangodb::ExecContextSuperuserScope exeScope;

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"read\": [\"33\"]}}");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"33\"]}}");
  res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"exclusive\": [\"33\"]}}");
  res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

TEST_F(TransactionManagerTest, transaction_id_reuse) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"read\": [\"42\"]}}");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  ASSERT_TRUE(res.ok());

  json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"33\"]}}");
  res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_TRANSACTION_INTERNAL);

  res = mgr->abortManagedTrx(tid, vocbase.name());
  ASSERT_TRUE(res.ok());
}

TEST_F(TransactionManagerTest, simple_transaction_and_abort) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  ASSERT_TRUE(res.ok());

  auto doc = arangodb::velocypack::Parser::fromJson("{ \"_key\": \"1\"}");
  {
    auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
    ASSERT_NE(ctx.get(), nullptr);

    SingleCollectionTransaction trx(ctx, "testCollection", AccessMode::Type::WRITE);
    ASSERT_TRUE(!trx.isMainTransaction());

    OperationOptions opts;
    auto opRes = trx.insert(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }

  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::RUNNING);

  {  // lease again
    auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
    ASSERT_NE(ctx.get(), nullptr);

    SingleCollectionTransaction trx(ctx, "testCollection", AccessMode::Type::READ);
    ASSERT_TRUE(!trx.isMainTransaction());

    OperationOptions opts;
    auto opRes = trx.document(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }
  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::RUNNING);
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).ok());
  // perform same operation
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).ok());
  // cannot commit aborted transaction
  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).is(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION));

  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::ABORTED);
}

TEST_F(TransactionManagerTest, simple_transaction_and_commit) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  ASSERT_TRUE(res.ok());

  {
    auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
    ASSERT_NE(ctx.get(), nullptr);

    SingleCollectionTransaction trx(ctx, "testCollection", AccessMode::Type::WRITE);
    ASSERT_TRUE(!trx.isMainTransaction());
    ASSERT_FALSE(trx.state()->hasHint(transaction::Hints::Hint::IS_FOLLOWER_TRX));

    auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": 1}");

    OperationOptions opts;
    auto opRes = trx.insert(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }
  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::RUNNING);
  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).ok());
  // perform same operation
  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).ok());
  // cannot commit aborted transaction
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).is(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION));

  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::COMMITTED);
}

TEST_F(TransactionManagerTest, simple_transaction_and_commit_is_follower) {
  auto beforeRole = arangodb::ServerState::instance()->getRole();
  auto roleGuard = scopeGuard([&]() {
    arangodb::ServerState::instance()->setRole(beforeRole);
  });
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), true);
  ASSERT_TRUE(res.ok());

  {
    auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
    ASSERT_NE(ctx.get(), nullptr);

    SingleCollectionTransaction trx(ctx, "testCollection", AccessMode::Type::WRITE);
    ASSERT_TRUE(!trx.isMainTransaction());
    ASSERT_TRUE(trx.state()->hasHint(transaction::Hints::Hint::IS_FOLLOWER_TRX));

    auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": 1}");

    OperationOptions opts;
    auto opRes = trx.insert(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }
  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::RUNNING);
  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).ok());
  // perform same operation
  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).ok());
  // cannot commit aborted transaction
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).is(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION));

  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::COMMITTED);
}

TEST_F(TransactionManagerTest, simple_transaction_and_commit_while_in_use) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  ASSERT_TRUE(res.ok());

  {
    auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
    ASSERT_NE(ctx.get(), nullptr);

    SingleCollectionTransaction trx(ctx, "testCollection", AccessMode::Type::WRITE);
    ASSERT_TRUE(!trx.isMainTransaction());

    auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": 1}");

    OperationOptions opts;
    auto opRes = trx.insert(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_EQ(TRI_ERROR_LOCKED, mgr->commitManagedTrx(tid, vocbase.name()).errorNumber());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }
  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::RUNNING);

  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).ok());
  // perform same operation
  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).ok());
  // cannot abort committed transaction
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).is(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION));
  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::COMMITTED);
}

TEST_F(TransactionManagerTest, leading_multiple_readonly_transactions) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"read\": [\"42\"]}}");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  ASSERT_TRUE(res.ok());

  {
    transaction::Options opts;
    bool responsible;

    auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::READ);
    ASSERT_NE(ctx.get(), nullptr);
    auto state1 = ctx->acquireState(opts, responsible);
    ASSERT_NE(state1.get(), nullptr);
    ASSERT_TRUE(!responsible);

    auto ctx2 = mgr->leaseManagedTrx(tid, AccessMode::Type::READ);
    ASSERT_NE(ctx2.get(), nullptr);
    auto state2 = ctx2->acquireState(opts, responsible);
    EXPECT_EQ(state1.get(), state2.get());
    ASSERT_TRUE(!responsible);

    auto ctx3 = mgr->leaseManagedTrx(tid, AccessMode::Type::READ);
    ASSERT_NE(ctx3.get(), nullptr);
    auto state3 = ctx3->acquireState(opts, responsible);
    EXPECT_EQ(state3.get(), state2.get());
    ASSERT_TRUE(!responsible);
  }
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).ok());
  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::ABORTED);
}

TEST_F(TransactionManagerTest, lock_conflict) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  ASSERT_TRUE(res.ok());
  {
    transaction::Options opts;
    bool responsible;

    auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
    ASSERT_NE(ctx.get(), nullptr);
    auto state1 = ctx->acquireState(opts, responsible);
    ASSERT_NE(state1.get(), nullptr);
    ASSERT_TRUE(!responsible);
    ASSERT_ANY_THROW(mgr->leaseManagedTrx(tid, AccessMode::Type::READ));
  }
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).ok());
  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::ABORTED);
}

TEST_F(TransactionManagerTest, garbage_collection_shutdown) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  ASSERT_TRUE(res.ok());
  {
    transaction::Options opts;
    bool responsible;
    
    auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
    ASSERT_NE(ctx.get(), nullptr);
    auto state1 = ctx->acquireState(opts, responsible);
    ASSERT_NE(state1.get(), nullptr);
    ASSERT_TRUE(!responsible);
  }
  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::RUNNING);
  ASSERT_TRUE(mgr->garbageCollect(/*abortAll*/ true));
  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::ABORTED);
}

TEST_F(TransactionManagerTest, aql_standalone_transaction) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  {
    auto ctx = transaction::StandaloneContext::Create(vocbase);
    SingleCollectionTransaction trx(ctx, "testCollection", AccessMode::Type::WRITE);
    ASSERT_TRUE(trx.begin().ok());

    auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": 1}");
    OperationOptions opts;
    auto opRes = trx.insert(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }

  auto ctx = std::make_shared<transaction::AQLStandaloneContext>(vocbase, tid);
  auto qq = "FOR doc IN testCollection RETURN doc";
  arangodb::aql::QueryResult qres = executeQuery(vocbase, qq, ctx);
  ASSERT_TRUE(qres.ok());
  ASSERT_NE(nullptr, qres.data);
  VPackSlice data = qres.data->slice();
  ASSERT_TRUE(data.isArray());
  ASSERT_EQ(data.length(), 1);
  EXPECT_TRUE(data.at(0).isObject());
  EXPECT_TRUE(data.at(0).hasKey("abc"));
}

TEST_F(TransactionManagerTest, abort_transactions_with_matcher) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  ASSERT_TRUE(res.ok());

  {
    auto ctx = mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE);
    ASSERT_NE(ctx.get(), nullptr);

    SingleCollectionTransaction trx(ctx, "testCollection", AccessMode::Type::WRITE);
    ASSERT_TRUE(!trx.isMainTransaction());

    auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": 1}");
    OperationOptions opts;
    auto opRes = trx.insert(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }
  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::RUNNING);

  //
  mgr->abortManagedTrx([](TransactionState const& state, std::string const & /*user*/) -> bool {
    TransactionCollection* tcoll =
        state.collection(DataSourceId{42}, AccessMode::Type::NONE);
    return tcoll != nullptr;
  });

  ASSERT_EQ(mgr->getManagedTrxStatus(tid, vocbase.name()), transaction::Status::ABORTED);
}

TEST_F(TransactionManagerTest, permission_denied_readonly) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Internal, "dummy",
                                "testVocbase", arangodb::auth::Level::RO,
                                arangodb::auth::Level::RO, false) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"read\": [\"42\"]}}");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  EXPECT_TRUE(res.ok());
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).ok());

  tid = TransactionId::createSingleServer();
  json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  ASSERT_EQ(res.errorNumber(), TRI_ERROR_ARANGO_READ_ONLY);
}

TEST_F(TransactionManagerTest, permission_denied_forbidden) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Internal, "dummy",
                                "testVocbase", arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE, false) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"read\": [\"42\"]}}");
  Result res = mgr->ensureManagedTrx(vocbase, tid, json->slice(), false);
  ASSERT_EQ(res.errorNumber(), TRI_ERROR_FORBIDDEN);
}
