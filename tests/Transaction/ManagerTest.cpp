////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Query.h"

#include "Aql/SharedQueryState.h"
#include "Basics/ScopeGuard.h"
#include "Basics/debugging.h"
#include "Cluster/ServerState.h"
#include "Rest/GeneralResponse.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Manager.h"
#include "Transaction/SmartContext.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/Status.h"
#include "Utils/ExecContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Mocks/CollectionDescriptors.h"
#include "Mocks/ExecContextFactory.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

#include "gtest/gtest.h"

#include "ManagerSetup.h"
#include "../IResearch/common.h"

using namespace arangodb;

static arangodb::aql::QueryResult executeQuery(
    TRI_vocbase_t& vocbase, std::string const& queryString,
    std::shared_ptr<transaction::Context> ctx) {
  auto query = arangodb::aql::Query::create(
      ctx, arangodb::aql::QueryString(queryString), nullptr);

  return query->executeSync();
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
      : vocbase(testDBInfo(setup.server.server()), setup.server.engine()),
        mgr(transaction::ManagerFeature::manager()),
        tid(TransactionId::createLeader()) {}

  ~TransactionManagerTest() { mgr->garbageCollect(true); }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(TransactionManagerTest, parsing_errors) {
  auto json = arangodb::velocypack::Parser::fromJson("{ \"write\": [33] }");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  EXPECT_TRUE(res.is(TRI_ERROR_BAD_PARAMETER));

  json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": \"33\"}, \"lockTimeout\": -1 }");
  res = mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                              transaction::OperationOriginTestCase{}, false)
            .waitAndGet();
  EXPECT_TRUE(res.is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(TransactionManagerTest, collection_not_found) {
  arangodb::ExecContextSuperuserScope exeScope;

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"read\": [\"33\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"33\"]}}");
  res = mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                              transaction::OperationOriginTestCase{}, false)
            .waitAndGet();
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"exclusive\": [\"33\"]}}");
  res = mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                              transaction::OperationOriginTestCase{}, false)
            .waitAndGet();
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

TEST_F(TransactionManagerTest, transaction_id_reuse) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"read\": [\"42\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  ASSERT_TRUE(res.ok());

  json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"33\"]}}");
  res = mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                              transaction::OperationOriginTestCase{}, false)
            .waitAndGet();
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_TRANSACTION_INTERNAL);

  res = mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet();
  ASSERT_TRUE(res.ok());
}

TEST_F(TransactionManagerTest, simple_transaction_and_abort) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  ASSERT_TRUE(res.ok());

  auto doc = arangodb::velocypack::Parser::fromJson("{ \"_key\": \"1\"}");
  {
    auto ctx =
        mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
    ASSERT_NE(ctx.get(), nullptr);
    auto origin = ctx->operationOrigin();
    ASSERT_EQ(transaction::OperationOrigin::Type::kInternal, origin.type);
    ASSERT_EQ("unit test", origin.description);

    SingleCollectionTransaction trx(ctx, "testCollection",
                                    AccessMode::Type::WRITE);
    ASSERT_TRUE(!trx.isMainTransaction());

    OperationOptions opts;
    auto opRes = trx.insert(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }

  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::RUNNING);

  {  // lease again
    auto ctx =
        mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
    ASSERT_NE(ctx.get(), nullptr);

    SingleCollectionTransaction trx(ctx, "testCollection",
                                    AccessMode::Type::READ);
    ASSERT_TRUE(!trx.isMainTransaction());

    OperationOptions opts;
    auto opRes = trx.document(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::RUNNING);
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet().ok());
  // perform same operation
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet().ok());
  // cannot commit aborted transaction
  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name())
                  .waitAndGet()
                  .is(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION));

  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);
}

