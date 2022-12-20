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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Cluster/CallbackGuard.h"
#include "Cluster/ClusterTypes.h"
#include "Containers/FlatHashMap.h"

#include <string>
#include <function2.hpp>
#include <map>
#include <memory>
#include <type_traits>
#include <mutex>

namespace arangodb {

class SupervisedScheduler;

namespace velocypack {

class Builder;
class Slice;

}  // namespace velocypack
namespace cluster {

// Note:
// Instances of this class must be destructed during shutdown before the
// scheduler is destroyed.
class RebootTracker {
 public:
  using SchedulerPointer = SupervisedScheduler*;
  using Callback = fu2::unique_function<void()>;
  struct DescriptedCallback {
    Callback callback;
    std::string description;
  };
  using State = containers::FlatHashMap<ServerID, RebootId>;
  using CallbackId = uint64_t;
  using RebootIds =
      std::map<RebootId,
               containers::FlatHashMap<CallbackId, DescriptedCallback>>;
  using Callbacks = containers::FlatHashMap<ServerID, RebootIds>;
  struct PeerState {
    std::string serverId;
    RebootId rebootId{0};
  };

  explicit RebootTracker(SchedulerPointer scheduler);

  CallbackGuard callMeOnChange(PeerState peer, Callback callback,
                               std::string description);

  void updateServerState(State state);

 private:
  void unregisterCallback(PeerState const& peer,
                          CallbackId callbackId) noexcept;

  void queueCallback(DescriptedCallback&& callback) noexcept;
  void queueCallbacks(std::string_view serverId, RebootId to);

  mutable std::mutex _mutex;

  CallbackId _nextCallbackId{1};

  /// @brief Save a pointer to the scheduler for easier testing
  SchedulerPointer _scheduler;

  /// @brief Last known rebootId of every server.
  /// Will regularly get updates from the agency.
  /// Updates may not be applied if scheduling the affected callbacks fails, so
  /// the scheduling will be tried again on the next update.
  State _state;

  /// @brief List of registered callbacks per server.
  /// Needs to fulfill the following:
  ///  - All ServerIDs in this map must always exist in _state
  ///    (though not necessarily the other way round).
  ///  - None of the nested maps is empty
  ///  - The RebootIds used as index in the inner map are expected to not be
  ///    smaller than the corresponding ones in _state
  Callbacks _callbacks;
};

}  // namespace cluster
}  // namespace arangodb
