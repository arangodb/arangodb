////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/ReplicatedState/UnconfiguredStateManager.h"

#include "Replication2/Exceptions/ParticipantResignedException.h"

namespace arangodb::replication2::replicated_state {

template<typename S>
UnconfiguredStateManager<S>::UnconfiguredStateManager(
    std::unique_ptr<CoreType> core, std::unique_ptr<ReplicatedStateToken> token)
    : _core(std::move(core)), _token(std::move(token)) {}

template<typename S>
auto UnconfiguredStateManager<S>::getStatus() const -> StateStatus {
  if (_core == nullptr || _token == nullptr) {
    TRI_ASSERT(_core == nullptr && _token == nullptr);
    throw replicated_log::ParticipantResignedException(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_PARTICIPANT_GONE, ADB_HERE);
  }
  UnconfiguredStatus status;
  status.snapshot = _token->snapshot;
  status.generation = _token->generation;
  return StateStatus{.variant = std::move(status)};
}

template<typename S>
auto UnconfiguredStateManager<S>::resign() && noexcept
    -> std::pair<std::unique_ptr<CoreType>,
                 std::unique_ptr<ReplicatedStateToken>> {
  return {std::move(_core), std::move(_token)};
}

}  // namespace arangodb::replication2::replicated_state