TEST_F(TransactionManagerTest, simple_transaction_and_commit) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  ASSERT_TRUE(res.ok());

  {
    auto ctx =
        mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
    ASSERT_NE(ctx.get(), nullptr);

    SingleCollectionTransaction trx(std::move(ctx), "testCollection",
                                    AccessMode::Type::WRITE);
    ASSERT_TRUE(!trx.isMainTransaction());
    ASSERT_FALSE(
        trx.state()->hasHint(transaction::Hints::Hint::IS_FOLLOWER_TRX));

    auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": 1}");

    OperationOptions opts;
    auto opRes = trx.insert(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::RUNNING);
  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet().ok());
  // perform same operation
  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet().ok());
  // cannot commit aborted transaction
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name())
                  .waitAndGet()
                  .is(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION));

  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::COMMITTED);
}

TEST_F(TransactionManagerTest, simple_transaction_and_commit_is_follower) {
  tid = TransactionId::createFollower();
  auto beforeRole = arangodb::ServerState::instance()->getRole();
  auto roleGuard = scopeGuard([&]() noexcept {
    arangodb::ServerState::instance()->setRole(beforeRole);
  });
  arangodb::ServerState::instance()->setRole(
      arangodb::ServerState::ROLE_DBSERVER);
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, true)
          .waitAndGet();
  ASSERT_TRUE(res.ok());

  {
    auto ctx =
        mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
    ASSERT_NE(ctx.get(), nullptr);

    SingleCollectionTransaction trx(std::move(ctx), "testCollection",
                                    AccessMode::Type::WRITE);
    ASSERT_TRUE(!trx.isMainTransaction());
    ASSERT_TRUE(
        trx.state()->hasHint(transaction::Hints::Hint::IS_FOLLOWER_TRX));

    auto doc = arangodb::velocypack::Parser::fromJson(
        "{ \"_key\": \"blablabla\", \"abc\": 1}");

    OperationOptions opts;
    auto opRes = trx.insert(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::RUNNING);
  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet().ok());
  // perform same operation
  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet().ok());
  // cannot commit aborted transaction
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name())
                  .waitAndGet()
                  .is(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION));

  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::COMMITTED);
}

TEST_F(TransactionManagerTest, simple_transaction_and_commit_while_in_use) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  ASSERT_TRUE(res.ok());

  {
    auto ctx =
        mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
    ASSERT_NE(ctx.get(), nullptr);

    SingleCollectionTransaction trx(std::move(ctx), "testCollection",
                                    AccessMode::Type::WRITE);
    ASSERT_TRUE(!trx.isMainTransaction());

    auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": 1}");

    OperationOptions opts;
    auto opRes = trx.insert(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_EQ(
        TRI_ERROR_LOCKED,
        mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet().errorNumber());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::RUNNING);

  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet().ok());
  // perform same operation
  ASSERT_TRUE(mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet().ok());
  // cannot abort committed transaction
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name())
                  .waitAndGet()
                  .is(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION));
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::COMMITTED);
}

TEST_F(TransactionManagerTest, leading_multiple_readonly_transactions) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"read\": [\"42\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  ASSERT_TRUE(res.ok());

  {
    transaction::Options opts;
    bool responsible;

    auto ctx =
        mgr->leaseManagedTrx(tid, AccessMode::Type::READ, false).waitAndGet();
    ASSERT_NE(ctx.get(), nullptr);
    auto state1 = ctx->acquireState(opts, responsible);
    ASSERT_NE(state1.get(), nullptr);
    ASSERT_TRUE(!responsible);

    auto ctx2 =
        mgr->leaseManagedTrx(tid, AccessMode::Type::READ, false).waitAndGet();
    ASSERT_NE(ctx2.get(), nullptr);
    auto state2 = ctx2->acquireState(opts, responsible);
    EXPECT_EQ(state1.get(), state2.get());
    ASSERT_TRUE(!responsible);

    auto ctx3 =
        mgr->leaseManagedTrx(tid, AccessMode::Type::READ, false).waitAndGet();
    ASSERT_NE(ctx3.get(), nullptr);
    auto state3 = ctx3->acquireState(opts, responsible);
    EXPECT_EQ(state3.get(), state2.get());
    ASSERT_TRUE(!responsible);
  }
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet().ok());
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);
}

