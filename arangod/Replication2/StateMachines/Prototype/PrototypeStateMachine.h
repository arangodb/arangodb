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

#include "Basics/ResultT.h"
#include "Basics/UnshackledMutex.h"
#include "Futures/Future.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/StateInterfaces.h"

#include <string>
#include <unordered_map>

namespace rocksdb {
class TransactionDB;
}

namespace arangodb::replication2::replicated_state {
/**
 * This prototype state machine acts as a simple key value store. It is meant to
 * be used during integration tests.
 * Data is persisted.
 * Snapshot transfers are supported.
 */
namespace prototype {

struct PrototypeFactory;
struct PrototypeLogEntry;
struct PrototypeLeaderState;
struct PrototypeFollowerState;
struct PrototypeCore;
struct PrototypeDump;

struct PrototypeState {
  using LeaderType = PrototypeLeaderState;
  using FollowerType = PrototypeFollowerState;
  using EntryType = PrototypeLogEntry;
  using FactoryType = PrototypeFactory;
  using CoreType = PrototypeCore;
  using CoreParameterType = void;
};

struct IPrototypeLeaderInterface {
  virtual ~IPrototypeLeaderInterface() = default;
  virtual auto getSnapshot(GlobalLogIdentifier const& logId,
                           LogIndex waitForIndex)
      -> futures::Future<
          ResultT<std::unordered_map<std::string, std::string>>> = 0;
};

struct IPrototypeNetworkInterface {
  virtual ~IPrototypeNetworkInterface() = default;
  virtual auto getLeaderInterface(ParticipantId id)
      -> ResultT<std::shared_ptr<IPrototypeLeaderInterface>> = 0;
};

struct IPrototypeStorageInterface {
  virtual ~IPrototypeStorageInterface() = default;
  virtual auto put(GlobalLogIdentifier const& logId, PrototypeDump dump)
      -> Result = 0;
  virtual auto get(GlobalLogIdentifier const& logId)
      -> ResultT<PrototypeDump> = 0;
};

struct PrototypeFactory {
  explicit PrototypeFactory(
      std::shared_ptr<IPrototypeNetworkInterface> networkInterface,
      std::shared_ptr<IPrototypeStorageInterface> storageInterface);

  auto constructFollower(std::unique_ptr<PrototypeCore> core)
      -> std::shared_ptr<PrototypeFollowerState>;
  auto constructLeader(std::unique_ptr<PrototypeCore> core)
      -> std::shared_ptr<PrototypeLeaderState>;
  auto constructCore(GlobalLogIdentifier const&)
      -> std::unique_ptr<PrototypeCore>;

  std::shared_ptr<IPrototypeNetworkInterface> const networkInterface;
  std::shared_ptr<IPrototypeStorageInterface> const storageInterface;
};
}  // namespace prototype

extern template struct replicated_state::ReplicatedState<
    prototype::PrototypeState>;
}  // namespace arangodb::replication2::replicated_state
