////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/ReplicatedState/StateMachines/DocumentState/DocumentStateMachineTest.h"

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::replication2;
using namespace arangodb::replication2::tests;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;

struct DocumentCoreTest : DocumentStateMachineTest {};

TEST_F(DocumentCoreTest, constructing_the_core_does_not_create_shard) {
  using namespace testing;

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);

  EXPECT_CALL(*shardHandlerMock, ensureShard(shardId, collectionId, _))
      .Times(0);
  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);

  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentCoreTest, dropping_the_core_with_error_messages) {
  using namespace testing;
  auto transactionHandlerMock = createRealTransactionHandler();

  ON_CALL(*transactionHandlerMock, applyEntry(Matcher<ReplicatedOperation>(_)))
      .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  EXPECT_CALL(*shardHandlerMock, dropAllShards()).Times(1);
  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);
  ON_CALL(*shardHandlerMock, dropAllShards)
      .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));
  core->drop();
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentStateMachineTest,
       shard_is_dropped_and_transactions_aborted_during_cleanup) {
  using namespace testing;

  // For simplicity, no shards for this snapshot.
  ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&]() {
    return futures::Future<ResultT<SnapshotConfig>>{
        std::in_place, SnapshotConfig{SnapshotId{1}, {}}};
  });

  auto transactionHandlerMock = handlersFactoryMock->makeRealTransactionHandler(
      &vocbaseMock, globalId, shardHandlerMock);
  ON_CALL(*handlersFactoryMock, createTransactionHandler(_, _, _))
      .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const&,
                         std::shared_ptr<IDocumentStateShardHandler> const&) {
        return std::make_unique<NiceMock<MockDocumentStateTransactionHandler>>(
            transactionHandlerMock);
      });

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);

  // transaction should be aborted before the snapshot is acquired
  EXPECT_CALL(
      *transactionHandlerMock,
      applyEntry(ReplicatedOperation::buildAbortAllOngoingTrxOperation()))
      .Times(1);
  auto res = follower->acquireSnapshot("participantId");
  EXPECT_TRUE(res.isReady() && res.get().ok());
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());

  EXPECT_CALL(
      *transactionHandlerMock,
      applyEntry(ReplicatedOperation::buildAbortAllOngoingTrxOperation()))
      .Times(1);
  EXPECT_CALL(*shardHandlerMock, dropAllShards()).Times(1);
  auto cleanupHandler = factory.constructCleanupHandler();
  auto core = std::move(*follower).resign();
  cleanupHandler->drop(std::move(core));
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());
}