TEST_F(TransactionManagerTest, lock_conflict) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  ASSERT_TRUE(res.ok());
  {
    transaction::Options opts;
    bool responsible;

    auto ctx =
        mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
    ASSERT_NE(ctx.get(), nullptr);
    auto state1 = ctx->acquireState(opts, responsible);
    ASSERT_NE(state1.get(), nullptr);
    ASSERT_TRUE(!responsible);
    ASSERT_ANY_THROW(
        mgr->leaseManagedTrx(tid, AccessMode::Type::READ, false).waitAndGet());
  }
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet().ok());
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);
}

TEST_F(TransactionManagerTest, lock_conflict_side_user) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  ASSERT_TRUE(res.ok());
  {
    transaction::Options opts;
    bool responsible;

    auto ctx =
        mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
    ASSERT_NE(ctx.get(), nullptr);
    auto state1 = ctx->acquireState(opts, responsible);
    ASSERT_NE(state1.get(), nullptr);
    ASSERT_TRUE(!responsible);
    ASSERT_ANY_THROW(
        mgr->leaseManagedTrx(tid, AccessMode::Type::READ, false).waitAndGet());

    auto ctxSide =
        mgr->leaseManagedTrx(tid, AccessMode::Type::READ, true).waitAndGet();
    ASSERT_NE(ctxSide.get(), nullptr);
    auto state2 = ctxSide->acquireState(opts, responsible);
    ASSERT_NE(state2.get(), nullptr);
    ASSERT_TRUE(!responsible);
  }
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet().ok());
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);
}

TEST_F(TransactionManagerTest, garbage_collection_shutdown) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  ASSERT_TRUE(res.ok());
  {
    transaction::Options opts;
    bool responsible;

    auto ctx =
        mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
    ASSERT_NE(ctx.get(), nullptr);
    auto state1 = ctx->acquireState(opts, responsible);
    ASSERT_NE(state1.get(), nullptr);
    ASSERT_TRUE(!responsible);
  }
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::RUNNING);
  ASSERT_TRUE(mgr->garbageCollect(/*abortAll*/ true));
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);
}

TEST_F(TransactionManagerTest, aql_standalone_transaction) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  {
    auto ctx = transaction::StandaloneContext::create(
        vocbase, arangodb::transaction::OperationOriginTestCase{});
    SingleCollectionTransaction trx(std::move(ctx), "testCollection",
                                    AccessMode::Type::WRITE);
    ASSERT_TRUE(trx.begin().ok());

    auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": 1}");
    OperationOptions opts;
    auto opRes = trx.insert(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }

  auto ctx = std::make_shared<transaction::AQLStandaloneContext>(
      vocbase, tid,
      arangodb::transaction::OperationOriginAQL{"running AQL query"});
  auto origin = ctx->operationOrigin();
  ASSERT_EQ(transaction::OperationOrigin::Type::kAQL, origin.type);
  ASSERT_EQ("running AQL query", origin.description);

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
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"42\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  ASSERT_TRUE(res.ok());

  {
    auto ctx =
        mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
    ASSERT_NE(ctx.get(), nullptr);

    SingleCollectionTransaction trx(std::move(ctx), "testCollection",
                                    AccessMode::Type::WRITE);
    ASSERT_TRUE(!trx.isMainTransaction());

    auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": 1}");
    OperationOptions opts;
    auto opRes = trx.insert(coll->name(), doc->slice(), opts);
    ASSERT_TRUE(opRes.ok());
    ASSERT_TRUE(trx.finish(opRes.result).ok());
  }
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::RUNNING);

  mgr->abortManagedTrx(
      [](TransactionState const& state, std::string const& /*user*/) -> bool {
        TransactionCollection* tcoll =
            state.collection(DataSourceId{42}, AccessMode::Type::NONE);
        return tcoll != nullptr;
      });

  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);
}

