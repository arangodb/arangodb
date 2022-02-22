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
#include "Basics/UnshackledMutex.h"

#include <immer/map.hpp>

#include <optional>
#include <string>
#include <variant>
#include <unordered_map>

namespace arangodb::replication2::replicated_state {
/**
 * This prototype state machine acts as a simple key value store. It is meant to
 * be used during integration tests.
 * Doesn't deal with persisting data.
 * Snapshot transfers are supported.
 */
namespace prototype {

struct PrototypeFactory;
struct PrototypeLogEntry;
struct PrototypeLeaderState;
struct PrototypeFollowerState;
struct PrototypeCore;

struct PrototypeState {
  using LeaderType = PrototypeLeaderState;
  using FollowerType = PrototypeFollowerState;
  using EntryType = PrototypeLogEntry;
  using FactoryType = PrototypeFactory;
  using CoreType = PrototypeCore;
};

struct PrototypeLogEntry {
  struct InsertOperation {
    std::string key, value;
  };
  /*
  struct InsertBulkOperation {
    std::unordered_map<std::string, std::string> map;
  };
   */
  struct DeleteOperation {
    std::string key;
  };

  std::variant<InsertOperation, DeleteOperation> operation;
};

struct PrototypeCore {
  using StorageType = ::immer::map<std::string, std::string>;
  StorageType store;

  template<typename EntryIterator>
  void applyEntries(std::unique_ptr<EntryIterator> ptr);
};

struct PrototypeLeaderState
    : IReplicatedLeaderState<PrototypeState>,
      std::enable_shared_from_this<PrototypeLeaderState> {
  explicit PrototypeLeaderState(std::unique_ptr<PrototypeCore> core);

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<PrototypeCore> override;

  auto recoverEntries(std::unique_ptr<EntryIterator> ptr)
      -> futures::Future<Result> override;

  auto set(std::string key, std::string value) -> futures::Future<Result>;
  // auto set(PrototypeCore::StorageType map) -> futures::Future<Result>;
  auto remove(std::string key) -> futures::Future<Result>;
  auto get(std::string key) -> std::optional<std::string>;

  template<typename Iterator, typename std::enable_if_t<std::is_same_v<
                                  typename Iterator::value_type, std::string>>>
  auto get(Iterator const& begin, Iterator const& end)
      -> PrototypeCore::StorageType;

  Guarded<std::unique_ptr<PrototypeCore>, basics::UnshackledMutex> guardedData;
};

struct PrototypeFollowerState : IReplicatedFollowerState<PrototypeState> {
  explicit PrototypeFollowerState(std::unique_ptr<PrototypeCore> core);

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<PrototypeCore> override;

  auto acquireSnapshot(ParticipantId const& destination, LogIndex) noexcept
      -> futures::Future<Result> override;

  auto applyEntries(std::unique_ptr<EntryIterator> ptr) noexcept
      -> futures::Future<Result> override;

  Guarded<std::unique_ptr<PrototypeCore>, basics::UnshackledMutex> guardedData;
};

struct PrototypeFactory {
  auto constructFollower(std::unique_ptr<PrototypeCore> core)
      -> std::shared_ptr<PrototypeFollowerState>;
  auto constructLeader(std::unique_ptr<PrototypeCore> core)
      -> std::shared_ptr<PrototypeLeaderState>;
};

}  // namespace prototype

template<>
struct EntryDeserializer<prototype::PrototypeLogEntry> {
  auto operator()(streams::serializer_tag_t<prototype::PrototypeLogEntry>,
                  velocypack::Slice s) const -> prototype::PrototypeLogEntry;
};

template<>
struct EntrySerializer<prototype::PrototypeLogEntry> {
  void operator()(streams::serializer_tag_t<prototype::PrototypeLogEntry>,
                  prototype::PrototypeLogEntry const& e,
                  velocypack::Builder& b) const;
};

extern template struct replicated_state::ReplicatedState<
    prototype::PrototypeState>;
}  // namespace arangodb::replication2::replicated_state
