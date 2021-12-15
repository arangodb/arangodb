////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Futures/Future.h"

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/Streams/Streams.h"

namespace arangodb::replication2::test {
template <typename S>
struct EmptyFollowerType : replicated_state::ReplicatedFollowerState<S> {
  using EntryType = typename replicated_state::ReplicatedStateTraits<S>::EntryType;
  using Stream = streams::Stream<EntryType>;
  using EntryIterator = typename Stream::Iterator;

 protected:
  auto applyEntries(std::unique_ptr<EntryIterator>) noexcept
      -> futures::Future<Result> override {
    return futures::Future<Result>{std::in_place};
  }
  auto acquireSnapshot(ParticipantId const&, LogIndex) noexcept
      -> futures::Future<Result> override {
    return futures::Future<Result>{std::in_place};
  }
};

template<typename S>
struct EmptyLeaderType : replicated_state::ReplicatedLeaderState<S> {
  using EntryType = typename replicated_state::ReplicatedStateTraits<S>::EntryType;
  using Stream = streams::Stream<EntryType>;
  using EntryIterator = typename Stream::Iterator;
 protected:
  auto recoverEntries(std::unique_ptr<EntryIterator>) -> futures::Future<Result> override {
    return futures::Future<Result>(std::in_place);
  }
};
}
