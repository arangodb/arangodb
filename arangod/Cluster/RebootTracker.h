////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CLUSTER_REBOOTTRACKER_H
#define ARANGOD_CLUSTER_REBOOTTRACKER_H

#include "Cluster/ClusterTypes.h"
#include "Basics/Mutex.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

#include <map>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace arangodb {
namespace cluster {

/// @brief If constructed with a callback, the given callback will be called
/// exactly once: Either during destruction, or when the object is overwritten
/// (via operator=()), or when it's explicitly cleared. It's not copyable,
/// but movable.
class CallbackGuard {
 public:
  // Calls the callback given callback upon destruction.
  // Allows only move semantics and no copy semantics.

  CallbackGuard();
  // IMPORTANT NOTE:
  // The passed callback should not throw exceptions, they will not be caught
  // here, but thrown by the destructor!
  explicit CallbackGuard(std::function<void(void)> callback);
  ~CallbackGuard();

  // Note that the move constructor of std::function is not noexcept until
  // C++20. Thus we cannot mark the constructors here noexcept.
  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  CallbackGuard(CallbackGuard&& other);
  // operator= additionally calls the _callback, and this can also throw.
  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  CallbackGuard& operator=(CallbackGuard&&);

  CallbackGuard(CallbackGuard const&) = delete;
  CallbackGuard& operator=(CallbackGuard const&) = delete;

  /// @brief Call the contained callback, then delete it.
  void callAndClear();

 private:
  void call();

  std::function<void(void)> _callback;
};

// Note:
// Instances of this class must be destructed during shutdown before the
// scheduler is destroyed.
class RebootTracker {
 public:
  using Callback = std::function<void(void)>;
  using SchedulerPointer = decltype(SchedulerFeature::SCHEDULER);
  static_assert(std::is_pointer<SchedulerPointer>::value,
                "If SCHEDULER is changed to a non-pointer type, this class "
                "might have to be adapted");
  static_assert(
      std::is_base_of<Scheduler, std::remove_pointer<SchedulerPointer>::type>::value,
      "SchedulerPointer is expected to point to an instance of Scheduler");

  class PeerState {
   public:
    PeerState(ServerID serverId, RebootId rebootId)
        : _serverId{std::move(serverId)}, _rebootId{rebootId} {}

    ServerID const& serverId() const noexcept { return _serverId; }
    RebootId rebootId() const noexcept { return _rebootId; }

   private:
    ServerID _serverId;
    RebootId _rebootId;
  };

  explicit RebootTracker(SchedulerPointer scheduler);

  CallbackGuard callMeOnChange(PeerState const& peerState, Callback callback,
                               std::string callbackDescription);

  void updateServerState(std::unordered_map<ServerID, RebootId> const& state);

 private:
  using CallbackId = uint64_t;

  struct DescriptedCallback {
    Callback callback;
    std::string description;
  };

  CallbackId getNextCallbackId() noexcept;

  void unregisterCallback(PeerState const&, CallbackId);

  // bool notifyChange(PeerState const& peerState);

  void scheduleAllCallbacksFor(ServerID const& serverId);
  void scheduleCallbacksFor(ServerID const& serverId, RebootId rebootId);

  static Callback createSchedulerCallback(
      std::vector<std::shared_ptr<std::unordered_map<CallbackId, DescriptedCallback>>> callbacks);

  void queueCallbacks(std::vector<std::shared_ptr<std::unordered_map<CallbackId, DescriptedCallback>>> callbacks);
  void queueCallback(DescriptedCallback callback);

 private:
  Mutex _mutex;

  CallbackId _nextCallbackId{1};

  /// @brief Last known rebootId of every server.
  /// Will regularly get updates from the agency.
  /// Updates may not be applied if scheduling the affected callbacks fails, so
  /// the scheduling will be tried again on the next update.
  std::unordered_map<ServerID, RebootId> _rebootIds;

  /// @brief List of registered callbacks per server.
  /// Maps (serverId, rebootId) to a set of callbacks (indexed by a CallbackId).
  /// Needs to fulfill the following:
  ///  - A callback with a given ID may never be moved to another (serverId, rebootId) entry,
  ///    to allow CallbackGuard to find a callback.
  ///  - All ServerIDs in this map must always exist in _rebootIds (though not
  ///    necessarily the other way round).
  ///  - The shared_ptr in the RebootId-indexed map must never be nullptr
  ///  - The unordered_map pointed to by the aforementioned shared_ptr must never be empty
  ///  - The RebootIds used as index in the inner map are expected to not be smaller than the corresponding ones in _rebootIds
  std::unordered_map<ServerID, std::map<RebootId, std::shared_ptr<std::unordered_map<CallbackId, DescriptedCallback>>>> _callbacks;

  /// @brief Save a pointer to the scheduler for easier testing
  SchedulerPointer _scheduler;
};

}  // namespace cluster
}  // namespace arangodb

#endif  // ARANGOD_CLUSTER_REBOOTTRACKER_H