TEST_F(TransactionManagerTest, permission_denied_readonly) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto classicCtx = arangodb::tests::mocks::makeClassicExecContext(
      "dummy", "testVocbase", arangodb::auth::Level::RO,
      arangodb::auth::Level::RO);
  arangodb::ExecContextScope execContextScope(classicCtx.execContext);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"read\": [\"testCollection\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  EXPECT_TRUE(res.ok());
  ASSERT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet().ok());

  tid = TransactionId::createSingleServer();
  json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"testCollection\"]}}");
  res = mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                              transaction::OperationOriginTestCase{}, false)
            .waitAndGet();
  ASSERT_EQ(res.errorNumber(), TRI_ERROR_ARANGO_READ_ONLY);
}

TEST_F(TransactionManagerTest, permission_denied_forbidden) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor = arangodb::tests::testCollectionDescriptor(
        "testCollection", arangodb::DataSourceId{42});
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto classicCtx = arangodb::tests::mocks::makeClassicExecContext(
      "dummy", "testVocbase", arangodb::auth::Level::NONE,
      arangodb::auth::Level::NONE);
  arangodb::ExecContextScope execContextScope(classicCtx.execContext);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"read\": [\"testCollection\"]}}");
  Result res =
      mgr->ensureManagedTrx(vocbase, tid, json->slice(),
                            transaction::OperationOriginTestCase{}, false)
          .waitAndGet();
  ASSERT_EQ(res.errorNumber(), TRI_ERROR_FORBIDDEN);
}

// -----------------------------------------------------------------------------
// --SECTION--                             access to other users' transactions
// -----------------------------------------------------------------------------

namespace {

/**
 * Begins a write transaction on `testCollection` as user "alice" and returns
 * once it is running
 */
void beginTransactionAsAlice(TRI_vocbase_t& vocbase, transaction::Manager& mgr,
                             TransactionId tid) {
  auto colDescriptor = arangodb::tests::testCollectionDescriptor(
      "testCollection", arangodb::DataSourceId{42});
  ASSERT_NE(vocbase.createCollection(colDescriptor), nullptr);

  auto const alice = arangodb::tests::mocks::makeClassicExecContext(
      "alice", vocbase.name(), arangodb::auth::Level::NONE,
      arangodb::auth::Level::RW);
  arangodb::ExecContextScope scope(alice.execContext);

  auto collections = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"testCollection\"]}}");
  ASSERT_TRUE(mgr.ensureManagedTrx(vocbase, tid, collections->slice(),
                                   transaction::OperationOriginTestCase{},
                                   false)
                  .waitAndGet()
                  .ok());
}

/// @brief ids of the transactions the current identity may see in `database`
std::vector<std::string> visibleTransactionIds(transaction::Manager& mgr,
                                               std::string const& database) {
  arangodb::velocypack::Builder builder;
  {
    arangodb::velocypack::ArrayBuilder guard(&builder);
    mgr.toVelocyPack(builder, database, "", /*fanout*/ false,
                     /*details*/ false);
  }
  std::vector<std::string> ids;
  for (auto const entry :
       arangodb::velocypack::ArrayIterator(builder.slice())) {
    ids.push_back(entry.get("id").copyString());
  }
  return ids;
}

}  // namespace

TEST_F(TransactionManagerTest, admin_can_abort_foreign_transaction) {
  beginTransactionAsAlice(vocbase, *mgr, tid);

  // classic admin: read-write access to _system
  auto const admin = arangodb::tests::mocks::makeClassicExecContext(
      "admin", vocbase.name(), arangodb::auth::Level::RW,
      arangodb::auth::Level::RW);
  arangodb::ExecContextScope scope(admin.execContext);

  EXPECT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet().ok());
}

