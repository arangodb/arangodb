////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"
#include "Replication2/Streams/Streams.h"

#include "Basics/Guarded.h"

namespace arangodb {

namespace futures {
template<typename T>
class Future;
}  // namespace futures

}  // namespace arangodb

namespace arangodb::replication2::replicated_state {

// The streams for follower states use IReplicatedLogFollowerMethods, while the
// leader ones use IReplicatedLogLeaderMethods.
template<class ILogMethodsT>
concept ValidStreamLogMethods =
    std::is_same_v<ILogMethodsT,
                   replicated_log::IReplicatedLogFollowerMethods> ||
    std::is_same_v<ILogMethodsT, replicated_log::IReplicatedLogLeaderMethods>;

// TODO Clean this up, starting with trimming Stream to its minimum
template<typename S, template<typename> typename Interface = streams::Stream,
         ValidStreamLogMethods ILogMethodsT =
             replicated_log::IReplicatedLogFollowerMethods>
struct StreamProxy : Interface<typename ReplicatedStateTraits<S>::EntryType> {
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Deserializer = typename ReplicatedStateTraits<S>::Deserializer;
  using WaitForResult = typename streams::Stream<EntryType>::WaitForResult;
  using Iterator = typename streams::Stream<EntryType>::Iterator;

 protected:
  Guarded<std::unique_ptr<ILogMethodsT>> _logMethods;

 public:
  explicit StreamProxy(std::unique_ptr<ILogMethodsT> methods);

  struct MethodsGuard {
    using Guard =
        typename Guarded<std::unique_ptr<ILogMethodsT>>::mutex_guard_type;
    auto operator->() noexcept -> ILogMethodsT* { return guard.get().get(); }
    auto operator*() noexcept -> ILogMethodsT& { return *guard.get().get(); }
    explicit MethodsGuard(Guard&& guard) : guard(std::move(guard)) {}
    auto isResigned() const noexcept -> bool { return guard.get() == nullptr; }

   private:
    Guard guard;
  };

  auto methods() -> MethodsGuard;

  auto resign() && -> std::unique_ptr<ILogMethodsT>;

  auto isResigned() const noexcept -> bool;

  auto waitFor(LogIndex index) -> futures::Future<WaitForResult> override;
  auto waitForIterator(LogIndex index)
      -> futures::Future<std::unique_ptr<Iterator>> override;

  auto release(LogIndex index) -> void override;

 protected:
  [[noreturn]] static void throwResignedException();
};

template<typename S>
struct ProducerStreamProxy
    : StreamProxy<S, streams::ProducerStream,
                  replicated_log::IReplicatedLogLeaderMethods> {
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Deserializer = typename ReplicatedStateTraits<S>::Deserializer;
  using Serializer = typename ReplicatedStateTraits<S>::Serializer;
  explicit ProducerStreamProxy(
      std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> methods);

  auto insert(EntryType const& v, bool waitForSync) -> LogIndex override;

 private:
  auto serialize(EntryType const& v) -> LogPayload;
};

}  // namespace arangodb::replication2::replicated_state
