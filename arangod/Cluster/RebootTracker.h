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

#include "ClusterInfo.h"

#include <map>
#include <unordered_map>
#include <vector>

namespace arangodb {
namespace cluster {

class RebootTracker {
 public:
  // TODO Maybe pass information about the change(s) to the callback - is there
  //      anything useful?
  using Callback = std::function<void(void)>;

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

  void callMeOnChange(PeerState const& peerState, Callback callback,
                      std::string const& callbackDescription);

  void notifyChanges(std::vector<PeerState> const& peerStates);

 private:
  bool notifyChange(PeerState const& peerState);

  bool tryScheduleCallbacksFor(ServerID const& serverId);

  static Callback createSchedulerCallback(std::shared_ptr<std::vector<Callback>> callbacks);

  static bool queueCallbacks(std::shared_ptr<std::vector<RebootTracker::Callback>> callbacks);

 private:
  // using CallbackId = uint64_t;
  // struct CallbackInfo {
  //   CallbackId id;
  //   Callback callback;
  //   // std::vector<PeerState> peerStates;
  // };

  Mutex _mutex;

  /// @brief Last known rebootId of every server
  std::unordered_map<ServerID, RebootId> _rebootIds;

  /// @brief List of registered callbacks per server.
  std::unordered_map<ServerID, std::shared_ptr<std::vector<Callback>>> _callbacks;
};

}  // namespace cluster
}  // namespace arangodb

#endif  // ARANGOD_CLUSTER_REBOOTTRACKER_H
