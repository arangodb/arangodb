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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include <Basics/voc-errors.h>
#include <Futures/Future.h>

#include "BlackHoleStateMachine.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::black_hole;

auto BlackHoleLeaderState::recoverEntries(std::unique_ptr<EntryIterator> ptr)
    -> futures::Future<Result> {
  return {TRI_ERROR_NO_ERROR};
}

auto BlackHoleLeaderState::write(std::string_view data) -> LogIndex {
  BlackHoleLogEntry entry{.value = std::string{data}};
  return getStream()->insert(entry);
}

BlackHoleLeaderState::BlackHoleLeaderState(std::unique_ptr<BlackHoleCore> core)
    : _core(std::move(core)) {}

auto BlackHoleLeaderState::resign() && noexcept
    -> std::unique_ptr<BlackHoleCore> {
  return std::move(_core);
}

auto BlackHoleFollowerState::acquireSnapshot(ParticipantId const& destination,
                                             LogIndex) noexcept
    -> futures::Future<Result> {
  return {TRI_ERROR_NO_ERROR};
}

auto BlackHoleFollowerState::applyEntries(
    std::unique_ptr<EntryIterator> ptr) noexcept -> futures::Future<Result> {
  return {TRI_ERROR_NO_ERROR};
}

BlackHoleFollowerState::BlackHoleFollowerState(
    std::unique_ptr<BlackHoleCore> core)
    : _core(std::move(core)) {}
auto BlackHoleFollowerState::resign() && noexcept
    -> std::unique_ptr<BlackHoleCore> {
  return std::move(_core);
}

auto BlackHoleFactory::constructFollower(std::unique_ptr<BlackHoleCore> core)
    -> std::shared_ptr<BlackHoleFollowerState> {
  return std::make_shared<BlackHoleFollowerState>(std::move(core));
}

auto BlackHoleFactory::constructLeader(std::unique_ptr<BlackHoleCore> core)
    -> std::shared_ptr<BlackHoleLeaderState> {
  return std::make_shared<BlackHoleLeaderState>(std::move(core));
}

auto BlackHoleFactory::constructCore(GlobalLogIdentifier const&)
    -> std::unique_ptr<BlackHoleCore> {
  return std::make_unique<BlackHoleCore>();
}

auto replicated_state::EntryDeserializer<
    replicated_state::black_hole::BlackHoleLogEntry>::
operator()(
    streams::serializer_tag_t<replicated_state::black_hole::BlackHoleLogEntry>,
    velocypack::Slice s) const
    -> replicated_state::black_hole::BlackHoleLogEntry {
  return replicated_state::black_hole::BlackHoleLogEntry{s.copyString()};
}

void replicated_state::EntrySerializer<
    replicated_state::black_hole::BlackHoleLogEntry>::
operator()(
    streams::serializer_tag_t<replicated_state::black_hole::BlackHoleLogEntry>,
    black_hole::BlackHoleLogEntry const& e, velocypack::Builder& b) const {
  b.add(velocypack::Value(e.value));
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

template struct replicated_state::ReplicatedState<BlackHoleState>;
