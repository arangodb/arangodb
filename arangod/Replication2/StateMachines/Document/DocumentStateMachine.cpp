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

#include <Basics/voc-errors.h>
#include <Futures/Future.h>

#include "DocumentStateMachine.h"

using namespace arangodb::replication2::replicated_state::document;

DocumentLeaderState::DocumentLeaderState(std::unique_ptr<DocumentCore> core)
    : _core(std::move(core)){};

auto DocumentLeaderState::resign() && noexcept
    -> std::unique_ptr<DocumentCore> {
  return std::move(_core);
}

auto DocumentLeaderState::recoverEntries(std::unique_ptr<EntryIterator> ptr)
    -> futures::Future<Result> {
  return {TRI_ERROR_NO_ERROR};
}

DocumentFollowerState::DocumentFollowerState(std::unique_ptr<DocumentCore> core)
    : _core(std::move(core)){};

auto DocumentFollowerState::resign() && noexcept
    -> std::unique_ptr<DocumentCore> {
  return std::move(_core);
}

auto DocumentFollowerState::acquireSnapshot(ParticipantId const& destination,
                                            LogIndex) noexcept
    -> futures::Future<Result> {
  return {TRI_ERROR_NO_ERROR};
}

auto DocumentFollowerState::applyEntries(
    std::unique_ptr<EntryIterator> ptr) noexcept -> futures::Future<Result> {
  return {TRI_ERROR_NO_ERROR};
};

auto DocumentFactory::constructFollower(std::unique_ptr<DocumentCore> core)
    -> std::shared_ptr<DocumentFollowerState> {
  return std::make_shared<DocumentFollowerState>(std::move(core));
}

auto DocumentFactory::constructLeader(std::unique_ptr<DocumentCore> core)
    -> std::shared_ptr<DocumentLeaderState> {
  return std::make_shared<DocumentLeaderState>(std::move(core));
}

auto DocumentFactory::constructCore(GlobalLogIdentifier const& gid)
    -> std::unique_ptr<DocumentCore> {
  return std::make_unique<DocumentCore>();
}

auto arangodb::replication2::replicated_state::EntryDeserializer<
    DocumentLogEntry>::operator()(streams::serializer_tag_t<DocumentLogEntry>,
                                  velocypack::Slice s) const
    -> DocumentLogEntry {
  return DocumentLogEntry{};
}

void arangodb::replication2::replicated_state::EntrySerializer<
    DocumentLogEntry>::operator()(streams::serializer_tag_t<DocumentLogEntry>,
                                  DocumentLogEntry const& e,
                                  velocypack::Builder& b) const {}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

template struct arangodb::replication2::replicated_state::ReplicatedState<
    DocumentState>;
