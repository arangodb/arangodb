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

#pragma once

// Must be included early to avoid
//   "explicit specialization of 'std::char_traits<unsigned char>' after
//   instantiation"
// errors.
#include "utils/string.hpp"

#include "Replication2/ReplicatedState/ReplicatedStateImpl.tpp"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

#include "Mocks/Servers.h"
#include "Replication2/Mocks/DocumentStateMocks.h"
#include "Replication2/Mocks/MockVocbase.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"
#include "Transaction/Manager.h"

#include "gmock/gmock.h"

namespace arangodb::replication2::tests {

/**
 * Defines a test fixture for the DocumentStateMachine.
 * Contains all the mocks and interactions between them.
 */
struct DocumentStateMachineTest : testing::Test {
  std::vector<std::string> collectionData;
  std::shared_ptr<testing::NiceMock<MockCollectionReader>>
      collectionReaderMock =
          std::make_shared<testing::NiceMock<MockCollectionReader>>(
              collectionData);
  std::shared_ptr<testing::NiceMock<MockDatabaseSnapshot>>
      databaseSnapshotMock =
          std::make_shared<testing::NiceMock<MockDatabaseSnapshot>>(
              collectionReaderMock);
  std::shared_ptr<testing::NiceMock<MockDatabaseSnapshotFactory>>
      databaseSnapshotFactoryMock =
          std::make_shared<testing::NiceMock<MockDatabaseSnapshotFactory>>();

  std::shared_ptr<testing::NiceMock<MockDocumentStateTransaction>>
      transactionMock =
          std::make_shared<testing::NiceMock<MockDocumentStateTransaction>>();
  std::shared_ptr<testing::NiceMock<MockDocumentStateShardHandler>>
      shardHandlerMock =
          std::make_shared<testing::NiceMock<MockDocumentStateShardHandler>>();
  std::shared_ptr<testing::NiceMock<MockDocumentStateNetworkHandler>>
      networkHandlerMock = std::make_shared<
          testing::NiceMock<MockDocumentStateNetworkHandler>>();
  std::shared_ptr<testing::NiceMock<MockDocumentStateLeaderInterface>>
      leaderInterfaceMock = std::make_shared<
          testing::NiceMock<MockDocumentStateLeaderInterface>>();
  std::shared_ptr<testing::NiceMock<MockMaintenanceActionExecutor>>
      maintenanceActionExecutorMock =
          std::make_shared<testing::NiceMock<MockMaintenanceActionExecutor>>();

  std::shared_ptr<testing::NiceMock<MockDocumentStateHandlersFactory>>
      handlersFactoryMock =
          std::make_shared<testing::NiceMock<MockDocumentStateHandlersFactory>>(
              databaseSnapshotFactoryMock);
  MockTransactionManager transactionManagerMock;
  arangodb::tests::mocks::MockServer mockServer =
      arangodb::tests::mocks::MockServer();
  MockVocbase vocbaseMock =
      MockVocbase(mockServer.server(), "documentStateMachineTestDb", 2);

  auto createDocumentEntry(
      TransactionId tid,
      TRI_voc_document_operation_e op = TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    using namespace replicated_state::document;

    return DocumentLogEntry{ReplicatedOperation::buildDocumentOperation(
        op, TransactionId{tid}, shardId, velocypack::SharedSlice())};
  }

  auto createRealTransactionHandler()
      -> std::shared_ptr<MockDocumentStateTransactionHandler> {
    auto transactionHandlerMock =
        handlersFactoryMock->makeRealTransactionHandler(&vocbaseMock, globalId,
                                                        shardHandlerMock);

    ON_CALL(*handlersFactoryMock,
            createTransactionHandler(testing::_, testing::_, testing::_))
        .WillByDefault(
            [&](TRI_vocbase_t&, GlobalLogIdentifier const&,
                std::shared_ptr<replicated_state::document::
                                    IDocumentStateShardHandler> const&) {
              return std::make_unique<
                  testing::NiceMock<MockDocumentStateTransactionHandler>>(
                  transactionHandlerMock);
            });

    return transactionHandlerMock;
  }

  auto createLeader() -> std::shared_ptr<DocumentLeaderStateWrapper> {
    auto factory = replicated_state::document::DocumentFactory(
        handlersFactoryMock, transactionManagerMock);
    return std::make_shared<DocumentLeaderStateWrapper>(
        factory.constructCore(vocbaseMock, globalId, coreParams),
        handlersFactoryMock, transactionManagerMock);
  }

  auto createFollower() -> std::shared_ptr<DocumentFollowerStateWrapper> {
    auto factory = replicated_state::document::DocumentFactory(
        handlersFactoryMock, transactionManagerMock);
    return std::make_shared<DocumentFollowerStateWrapper>(
        factory.constructCore(vocbaseMock, globalId, coreParams),
        handlersFactoryMock);
  }

