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

#include "PrototypeStateMachine.h"

namespace arangodb::replication2::replicated_state::prototype {
struct PrototypeFollowerState
    : IReplicatedFollowerState<PrototypeState>,
      std::enable_shared_from_this<PrototypeFollowerState> {
  explicit PrototypeFollowerState(std::unique_ptr<PrototypeCore>,
                                  std::shared_ptr<IPrototypeNetworkInterface>);

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<PrototypeCore> override;

  auto acquireSnapshot(ParticipantId const& destination, LogIndex) noexcept
      -> futures::Future<Result> override;

  auto applyEntries(std::unique_ptr<EntryIterator> ptr) noexcept
      -> futures::Future<Result> override;

  auto get(std::string key, LogIndex waitForIndex)
      -> futures::Future<ResultT<std::optional<std::string>>>;
  auto get(std::vector<std::string> keys, LogIndex waitForIndex)
      -> futures::Future<ResultT<std::unordered_map<std::string, std::string>>>;

  LoggerContext const loggerContext;

 private:
  GlobalLogIdentifier const _logIdentifier;
  std::shared_ptr<IPrototypeNetworkInterface> const _networkInterface;
  Guarded<std::unique_ptr<PrototypeCore>, basics::UnshackledMutex> _guardedData;
};
}  // namespace arangodb::replication2::replicated_state::prototype
