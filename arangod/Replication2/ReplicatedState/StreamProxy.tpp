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

#include "StreamProxy.h"

#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedState/LazyDeserializingIterator.h"
#include "Replication2/Streams/StreamSpecification.h"

namespace arangodb::replication2::replicated_state {

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
StreamProxy<S, Interface, ILogMethodsT>::StreamProxy(
    std::unique_ptr<ILogMethodsT> methods)
    : _logMethods(std::move(methods)) {}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
auto StreamProxy<S, Interface, ILogMethodsT>::methods() -> MethodsGuard {
  return MethodsGuard{this->_logMethods.getLockedGuard()};
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
auto StreamProxy<S, Interface,
                 ILogMethodsT>::resign() && -> std::unique_ptr<ILogMethodsT> {
  // cppcheck-suppress returnStdMoveLocal ; bogus
  return std::move(this->_logMethods.getLockedGuard().get());
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
auto StreamProxy<S, Interface, ILogMethodsT>::isResigned() const noexcept
    -> bool {
  return _logMethods.getLockedGuard().get() == nullptr;
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
auto StreamProxy<S, Interface, ILogMethodsT>::waitFor(LogIndex index)
    -> futures::Future<WaitForResult> {
  // TODO We might want to remove waitFor here, in favor of updateCommitIndex.
  //      It's currently used by DocumentLeaderState::replicateOperation.
  //      If this is removed, delete it also in streams::Stream.
  auto guard = _logMethods.getLockedGuard();
  return guard.get()->waitFor(index).thenValue(
      [](auto const&) { return WaitForResult(); });
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
auto StreamProxy<S, Interface, ILogMethodsT>::waitForIterator(LogIndex index)
    -> futures::Future<std::unique_ptr<Iterator>> {
  // TODO As far as I can tell right now, we can get rid of this, but for the
  //      PrototypeState (currently). So:
  //      Delete this, also in streams::Stream.
  auto guard = _logMethods.getLockedGuard();
  return guard.get()->waitForIterator(index).thenValue([](auto&& logIter) {
    std::unique_ptr<Iterator> deserializedIter =
        std::make_unique<LazyDeserializingIterator<EntryType, Deserializer>>(
            std::move(logIter));
    return deserializedIter;
  });
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
auto StreamProxy<S, Interface, ILogMethodsT>::release(LogIndex index) -> void {
  auto guard = _logMethods.getLockedGuard();
  if (auto& methods = guard.get(); methods != nullptr) [[likely]] {
    methods->releaseIndex(index);
  } else {
    throwResignedException();
  }
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
void StreamProxy<S, Interface, ILogMethodsT>::throwResignedException() {
  static constexpr auto errorCode = ([]() consteval->ErrorCode {
    if constexpr (std::is_same_v<ILogMethodsT,
                                 replicated_log::IReplicatedLogLeaderMethods>) {
      return TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED;
    } else if constexpr (std::is_same_v<
                             ILogMethodsT,
                             replicated_log::IReplicatedLogFollowerMethods>) {
      return TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED;
    } else {
      []<bool flag = false>() {
        static_assert(flag,
                      "Unhandled log methods. This should have been "
                      "prevented by the concept ValidStreamLogMethods.");
      }
      ();
      ADB_PROD_ASSERT(false);
    }
  })();

  // The DocumentStates do not synchronize calls to `release()` or `insert()` on
  // the stream with resigning (see
  // https://github.com/arangodb/arangodb/pull/17850).
  // It relies on the stream throwing an exception in that case.
  throw replicated_log::ParticipantResignedException(errorCode, ADB_HERE);
}

template<typename S>
ProducerStreamProxy<S>::ProducerStreamProxy(
    std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> methods)
    : StreamProxy<S, streams::ProducerStream,
                  replicated_log::IReplicatedLogLeaderMethods>(
          std::move(methods)) {
  // this produces a lock inversion
  // ADB_PROD_ASSERT(this->_logMethods.getLockedGuard().get() != nullptr);
}

template<typename S>
auto ProducerStreamProxy<S>::insert(const EntryType& v) -> LogIndex {
  auto guard = this->_logMethods.getLockedGuard();
  if (auto& methods = guard.get(); methods != nullptr) [[likely]] {
    return methods->insert(serialize(v));
  } else {
    this->throwResignedException();
  }
}

template<typename S>
auto ProducerStreamProxy<S>::serialize(const EntryType& v) -> LogPayload {
  auto builder = velocypack::Builder();
  std::invoke(Serializer{}, streams::serializer_tag<EntryType>, v, builder);
  return LogPayload{*builder.steal()};
}

}  // namespace arangodb::replication2::replicated_state
