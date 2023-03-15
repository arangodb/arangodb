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

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/StateInterfaces.h"

#include <memory>

namespace arangodb::transaction {
struct IManager;
}

namespace arangodb::replication2::replicated_state {
/**
 * The Document State Machine is used as a middle-man between a shard and a
 * replicated log, inside collections from databases configured with
 * Replication2.
 */
namespace document {

struct DocumentFactory;
struct DocumentLogEntry;
struct DocumentLeaderState;
struct DocumentFollowerState;
struct DocumentCore;
struct DocumentCoreParameters;

struct IDocumentStateHandlersFactory;
struct IDocumentStateShardHandler;

struct ReplicationOptions {
  bool waitForCommit{false};
};

struct DocumentCleanupHandler {
  void drop(std::unique_ptr<DocumentCore>);
};

struct DocumentState {
  static constexpr std::string_view NAME = "document";

  using LeaderType = DocumentLeaderState;
  using FollowerType = DocumentFollowerState;
  using EntryType = DocumentLogEntry;
  using FactoryType = DocumentFactory;
  using CoreType = DocumentCore;
  using CoreParameterType = DocumentCoreParameters;
  using CleanupHandlerType = DocumentCleanupHandler;
};

struct DocumentCoreParameters {
  std::string databaseName;
  std::uint64_t groupId;  // TODO use CollectionGroupId type
  std::size_t shardSheafIndex;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, DocumentCoreParameters& p) {
    return f.object(p).fields(f.field("databaseName", p.databaseName),
                              f.field("groupId", p.groupId),
                              f.field("shardSheafIndex", p.shardSheafIndex));
  }

  [[nodiscard]] auto toSharedSlice() const -> velocypack::SharedSlice;
};

struct DocumentFactory {
  explicit DocumentFactory(
      std::shared_ptr<IDocumentStateHandlersFactory> handlersFactory,
      transaction::IManager& transactionManager);

  auto constructFollower(std::unique_ptr<DocumentCore> core)
      -> std::shared_ptr<DocumentFollowerState>;

  auto constructLeader(std::unique_ptr<DocumentCore> core)
      -> std::shared_ptr<DocumentLeaderState>;

  auto constructCore(TRI_vocbase_t&, GlobalLogIdentifier,
                     DocumentCoreParameters) -> std::unique_ptr<DocumentCore>;

  auto constructCleanupHandler() -> std::shared_ptr<DocumentCleanupHandler>;

 private:
  std::shared_ptr<IDocumentStateHandlersFactory> const _handlersFactory;
  transaction::IManager& _transactionManager;
};
}  // namespace document

extern template struct replicated_state::ReplicatedState<
    document::DocumentState>;

}  // namespace arangodb::replication2::replicated_state

#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLeaderState.h"
