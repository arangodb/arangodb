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

#include "Replication2/StateMachines/Document/ReplicatedOperation.h"
#include "Replication2/StateMachines/Document/ReplicatedOperationInspectors.h"

#include "Assertions/AssertionLogger.h"
#include "Assertions/ProdAssert.h"
#include "Inspection/VPack.h"

namespace arangodb::replication2::replicated_state::document {

ReplicatedOperation::DocumentOperation::DocumentOperation(
    TransactionId tid, ShardID shard, velocypack::SharedSlice payload)
    : tid{tid}, shard{std::move(shard)}, payload{std::move(payload)} {}

template<typename... Args>
ReplicatedOperation::ReplicatedOperation(std::in_place_t,
                                         Args&&... args) noexcept
    : operation(std::forward<Args>(args)...) {}

auto ReplicatedOperation::fromOperationType(OperationType op) noexcept
    -> ReplicatedOperation {
  return ReplicatedOperation{std::in_place, std::move(op)};
}

auto ReplicatedOperation::buildAbortAllOngoingTrxOperation() noexcept
    -> ReplicatedOperation {
  return ReplicatedOperation{std::in_place, AbortAllOngoingTrx{}};
}

auto ReplicatedOperation::buildCommitOperation(TransactionId tid) noexcept
    -> ReplicatedOperation {
  return ReplicatedOperation{std::in_place, Commit{tid}};
}

auto ReplicatedOperation::buildIntermediateCommitOperation(
    TransactionId tid) noexcept -> ReplicatedOperation {
  return ReplicatedOperation{std::in_place, IntermediateCommit{tid}};
}

auto ReplicatedOperation::buildAbortOperation(TransactionId tid) noexcept
    -> ReplicatedOperation {
  return ReplicatedOperation{std::in_place, Abort{tid}};
}

auto ReplicatedOperation::buildTruncateOperation(TransactionId tid,
                                                 ShardID shard) noexcept
    -> ReplicatedOperation {
  return ReplicatedOperation{std::in_place, Truncate{tid, std::move(shard)}};
}

auto ReplicatedOperation::buildCreateShardOperation(
    ShardID shard, CollectionID collection,
    std::shared_ptr<VPackBuilder> properties) noexcept -> ReplicatedOperation {
  return ReplicatedOperation{
      std::in_place, CreateShard{std::move(shard), std::move(collection),
                                 std::move(properties)}};
}

auto ReplicatedOperation::buildModifyShardOperation(
    ShardID shard, CollectionID collection,
    std::shared_ptr<VPackBuilder> properties,
    std::string followersToDrop) noexcept -> ReplicatedOperation {
  return ReplicatedOperation{
      std::in_place,
      ModifyShard{std::move(shard), std::move(collection),
                  std::move(properties), std::move(followersToDrop), true}};
}

auto ReplicatedOperation::buildDropShardOperation(
    ShardID shard, CollectionID collection) noexcept -> ReplicatedOperation {
  return ReplicatedOperation{
      std::in_place, DropShard{std::move(shard), std::move(collection)}};
}

auto ReplicatedOperation::buildDocumentOperation(
    TRI_voc_document_operation_e const& op, TransactionId tid, ShardID shard,
    velocypack::SharedSlice payload) noexcept -> ReplicatedOperation {
  switch (op) {
    case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      return ReplicatedOperation{
          std::in_place,
          Insert{DocumentOperation(tid, std::move(shard), std::move(payload))}};
    case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
      return ReplicatedOperation{
          std::in_place,
          Update{DocumentOperation(tid, std::move(shard), std::move(payload))}};
    case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
      return ReplicatedOperation{
          std::in_place, Replace{DocumentOperation(tid, std::move(shard),
                                                   std::move(payload))}};
    case TRI_VOC_DOCUMENT_OPERATION_REMOVE:
      return ReplicatedOperation{
          std::in_place,
          Remove{DocumentOperation(tid, std::move(shard), std::move(payload))}};
    default:
      ADB_PROD_ASSERT(false) << "Unexpected document operation " << op;
  }
  return {};  // should never be reached, but compiler complains about
              // missing return
}

auto operator<<(std::ostream& ostream, ReplicatedOperation const& operation)
    -> std::ostream& {
  return ostream << velocypack::serialize(operation).toJson();
}

auto operator<<(std::ostream& ostream,
                ReplicatedOperation::OperationType const& operation)
    -> std::ostream& {
  auto replicatedOp = ReplicatedOperation::fromOperationType(operation);
  return ostream << velocypack::serialize(replicatedOp).toJson();
}
}  // namespace arangodb::replication2::replicated_state::document