TEST_F(TransactionManagerTest, user_cannot_abort_foreign_transaction) {
  beginTransactionAsAlice(vocbase, *mgr, tid);

  {
    auto const bob = arangodb::tests::mocks::makeClassicExecContext(
        "bob", vocbase.name(), arangodb::auth::Level::NONE,
        arangodb::auth::Level::RO);
    arangodb::ExecContextScope scope(bob.execContext);

    auto const res = mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet();
    EXPECT_EQ(res.errorNumber(), TRI_ERROR_TRANSACTION_NOT_FOUND);
  }

  // the transaction is still there: its owner can abort it
  auto const alice = arangodb::tests::mocks::makeClassicExecContext(
      "alice", vocbase.name(), arangodb::auth::Level::NONE,
      arangodb::auth::Level::RW);
  arangodb::ExecContextScope scope(alice.execContext);
  EXPECT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet().ok());
}

TEST_F(TransactionManagerTest, admin_cannot_commit_foreign_transaction) {
  beginTransactionAsAlice(vocbase, *mgr, tid);

  auto const admin = arangodb::tests::mocks::makeClassicExecContext(
      "admin", vocbase.name(), arangodb::auth::Level::RW,
      arangodb::auth::Level::RW);
  arangodb::ExecContextScope scope(admin.execContext);

  auto const res = mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet();
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_TRANSACTION_NOT_FOUND);
  // aborting it is allowed, though
  EXPECT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet().ok());
}

TEST_F(TransactionManagerTest, admin_sees_foreign_transaction) {
  beginTransactionAsAlice(vocbase, *mgr, tid);

  auto const admin = arangodb::tests::mocks::makeClassicExecContext(
      "admin", vocbase.name(), arangodb::auth::Level::RW,
      arangodb::auth::Level::RW);
  arangodb::ExecContextScope scope(admin.execContext);

  EXPECT_EQ(visibleTransactionIds(*mgr, vocbase.name()),
            std::vector<std::string>{std::to_string(tid.id())});
  EXPECT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet().ok());
}

TEST_F(TransactionManagerTest, user_does_not_see_foreign_transaction) {
  beginTransactionAsAlice(vocbase, *mgr, tid);

  {
    auto const bob = arangodb::tests::mocks::makeClassicExecContext(
        "bob", vocbase.name(), arangodb::auth::Level::NONE,
        arangodb::auth::Level::RO);
    arangodb::ExecContextScope scope(bob.execContext);

    EXPECT_TRUE(visibleTransactionIds(*mgr, vocbase.name()).empty());
  }

  auto const alice = arangodb::tests::mocks::makeClassicExecContext(
      "alice", vocbase.name(), arangodb::auth::Level::NONE,
      arangodb::auth::Level::RW);
  arangodb::ExecContextScope scope(alice.execContext);
  EXPECT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet().ok());
}

TEST_F(TransactionManagerTest, transaction_invalid_mode) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor =
        arangodb::tests::testCollectionDescriptor("testCollection");
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"read\": [\"testCollection\"]}}");
  Result res = mgr->ensureManagedTrx(
                      vocbase, tid, json->slice(),
                      transaction::OperationOriginInternal{"some test"}, false)
                   .waitAndGet();
  ASSERT_TRUE(res.ok());

  auto ctx =
      mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
  ASSERT_NE(ctx.get(), nullptr);

  ASSERT_THROW(SingleCollectionTransaction(std::move(ctx), "testCollection",
                                           AccessMode::Type::WRITE),
               arangodb::basics::Exception);
  ;

  res = mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet();
  ASSERT_TRUE(res.ok());
}

