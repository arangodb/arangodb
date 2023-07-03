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

#include "Cluster/ClusterTypes.h"
#include "Inspection/Format.h"
#include "Inspection/Status.h"
#include "Inspection/Types.h"
#include "VocBase/Identifiers/TransactionId.h"

#include <variant>

namespace arangodb::replication2::replicated_state::document {

/*
 * When a log entry is received, the ReplicatedOperation tells the state machine
 * what it has to do (i.e. start a transaction, drop a shard, ...).
 */
struct ReplicatedOperation {
  ReplicatedOperation() = default;

  struct DocumentOperation {
    TransactionId tid;
    ShardID shard;
    velocypack::SharedSlice payload;

    DocumentOperation() = default;
    explicit DocumentOperation(TransactionId tid, ShardID shard,
                               velocypack::SharedSlice payload);

    friend auto operator==(DocumentOperation const& a,
                           DocumentOperation const& b) -> bool {
      return a.tid == b.tid && a.shard == b.shard &&
             a.payload.binaryEquals(b.payload.slice());
    }
  };

  struct AbortAllOngoingTrx {
    friend auto operator==(AbortAllOngoingTrx const&, AbortAllOngoingTrx const&)
        -> bool = default;
  };

  struct Commit {
    TransactionId tid;

    friend auto operator==(Commit const&, Commit const&) -> bool = default;
  };

  struct IntermediateCommit {
    TransactionId tid;

    friend auto operator==(IntermediateCommit const&, IntermediateCommit const&)
        -> bool = default;
  };

  struct Abort {
    TransactionId tid;

    friend auto operator==(Abort const&, Abort const&) -> bool = default;
  };

  struct Truncate {
    TransactionId tid;
    ShardID shard;

    friend auto operator==(Truncate const&, Truncate const&) -> bool = default;
  };

  struct CreateShard {
    ShardID shard;
    CollectionID collection;
    std::shared_ptr<VPackBuilder> properties;

    friend auto operator==(CreateShard const&, CreateShard const&)
        -> bool = default;
  };

  struct DropShard {
    ShardID shard;
    CollectionID collection;

    friend auto operator==(DropShard const&, DropShard const&)
        -> bool = default;
  };

  struct Insert : DocumentOperation {};

  struct Update : DocumentOperation {};

  struct Replace : DocumentOperation {};

  struct Remove : DocumentOperation {};

 public:
  using OperationType =
      std::variant<AbortAllOngoingTrx, Commit, IntermediateCommit, Abort,
                   Truncate, CreateShard, DropShard, Insert, Update, Replace,
                   Remove>;
  OperationType operation;

  static auto fromOperationType(OperationType op) noexcept
      -> ReplicatedOperation;
  static auto buildAbortAllOngoingTrxOperation() noexcept
      -> ReplicatedOperation;
  static auto buildCommitOperation(TransactionId tid) noexcept
      -> ReplicatedOperation;
  static auto buildIntermediateCommitOperation(TransactionId tid) noexcept
      -> ReplicatedOperation;
  static auto buildAbortOperation(TransactionId tid) noexcept
      -> ReplicatedOperation;
  static auto buildTruncateOperation(TransactionId tid, ShardID shard) noexcept
      -> ReplicatedOperation;
  static auto buildCreateShardOperation(
      ShardID shard, CollectionID collection,
      std::shared_ptr<VPackBuilder> properties) noexcept -> ReplicatedOperation;
  static auto buildDropShardOperation(ShardID shard,
                                      CollectionID collection) noexcept
      -> ReplicatedOperation;
  static auto buildDocumentOperation(TRI_voc_document_operation_e const& op,
                                     TransactionId tid, ShardID shard,
                                     velocypack::SharedSlice payload) noexcept
      -> ReplicatedOperation;

  friend auto operator==(ReplicatedOperation const&, ReplicatedOperation const&)
      -> bool = default;

 private:
  template<typename... Args>
  explicit ReplicatedOperation(std::in_place_t, Args&&... args) noexcept;
};

template<typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

template<class T>
concept ModifiesUserTransaction =
    std::is_same_v<T, ReplicatedOperation::Truncate> ||
    std::is_same_v<T, ReplicatedOperation::Insert> ||
    std::is_same_v<T, ReplicatedOperation::Update> ||
    std::is_same_v<T, ReplicatedOperation::Replace> ||
    std::is_same_v<T, ReplicatedOperation::Remove>;

template<class T>
concept FinishesUserTransaction =
    std::is_same_v<T, ReplicatedOperation::Commit> ||
    std::is_same_v<T, ReplicatedOperation::Abort>;

template<class T>
concept FinishesUserTransactionOrIntermediate = FinishesUserTransaction<T> ||
    std::is_same_v<T, ReplicatedOperation::IntermediateCommit>;

template<class T>
concept InsertsDocuments =
    IsAnyOf<T, ReplicatedOperation::Insert, ReplicatedOperation::Update,
            ReplicatedOperation::Replace>;

template<class T>
concept UserTransaction =
    ModifiesUserTransaction<T> || FinishesUserTransactionOrIntermediate<T>;

auto operator<<(std::ostream&, ReplicatedOperation const&) -> std::ostream&;
auto operator<<(std::ostream&, ReplicatedOperation::OperationType const&)
    -> std::ostream&;
}  // namespace arangodb::replication2::replicated_state::document
