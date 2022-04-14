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
    std::shared_ptr<ReplicatedState<S>> const& parent,
    std::shared_ptr<replicated_log::LogUnconfiguredParticipant>
        unconfiguredParticipant,
    std::unique_ptr<CoreType> core, std::unique_ptr<ReplicatedStateToken> token)
    : _parent(parent),
      _unconfiguredParticipant(std::move(unconfiguredParticipant)),
      _core(std::move(core)),
      _token(std::move(token)) {}

template<typename S>
void UnconfiguredStateManager<S>::run() noexcept {
  _unconfiguredParticipant->waitForResign().thenFinal(
      [weak = _parent](futures::Try<futures::Unit>&& result) {
        TRI_ASSERT(result.valid());
        if (result.hasValue()) {
          if (auto self = weak.lock(); self != nullptr) {
            self->forceRebuild();
          }
        } else if (result.hasException()) {
          // This can be a FutureException(ErrorCode::BrokenPromise), or
          // TRI_ERROR_REPLICATION_REPLICATED_LOG_PARTICIPANT_GONE.
          // In either case, the ReplicatedLog itself is dropped or destroyed
          // (not just the LogParticipant instance of the current term).
          LOG_TOPIC("4ffab", TRACE, Logger::REPLICATED_STATE)
              << "Replicated log participant is gone. Replicated state will go "
                 "soon as well.";
        } else {
          TRI_ASSERT(false);
        }
      });
}

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
    -> std::tuple<std::unique_ptr<CoreType>,
                  std::unique_ptr<ReplicatedStateToken>, DeferredAction> {
  return std::make_tuple(std::move(_core), std::move(_token), DeferredAction{});
}

}  // namespace arangodb::replication2::replicated_state
