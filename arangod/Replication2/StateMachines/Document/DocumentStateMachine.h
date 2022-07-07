////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

struct IDocumentStateAgencyHandler;
struct IDocumentStateShardHandler;

struct ReplicationOptions {
  bool waitForCommit{false};
};

struct DocumentState {
  static constexpr std::string_view NAME = "document";

  using LeaderType = DocumentLeaderState;
  using FollowerType = DocumentFollowerState;
  using EntryType = DocumentLogEntry;
  using FactoryType = DocumentFactory;
  using CoreType = DocumentCore;
  using CoreParameterType = DocumentCoreParameters;
};

struct DocumentCoreParameters {
  std::string collectionId;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, DocumentCoreParameters& p) {
    return f.object(p).fields(f.field("collectionId", p.collectionId));
  }

  auto toSharedSlice() -> velocypack::SharedSlice;
};

struct DocumentFactory {
  explicit DocumentFactory(
      std::shared_ptr<IDocumentStateAgencyHandler> agencyReader,
      std::shared_ptr<IDocumentStateShardHandler> shardHandler);

  auto constructFollower(std::unique_ptr<DocumentCore> core)
      -> std::shared_ptr<DocumentFollowerState>;
  auto constructLeader(std::unique_ptr<DocumentCore> core)
      -> std::shared_ptr<DocumentLeaderState>;
  auto constructCore(GlobalLogIdentifier, DocumentCoreParameters)
      -> std::unique_ptr<DocumentCore>;

  auto getAgencyReader() -> std::shared_ptr<IDocumentStateAgencyHandler>;
  auto getShardHandler() -> std::shared_ptr<IDocumentStateShardHandler>;

 private:
  std::shared_ptr<IDocumentStateAgencyHandler> const _agencyReader;
  std::shared_ptr<IDocumentStateShardHandler> const _shardHandler;
};
}  // namespace document

extern template struct replicated_state::ReplicatedState<
    document::DocumentState>;

}  // namespace arangodb::replication2::replicated_state

#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLeaderState.h"
