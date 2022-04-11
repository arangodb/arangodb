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
#pragma once

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/StateInterfaces.h"

namespace arangodb::replication2::replicated_state {
/**
 * The black-hole state machine is here only for testing purpose. It accepts
 * all writes. Writes are replicated and then discarded. Followers do nothing,
 * except receiving log data. Snapshot transfer is always successful.
 */
namespace black_hole {

struct BlackHoleFactory;
struct BlackHoleLogEntry;
struct BlackHoleLeaderState;
struct BlackHoleFollowerState;
struct BlackHoleCore;

struct BlackHoleState {
  using LeaderType = BlackHoleLeaderState;
  using FollowerType = BlackHoleFollowerState;
  using EntryType = BlackHoleLogEntry;
  using FactoryType = BlackHoleFactory;
  using CoreType = BlackHoleCore;
};

struct BlackHoleLogEntry {
  std::string value;
};

struct BlackHoleLeaderState
    : replicated_state::IReplicatedLeaderState<BlackHoleState> {
  explicit BlackHoleLeaderState(std::unique_ptr<BlackHoleCore> core);
  auto write(std::string_view) -> LogIndex;

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<BlackHoleCore> override;

 protected:
  auto recoverEntries(std::unique_ptr<EntryIterator> ptr)
      -> futures::Future<Result> override;

  std::unique_ptr<BlackHoleCore> _core;
};

struct BlackHoleFollowerState
    : replicated_state::IReplicatedFollowerState<BlackHoleState> {
  explicit BlackHoleFollowerState(std::unique_ptr<BlackHoleCore> core);

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<BlackHoleCore> override;

 protected:
  auto acquireSnapshot(ParticipantId const& destination, LogIndex) noexcept
      -> futures::Future<Result> override;
  auto applyEntries(std::unique_ptr<EntryIterator> ptr) noexcept
      -> futures::Future<Result> override;

  std::unique_ptr<BlackHoleCore> _core;
};

struct BlackHoleCore {};

struct BlackHoleFactory {
  auto constructFollower(std::unique_ptr<BlackHoleCore> core)
      -> std::shared_ptr<BlackHoleFollowerState>;
  auto constructLeader(std::unique_ptr<BlackHoleCore> core)
      -> std::shared_ptr<BlackHoleLeaderState>;
  auto constructCore(GlobalLogIdentifier const&)
      -> std::unique_ptr<BlackHoleCore>;
};

}  // namespace black_hole

template<>
struct EntryDeserializer<black_hole::BlackHoleLogEntry> {
  auto operator()(streams::serializer_tag_t<
                      replicated_state::black_hole::BlackHoleLogEntry>,
                  velocypack::Slice s) const
      -> replicated_state::black_hole::BlackHoleLogEntry;
};

template<>
struct EntrySerializer<black_hole::BlackHoleLogEntry> {
  void operator()(streams::serializer_tag_t<
                      replicated_state::black_hole::BlackHoleLogEntry>,
                  replicated_state::black_hole::BlackHoleLogEntry const& e,
                  velocypack::Builder& b) const;
};

extern template struct replicated_state::ReplicatedState<
    black_hole::BlackHoleState>;
}  // namespace arangodb::replication2::replicated_state
