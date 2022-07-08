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

struct DocumentState {
  using LeaderType = DocumentLeaderState;
  using FollowerType = DocumentFollowerState;
  using EntryType = DocumentLogEntry;
  using FactoryType = DocumentFactory;
  using CoreType = DocumentCore;
};

/* Empty for now */
struct DocumentLogEntry {};
struct DocumentCore {};

struct DocumentLeaderState
    : replicated_state::IReplicatedLeaderState<DocumentState> {
  explicit DocumentLeaderState(std::unique_ptr<DocumentCore> core);

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<DocumentCore> override;

  auto recoverEntries(std::unique_ptr<EntryIterator> ptr)
      -> futures::Future<Result> override;

  std::unique_ptr<DocumentCore> _core;
};

struct DocumentFollowerState
    : replicated_state::IReplicatedFollowerState<DocumentState> {
  explicit DocumentFollowerState(std::unique_ptr<DocumentCore> core);

 protected:
  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<DocumentCore> override;
  auto acquireSnapshot(ParticipantId const& destination, LogIndex) noexcept
      -> futures::Future<Result> override;
  auto applyEntries(std::unique_ptr<EntryIterator> ptr) noexcept
      -> futures::Future<Result> override;

  std::unique_ptr<DocumentCore> _core;
};

struct DocumentFactory {
  auto constructFollower(std::unique_ptr<DocumentCore> core)
      -> std::shared_ptr<DocumentFollowerState>;
  auto constructLeader(std::unique_ptr<DocumentCore> core)
      -> std::shared_ptr<DocumentLeaderState>;
  auto constructCore(GlobalLogIdentifier const&)
      -> std::unique_ptr<DocumentCore>;
};
}  // namespace document

template<>
struct EntryDeserializer<document::DocumentLogEntry> {
  auto operator()(
      streams::serializer_tag_t<replicated_state::document::DocumentLogEntry>,
      velocypack::Slice s) const
      -> replicated_state::document::DocumentLogEntry;
};

template<>
struct EntrySerializer<document::DocumentLogEntry> {
  void operator()(
      streams::serializer_tag_t<replicated_state::document::DocumentLogEntry>,
      replicated_state::document::DocumentLogEntry const& e,
      velocypack::Builder& b) const;
};

extern template struct replicated_state::ReplicatedState<
    document::DocumentState>;

}  // namespace arangodb::replication2::replicated_state