TEST_F(TransactionManagerTest, transaction_origin) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor =
        arangodb::tests::testCollectionDescriptor("testCollection");
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"testCollection\"]}}");
  Result res = mgr->ensureManagedTrx(
                      vocbase, tid, json->slice(),
                      transaction::OperationOriginInternal{"some test"}, false)
                   .waitAndGet();
  ASSERT_TRUE(res.ok());

  auto ctx =
      mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
  ASSERT_NE(ctx.get(), nullptr);

  {
    SingleCollectionTransaction trx(std::move(ctx), "testCollection",
                                    AccessMode::Type::WRITE);
    ASSERT_FALSE(trx.isMainTransaction());
    auto origin = trx.state()->operationOrigin();
    ASSERT_EQ(transaction::OperationOrigin::Type::kInternal, origin.type);
    ASSERT_EQ("some test", origin.description);
  }

  res = mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet();
  ASSERT_TRUE(res.ok());
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
TEST_F(TransactionManagerTest, expired_transaction) {
  auto guard = scopeGuard([]() noexcept { TRI_ClearFailurePointsDebugging(); });

  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor =
        arangodb::tests::testCollectionDescriptor("testCollection");
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"testCollection\"]}}");

  // disables garbage-collection
  TRI_AddFailurePointDebugging("transaction::Manager::noGC");
  // sets TTL for transaction to a very low value
  TRI_AddFailurePointDebugging("transaction::Manager::shortTTL");
  Result res = mgr->ensureManagedTrx(
                      vocbase, tid, json->slice(),
                      transaction::OperationOriginInternal{"some test"}, false)
                   .waitAndGet();
  ASSERT_TRUE(res.ok());

  // wait until trx is expired
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  // we cannot use the transaction anymore
  ASSERT_ANY_THROW(
      mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet());

  // aborting it is fine though
  res = mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet();
  ASSERT_TRUE(res.ok());

  res = mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet();
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION, res.errorNumber());
}
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
TEST_F(TransactionManagerTest, lock_usage_of_expired_transaction) {
  auto guard = scopeGuard([]() noexcept { TRI_ClearFailurePointsDebugging(); });

  std::shared_ptr<LogicalCollection> coll;
  {
    auto colDescriptor =
        arangodb::tests::testCollectionDescriptor("testCollection");
    coll = vocbase.createCollection(colDescriptor);
  }
  ASSERT_NE(coll, nullptr);

  auto json1 = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"exclusive\": [\"testCollection\"]}}");

  // sets TTL for transaction to a very low value
  TRI_AddFailurePointDebugging("transaction::Manager::shortTTL");
  Result res = mgr->ensureManagedTrx(
                      vocbase, tid, json1->slice(),
                      transaction::OperationOriginInternal{"some test"}, false)
                   .waitAndGet();
  ASSERT_TRUE(res.ok());

  // wait until trx is expired
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  // we must now be able to open a second transaction on the
  // underlying collection, at least after the garbage collection
  mgr->garbageCollect(/*abortAll*/ false);

  TRI_ClearFailurePointsDebugging();

  TransactionId tid2 = TransactionId::createLeader();
  auto json2 = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"testCollection\"]}}");
  res = mgr->ensureManagedTrx(vocbase, tid2, json2->slice(),
                              transaction::OperationOriginInternal{"some test"},
                              false)
            .waitAndGet();
  ASSERT_TRUE(res.ok());

  // aborting trx1 is still fine, even though it is expired
  res = mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet();
  ASSERT_TRUE(res.ok());

  // committing trx2 must be ok
  res = mgr->commitManagedTrx(tid2, vocbase.name()).waitAndGet();
  ASSERT_TRUE(res.ok());
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                 soft-aborted managed transactions
// -----------------------------------------------------------------------------

namespace {

/// @brief begins a write transaction on a fresh collection `testCollection`
void beginWriteTransaction(TRI_vocbase_t& vocbase, transaction::Manager& mgr,
                           TransactionId tid) {
  auto colDescriptor = arangodb::tests::testCollectionDescriptor(
      "testCollection", arangodb::DataSourceId{42});
  ASSERT_NE(vocbase.createCollection(colDescriptor), nullptr);

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"collections\":{\"write\": [\"testCollection\"]}}");
  ASSERT_TRUE(mgr.ensureManagedTrx(vocbase, tid, json->slice(),
                                   transaction::OperationOriginTestCase{},
                                   false)
                  .waitAndGet()
                  .ok());
}

