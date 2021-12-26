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
#include <memory>

#include "Basics/Guarded.h"
#include "Basics/Result.h"
#include "Basics/UnshackledMutex.h"
#include "Futures/Future.h"

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/types.h"

namespace arangodb::replication2 {
namespace replicated_log {
struct ReplicatedLog;
}

namespace replicated_state {

template<typename T>
struct AbstractStateMachine
    : std::enable_shared_from_this<AbstractStateMachine<T>> {
  // TODO Maybe we can create a non-templated base class for functions that do
  // not
  //      require the template parameter. (waitFor, pollEntries, ...)
  using LogIterator = TypedLogIterator<T>;
  using LogRangeIterator = TypedLogRangeIterator<T>;

  virtual ~AbstractStateMachine() = default;

  explicit AbstractStateMachine(
      std::shared_ptr<arangodb::replication2::replicated_log::ReplicatedLog>
          log);
  auto triggerPollEntries() -> futures::Future<Result>;

 protected:
  virtual auto installSnapshot(ParticipantId const&)
      -> futures::Future<Result> = 0;
  virtual auto applyEntries(std::unique_ptr<LogRangeIterator>)
      -> futures::Future<Result> = 0;

  void releaseIndex(LogIndex);
  auto getEntry(LogIndex) -> std::optional<T>;
  auto getIterator(LogIndex first) -> std::unique_ptr<LogIterator>;
  auto insert(T const&) -> LogIndex;
  auto waitFor(LogIndex)
      -> futures::Future<replication2::replicated_log::WaitForResult>;

 private:
  struct GuardedData {
    bool pollOnGoing{false};
    LogIndex nextIndex{1};
  };

  Guarded<GuardedData, basics::UnshackledMutex> _guardedData;
  std::shared_ptr<arangodb::replication2::replicated_log::ReplicatedLog> const
      log;
};

}  // namespace replicated_state
}  // namespace arangodb::replication2
