////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

 private:
  struct DocumentOperation {
    TransactionId tid;
    ShardID shard;
    velocypack::SharedSlice payload;

    explicit DocumentOperation() = default;
    explicit DocumentOperation(TransactionId tid, ShardID shard,
                               velocypack::SharedSlice payload);

    template<class Inspector>
    friend auto inspect(Inspector& f, DocumentOperation& x) {
      return f.object(x).fields(f.field("tid", x.tid),
                                f.field("shard", x.shard),
                                f.field("payload", x.payload));
    }
  };

 public:
  struct AbortAllOngoingTrx {
    template<class Inspector>
    friend auto inspect(Inspector& f, AbortAllOngoingTrx& x) {
      return f.object(x).fields();
    }
  };

  struct Commit {
    TransactionId tid;

    template<class Inspector>
    friend auto inspect(Inspector& f, Commit& x) {
      return f.object(x).fields(f.field("tid", x.tid));
    }
  };

  struct IntermediateCommit {
    TransactionId tid;

    template<class Inspector>
    friend auto inspect(Inspector& f, IntermediateCommit& x) {
      return f.object(x).fields(f.field("tid", x.tid));
    }
  };

  struct Abort {
    TransactionId tid;

    template<class Inspector>
    friend auto inspect(Inspector& f, Abort& x) {
      return f.object(x).fields(f.field("tid", x.tid));
    }
  };

  struct Truncate {
    TransactionId tid;
    ShardID shard;

    template<class Inspector>
    friend auto inspect(Inspector& f, Truncate& x) {
      return f.object(x).fields(f.field("tid", x.tid),
                                f.field("shard", x.shard));
    }
  };

  struct CreateShard {
    ShardID shard;
    CollectionID collection;
    std::shared_ptr<VPackBuilder> properties;

    template<class Inspector>
    friend auto inspect(Inspector& f, CreateShard& x) {
      return f.object(x).fields(f.field("shard", x.shard),
                                f.field("collection", x.collection),
                                f.field("properties", x.properties));
    }
  };

  struct DropShard {
    ShardID shard;
    CollectionID collection;

    template<class Inspector>
    friend auto inspect(Inspector& f, DropShard& x) {
      return f.object(x).fields(f.field("shard", x.shard),
                                f.field("collection", x.collection));
    }
  };

  struct Insert : DocumentOperation {
    template<class Inspector>
    friend auto inspect(Inspector& f, Insert& x) {
      return f.object(x).fields(f.template embedFields<DocumentOperation>(x));
    }
  };

  struct Update : DocumentOperation {
    template<class Inspector>
    friend auto inspect(Inspector& f, Update& x) {
      return f.object(x).fields(f.template embedFields<DocumentOperation>(x));
    }
  };

  struct Replace : DocumentOperation {
    template<class Inspector>
    friend auto inspect(Inspector& f, Replace& x) {
      return f.object(x).fields(f.template embedFields<DocumentOperation>(x));
    }
  };

  struct Remove : DocumentOperation {
    template<class Inspector>
    friend auto inspect(Inspector& f, Remove& x) {
      return f.object(x).fields(f.template embedFields<DocumentOperation>(x));
    }
  };

 public:
  using OperationType =
      std::variant<AbortAllOngoingTrx, Commit, IntermediateCommit, Abort,
                   Truncate, CreateShard, DropShard, Insert, Update, Replace,
                   Remove>;
  OperationType operation;

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

  template<typename Inspector>
  friend auto inspect(Inspector& f, ReplicatedOperation& x) {
    return f.variant(x.operation)
        .embedded("type")
        .alternatives(
            inspection::type<AbortAllOngoingTrx>("AbortAllOngoingTrx"),
            inspection::type<Commit>("Commit"),
            inspection::type<IntermediateCommit>("IntermediateCommit"),
            inspection::type<Abort>("Abort"),
            inspection::type<Truncate>("Truncate"),
            inspection::type<CreateShard>("CreateShard"),
            inspection::type<DropShard>("DropShard"),
            inspection::type<Insert>("Insert"),
            inspection::type<Update>("Update"),
            inspection::type<Replace>("Replace"),
            inspection::type<Remove>("Remove"));
  }

 private:
  template<typename... Args>
  explicit ReplicatedOperation(std::in_place_t, Args&&... args) noexcept;
};

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
concept UserTransaction =
    ModifiesUserTransaction<T> || FinishesUserTransaction<T> ||
    std::is_same_v<T, ReplicatedOperation::IntermediateCommit>;

template<typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

template<class... T>
constexpr bool always_false_v = false;

auto operator<<(std::ostream&, ReplicatedOperation const&) -> std::ostream&;
}  // namespace arangodb::replication2::replicated_state::document

template<>
struct fmt::formatter<
    arangodb::replication2::replicated_state::document::ReplicatedOperation>
    : arangodb::inspection::inspection_formatter {};