/// @brief error number thrown when leasing the transaction, or
/// TRI_ERROR_NO_ERROR if leasing succeeds
ErrorCode leaseErrorNumber(transaction::Manager& mgr, TransactionId tid) {
  try {
    auto ctx =
        mgr.leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
    return TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  }
}

/// @brief details entry of the transaction as reported by toVelocyPack()
std::shared_ptr<arangodb::velocypack::Builder> transactionDetails(
    transaction::Manager& mgr, std::string const& database, TransactionId tid) {
  arangodb::velocypack::Builder builder;
  {
    arangodb::velocypack::ArrayBuilder guard(&builder);
    mgr.toVelocyPack(builder, database, "", /*fanout*/ false,
                     /*details*/ true);
  }
  auto result = std::make_shared<arangodb::velocypack::Builder>();
  for (auto const entry :
       arangodb::velocypack::ArrayIterator(builder.slice())) {
    if (entry.get("id").copyString() == std::to_string(tid.id())) {
      result->add(entry);
    }
  }
  return result;
}

}  // namespace

TEST_F(TransactionManagerTest, soft_abort_unknown_transaction) {
  EXPECT_FALSE(mgr->softAbortManagedTrxForTesting(tid));
  EXPECT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::UNDEFINED);
}

TEST_F(TransactionManagerTest,
       soft_aborted_transaction_is_aborted_not_expired) {
  arangodb::ExecContextSuperuserScope exeScope;
  beginWriteTransaction(vocbase, *mgr, tid);
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::RUNNING);

  ASSERT_TRUE(mgr->softAbortManagedTrxForTesting(tid));

  // the transaction reports as aborted
  EXPECT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);

  // ... and is flagged as soft-aborted, but has not timed out
  auto details = transactionDetails(*mgr, vocbase.name(), tid);
  ASSERT_FALSE(details->isEmpty());
  EXPECT_TRUE(details->slice().get("softAborted").isTrue());
  EXPECT_TRUE(details->slice().get("expired").isFalse());
  EXPECT_EQ(details->slice().get("type").copyString(), "managed");

  // it cannot be leased anymore
  EXPECT_EQ(leaseErrorNumber(*mgr, tid), TRI_ERROR_TRANSACTION_ABORTED);
}

TEST_F(TransactionManagerTest, soft_aborted_transaction_cannot_be_committed) {
  arangodb::ExecContextSuperuserScope exeScope;
  beginWriteTransaction(vocbase, *mgr, tid);
  ASSERT_TRUE(mgr->softAbortManagedTrxForTesting(tid));

  std::ignore = mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet();
  // the commit request turned into an abort
  EXPECT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);

  // the transaction is now a tombstone, and the tombstone is not marked
  // as expired
  auto res = mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet();
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION);
  EXPECT_NE(res.errorMessage().find("aborted"), std::string::npos)
      << res.errorMessage();
  EXPECT_EQ(res.errorMessage().find("expired"), std::string::npos)
      << res.errorMessage();
}

TEST_F(TransactionManagerTest, soft_aborted_transaction_can_be_aborted) {
  arangodb::ExecContextSuperuserScope exeScope;
  beginWriteTransaction(vocbase, *mgr, tid);
  ASSERT_TRUE(mgr->softAbortManagedTrxForTesting(tid));

  EXPECT_TRUE(mgr->abortManagedTrx(tid, vocbase.name()).waitAndGet().ok());
  EXPECT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);
  EXPECT_EQ(leaseErrorNumber(*mgr, tid), TRI_ERROR_TRANSACTION_ABORTED);
}