  void SetUp() override {
    using namespace testing;

    collectionData.emplace_back("foo");
    collectionData.emplace_back("bar");
    collectionData.emplace_back("baz");

    ON_CALL(*databaseSnapshotFactoryMock, createSnapshot).WillByDefault([&]() {
      return std::make_unique<MockDatabaseSnapshotDelegator>(
          databaseSnapshotMock);
    });

    ON_CALL(*transactionMock, commit).WillByDefault(Return(Result{}));
    ON_CALL(*transactionMock, abort).WillByDefault(Return(Result{}));
    ON_CALL(*transactionMock, apply(_)).WillByDefault([]() {
      return OperationResult{Result{}, OperationOptions{}};
    });
    ON_CALL(*transactionMock, intermediateCommit)
        .WillByDefault(Return(Result{}));

    ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&]() {
      return futures::Future<
          ResultT<replicated_state::document::SnapshotConfig>>{
          std::in_place,
          replicated_state::document::SnapshotConfig{
              replicated_state::document::SnapshotId{1}, shardMap}};
    });
    ON_CALL(*leaderInterfaceMock, nextSnapshotBatch)
        .WillByDefault([&](replicated_state::document::SnapshotId) {
          // An array is needed so that we can call the "length" method on the
          // the slice later on.
          auto payload = std::vector<int>{1, 2, 3};
          return futures::Future<
              ResultT<replicated_state::document::SnapshotBatch>>{
              std::in_place,
              replicated_state::document::SnapshotBatch{
                  .snapshotId = replicated_state::document::SnapshotId{1},
                  .shardId = shardId,
                  .hasMore = false,
                  .payload = velocypack::serialize(payload)}};
        });
    ON_CALL(*leaderInterfaceMock, finishSnapshot)
        .WillByDefault([&](replicated_state::document::SnapshotId) {
          return futures::Future<Result>{std::in_place};
        });

    ON_CALL(*networkHandlerMock, getLeaderInterface)
        .WillByDefault(Return(leaderInterfaceMock));

    ON_CALL(*maintenanceActionExecutorMock, executeCreateCollectionAction)
        .WillByDefault(Return(Result{}));
    ON_CALL(*maintenanceActionExecutorMock, executeDropCollectionAction)
        .WillByDefault(Return(Result{}));

    ON_CALL(*handlersFactoryMock, createShardHandler)
        .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
          ON_CALL(*shardHandlerMock, ensureShard).WillByDefault(Return(true));
          ON_CALL(*shardHandlerMock, dropShard).WillByDefault(Return(true));
          ON_CALL(*shardHandlerMock, dropAllShards)
              .WillByDefault(Return(Result{}));
          ON_CALL(*shardHandlerMock, isShardAvailable)
              .WillByDefault(Return(true));
          ON_CALL(*shardHandlerMock, getShardMap)
              .WillByDefault(
                  Return(replication2::replicated_state::document::ShardMap{}));
          return shardHandlerMock;
        });

    ON_CALL(*handlersFactoryMock, createTransactionHandler)
        .WillByDefault(
            [&](TRI_vocbase_t&, GlobalLogIdentifier gid,
                std::shared_ptr<
                    replicated_state::document::IDocumentStateShardHandler>
                    shardHandler) {
              return std::make_unique<
                  replicated_state::document::DocumentStateTransactionHandler>(
                  std::move(gid), nullptr, handlersFactoryMock,
                  std::move(shardHandler));
            });

    ON_CALL(*handlersFactoryMock, createSnapshotHandler)
        .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
          return std::make_unique<
              replicated_state::document::DocumentStateSnapshotHandler>(
              handlersFactoryMock->makeUniqueDatabaseSnapshotFactory(),
              MockDocumentStateSnapshotHandler::rebootTracker);
        });

    ON_CALL(*handlersFactoryMock, createTransaction)
        .WillByDefault(Return(transactionMock));

    ON_CALL(*handlersFactoryMock, createNetworkHandler)
        .WillByDefault(Return(networkHandlerMock));

    ON_CALL(*handlersFactoryMock, createMaintenanceActionExecutor)
        .WillByDefault(Return(maintenanceActionExecutorMock));
  }

  void TearDown() override {
    using namespace testing;

    collectionReaderMock->reset();
    Mock::VerifyAndClearExpectations(handlersFactoryMock.get());
    Mock::VerifyAndClearExpectations(shardHandlerMock.get());
    Mock::VerifyAndClearExpectations(transactionMock.get());
    Mock::VerifyAndClearExpectations(networkHandlerMock.get());
    Mock::VerifyAndClearExpectations(leaderInterfaceMock.get());
    Mock::VerifyAndClearExpectations(collectionReaderMock.get());
    Mock::VerifyAndClearExpectations(databaseSnapshotFactoryMock.get());
  }

  const std::string collectionId = "testCollectionID";
  static constexpr LogId logId = LogId{1};
  const std::string dbName = "testDB";
  const GlobalLogIdentifier globalId{dbName, logId};
  const ShardID shardId = "s1";
  const replicated_state::document::DocumentCoreParameters coreParams{dbName, 0,
                                                                      0};
  const velocypack::SharedSlice coreParamsSlice = coreParams.toSharedSlice();
  const std::string leaderId = "leader";
  const replicated_state::document::ShardMap shardMap{
      {shardId, replicated_state::document::ShardProperties{
                    .collection = collectionId,
                    .properties = std::make_shared<VPackBuilder>()}}};
};
}  // namespace arangodb::replication2::tests
