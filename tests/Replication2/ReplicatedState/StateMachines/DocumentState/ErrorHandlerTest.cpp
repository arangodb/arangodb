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

#include <gtest/gtest.h>

#include "Replication2/StateMachines/Document/DocumentStateErrorHandler.h"
#include "Replication2/StateMachines/Document/ReplicatedOperation.h"
#include "Utils/OperationResult.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state::document;

struct DocumentStateErrorHandlerTest : testing::Test {
  std::shared_ptr<IDocumentStateErrorHandler> errorHandler;

  void SetUp() override {
    errorHandler = std::make_shared<DocumentStateErrorHandler>(
        LoggerContext{Logger::REPLICATED_STATE});
  }

  void TearDown() override { errorHandler.reset(); }
};

TEST_F(DocumentStateErrorHandlerTest, create_shard_test) {
  auto op = ReplicatedOperation::buildCreateShardOperation(
      ShardID{"shard"}, TRI_COL_TYPE_DOCUMENT, velocypack::SharedSlice{});
  EXPECT_EQ(errorHandler->handleOpResult(op, TRI_ERROR_ARANGO_DUPLICATE_NAME),
            TRI_ERROR_NO_ERROR);
  EXPECT_EQ(errorHandler->handleOpResult(op, TRI_ERROR_WAS_ERLAUBE),
            TRI_ERROR_WAS_ERLAUBE);
}

TEST_F(DocumentStateErrorHandlerTest, drop_shard_test) {
  auto op = ReplicatedOperation::buildDropShardOperation(ShardID{"shard"});
  EXPECT_EQ(
      errorHandler->handleOpResult(op, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND),
      TRI_ERROR_NO_ERROR);
  EXPECT_EQ(errorHandler->handleOpResult(op, TRI_ERROR_WAS_ERLAUBE),
            TRI_ERROR_WAS_ERLAUBE);
}

TEST_F(DocumentStateErrorHandlerTest, modify_shard_test) {
  auto op = ReplicatedOperation::buildModifyShardOperation(
      ShardID{"shard"}, "collection", velocypack::SharedSlice{});
  EXPECT_EQ(
      errorHandler->handleOpResult(op, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND),
      TRI_ERROR_NO_ERROR);
  EXPECT_EQ(errorHandler->handleOpResult(op, TRI_ERROR_WAS_ERLAUBE),
            TRI_ERROR_WAS_ERLAUBE);
}

TEST_F(DocumentStateErrorHandlerTest, create_index_test) {
  auto op = ReplicatedOperation::buildCreateIndexOperation(
      ShardID{"shard"}, velocypack::SharedSlice());
  EXPECT_EQ(
      errorHandler->handleOpResult(op, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND),
      TRI_ERROR_NO_ERROR);
  EXPECT_EQ(errorHandler->handleOpResult(
                op, TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED),
            TRI_ERROR_NO_ERROR);
  EXPECT_EQ(errorHandler->handleOpResult(op, TRI_ERROR_WAS_ERLAUBE),
            TRI_ERROR_WAS_ERLAUBE);
}

TEST_F(DocumentStateErrorHandlerTest, drop_index_test) {
  auto op = ReplicatedOperation::buildDropIndexOperation(
      ShardID{"shard"}, velocypack::SharedSlice());
  EXPECT_EQ(
      errorHandler->handleOpResult(op, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND),
      TRI_ERROR_NO_ERROR);
  EXPECT_EQ(errorHandler->handleOpResult(op, TRI_ERROR_ARANGO_INDEX_NOT_FOUND),
            TRI_ERROR_NO_ERROR);
  EXPECT_EQ(errorHandler->handleOpResult(op, TRI_ERROR_WAS_ERLAUBE),
            TRI_ERROR_WAS_ERLAUBE);
}

TEST_F(DocumentStateErrorHandlerTest, document_transaction_test) {
  auto res = OperationResult{
      Result{TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED}, OperationOptions{}};
  EXPECT_EQ(
      errorHandler->handleDocumentTransactionResult(res, TransactionId{1}),
      TRI_ERROR_NO_ERROR);

  res = OperationResult{Result{TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND},
                        OperationOptions{}};
  EXPECT_EQ(
      errorHandler->handleDocumentTransactionResult(res, TransactionId{1}),
      TRI_ERROR_NO_ERROR);

  res = OperationResult{Result{TRI_ERROR_WAS_ERLAUBE}, OperationOptions{}};
  EXPECT_EQ(
      errorHandler->handleDocumentTransactionResult(res, TransactionId{1}),
      TRI_ERROR_WAS_ERLAUBE);

  res = OperationResult{Result{}, OperationOptions{}};
  res.countErrorCodes = {{TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, 1}};
  EXPECT_EQ(
      errorHandler->handleDocumentTransactionResult(res, TransactionId{1}),
      TRI_ERROR_NO_ERROR);

  res = OperationResult{Result{}, OperationOptions{}};
  res.countErrorCodes = {{TRI_ERROR_WAS_ERLAUBE, 1}};
  EXPECT_EQ(errorHandler->handleDocumentTransactionResult(res, TransactionId{1})
                .errorNumber(),
            TRI_ERROR_TRANSACTION_INTERNAL);
}

TEST_F(DocumentStateErrorHandlerTest, modify_transaction_test) {
  auto op = ReplicatedOperation::buildDocumentOperation(
      TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{1}, ShardID{"shard"},
      velocypack::SharedSlice{});
  EXPECT_EQ(
      errorHandler->handleOpResult(op, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND),
      TRI_ERROR_NO_ERROR);
  EXPECT_EQ(errorHandler->handleOpResult(op, TRI_ERROR_WAS_ERLAUBE),
            TRI_ERROR_WAS_ERLAUBE);
}

TEST_F(DocumentStateErrorHandlerTest, finish_transaction_test) {
  auto op = ReplicatedOperation::buildCommitOperation(TransactionId{1});
  EXPECT_EQ(errorHandler->handleOpResult(op, TRI_ERROR_TRANSACTION_NOT_FOUND),
            TRI_ERROR_TRANSACTION_NOT_FOUND);
  EXPECT_EQ(errorHandler->handleOpResult(op, TRI_ERROR_WAS_ERLAUBE),
            TRI_ERROR_WAS_ERLAUBE);
}