TEST_F(TransactionManagerTest, garbage_collection_aborts_soft_aborted_trx) {
  arangodb::ExecContextSuperuserScope exeScope;
  beginWriteTransaction(vocbase, *mgr, tid);

  // a running, unexpired transaction is left alone by the garbage collection
  EXPECT_FALSE(mgr->garbageCollect(/*abortAll*/ false));
  ASSERT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::RUNNING);
  EXPECT_TRUE(transactionDetails(*mgr, vocbase.name(), tid)
                  ->slice()
                  .get("type")
                  .isEqualString("managed"));

  // once soft-aborted, it is aborted even though it has not expired
  ASSERT_TRUE(mgr->softAbortManagedTrxForTesting(tid));
  EXPECT_TRUE(mgr->garbageCollect(/*abortAll*/ false));
  EXPECT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);
  // tombstones are not listed anymore
  EXPECT_TRUE(transactionDetails(*mgr, vocbase.name(), tid)->isEmpty());

  // and is not soft-aborted a second time
  EXPECT_FALSE(mgr->softAbortManagedTrxForTesting(tid));
}

TEST_F(TransactionManagerTest, shutdown_soft_aborts_transaction_in_use) {
  arangodb::ExecContextSuperuserScope exeScope;
  beginWriteTransaction(vocbase, *mgr, tid);

  {
    auto ctx =
        mgr->leaseManagedTrx(tid, AccessMode::Type::WRITE, false).waitAndGet();
    ASSERT_NE(ctx.get(), nullptr);

    // the transaction is in use, so it can only be soft-aborted
    EXPECT_TRUE(mgr->garbageCollect(/*abortAll*/ true));

    EXPECT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);
    auto details = transactionDetails(*mgr, vocbase.name(), tid);
    ASSERT_FALSE(details->isEmpty());
    EXPECT_TRUE(details->slice().get("type").isEqualString("managed"));
    EXPECT_TRUE(details->slice().get("softAborted").isTrue());
    EXPECT_TRUE(details->slice().get("expired").isFalse());

    // nobody else can lease it in the meantime
    EXPECT_EQ(leaseErrorNumber(*mgr, tid), TRI_ERROR_TRANSACTION_ABORTED);
  }

  // returning the lease aborted the transaction
  EXPECT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);
  // tombstones are not listed anymore
  EXPECT_TRUE(transactionDetails(*mgr, vocbase.name(), tid)->isEmpty());
  EXPECT_EQ(
      mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet().errorNumber(),
      TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION);
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
TEST_F(TransactionManagerTest, expired_transaction_is_not_soft_aborted) {
  auto guard = scopeGuard([]() noexcept { TRI_ClearFailurePointsDebugging(); });
  arangodb::ExecContextSuperuserScope exeScope;

  // disables garbage-collection
  TRI_AddFailurePointDebugging("transaction::Manager::noGC");
  // sets TTL for transaction to a very low value
  TRI_AddFailurePointDebugging("transaction::Manager::shortTTL");
  beginWriteTransaction(vocbase, *mgr, tid);

  // wait until trx is expired
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  EXPECT_EQ(mgr->getManagedTrxStatus(tid), transaction::Status::ABORTED);
  auto details = transactionDetails(*mgr, vocbase.name(), tid);
  ASSERT_FALSE(details->isEmpty());
  EXPECT_TRUE(details->slice().get("expired").isTrue());
  EXPECT_TRUE(details->slice().get("softAborted").isFalse());

  // expired transactions are reported as such, not as aborted
  EXPECT_EQ(leaseErrorNumber(*mgr, tid), TRI_ERROR_TRANSACTION_NOT_FOUND);

  // garbage collection aborts expired transactions
  TRI_RemoveFailurePointDebugging("transaction::Manager::noGC");
  EXPECT_TRUE(mgr->garbageCollect(/*abortAll*/ false));
  // tombstones are not listed anymore
  EXPECT_TRUE(transactionDetails(*mgr, vocbase.name(), tid)->isEmpty());
  auto res = mgr->commitManagedTrx(tid, vocbase.name()).waitAndGet();
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION);
  EXPECT_NE(res.errorMessage().find("expired"), std::string::npos)
      << res.errorMessage();
}
#endif
