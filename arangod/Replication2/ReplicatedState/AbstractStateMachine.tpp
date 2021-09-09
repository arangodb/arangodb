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

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>

#include <Basics/Exceptions.h>
#include <Replication2/ReplicatedLog/ReplicatedLog.h>
#include "AbstractStateMachine.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"

using namespace arangodb;
using namespace arangodb::replication2;

template <typename T>
auto replicated_state::AbstractStateMachine<T>::getIterator(LogIndex first)
    -> std::unique_ptr<LogIterator> {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

template <typename T>
auto replicated_state::AbstractStateMachine<T>::getEntry(LogIndex)
    -> std::optional<T> {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

template <typename T>
auto replicated_state::AbstractStateMachine<T>::insert(T const& v) -> LogIndex {
  velocypack::UInt8Buffer payload;
  {
    velocypack::Builder builder(payload);
    v.toVelocyPack(builder);
  }
  return log->getLeader()->insert(LogPayload(std::move(payload)));
}

template <typename T>
auto replication2::replicated_state::AbstractStateMachine<T>::waitFor(LogIndex idx)
    -> futures::Future<replicated_log::WaitForResult> {
  return log->getParticipant()->waitFor(idx);
}

template <typename T>
void replicated_state::AbstractStateMachine<T>::releaseIndex(LogIndex) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

namespace {
template <typename T>
struct DeserializeLogIterator : TypedLogRangeIterator<T> {
  explicit DeserializeLogIterator(std::unique_ptr<replication2::LogRangeIterator> base)
      : base(std::move(base)) {}

  auto next() -> std::optional<T> override {
    if (auto entry = base->next(); entry.has_value()) {
      return T::fromVelocyPack(entry->logPayload());
    }

    return std::nullopt;
  }

  auto range() const noexcept -> LogRange override {
    return base->range();
  }

  std::unique_ptr<replication2::LogRangeIterator> base;
};
}  // namespace

template <typename T>
auto replicated_state::AbstractStateMachine<T>::triggerPollEntries()
    -> futures::Future<Result> {
  auto nextIndex =
      _guardedData.doUnderLock([&](GuardedData& guard) -> std::optional<LogIndex> {
        if (guard.pollOnGoing) {
          return std::nullopt;
        }

        guard.pollOnGoing = true;
        return guard.nextIndex;
      });

  if (nextIndex.has_value()) {
    return log->getParticipant()
        ->waitForIterator(*nextIndex)
        .thenValue([weak = this->weak_from_this()](
                       std::unique_ptr<replication2::LogRangeIterator> res) {
          if (auto self = weak.lock()) {
            auto [from, to] = res->range();  // [from, to)
            TRI_ASSERT(from != to);

            auto iter = std::make_unique<DeserializeLogIterator<T>>(std::move(res));
            return self->applyEntries(std::move(iter)).thenValue([self, to = to](Result&& result) {
              auto guard = self->_guardedData.getLockedGuard();
              guard->pollOnGoing = false;
              TRI_ASSERT(to > guard->nextIndex);
              guard->nextIndex = to;
              return std::move(result);
            });
          }

          return futures::Future<Result>{TRI_ERROR_NO_ERROR};
        });
  }

  return futures::Future<Result>{TRI_ERROR_NO_ERROR};
}

template <typename T>
replicated_state::AbstractStateMachine<T>::AbstractStateMachine(
    std::shared_ptr<replicated_log::ReplicatedLog> log)
    : log(std::move(log)) {}
