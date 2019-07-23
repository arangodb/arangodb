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

#include "RebootTracker.h"

#include "Scheduler/SchedulerFeature.h"
#include "lib/Basics/Exceptions.h"
#include "lib/Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::cluster;

/*
 * TODO
 *  Think about how to handle failures gracefully. Mainly:
 *   - alloc failures
 *   - Scheduler->queue failures
 *  Maybe lazy deletion helps?
 */

RebootTracker::RebootTracker(RebootTracker::SchedulerPointer scheduler)
    : _scheduler(scheduler) {
  TRI_ASSERT(_scheduler != nullptr);
}

// TODO We should possibly get the complete list of peers from clusterinfo
//   (rather than only the list of changed peers) in order to be able to retry
//   regularly.
//   In this case, we must move the bidirectional comparison from
//   ClusterInfo.cpp here!

void RebootTracker::updateServerState(std::unordered_map<ServerID, RebootId> const& state) {}

// void RebootTracker::notifyChanges(std::vector<RebootTracker::PeerState> const& peerStates) {
//   MUTEX_LOCKER(guard, _mutex);
//
//   for (auto const& peerState : peerStates) {
//     // this can throw, in which case our update isn't complete.
//     // this is fine, we'll retry on the next call.
//     notifyChange(peerState);
//   }
// }

// bool RebootTracker::notifyChange(RebootTracker::PeerState const& peerState) {
//   _mutex.assertLockedByCurrentThread();
//
//   bool success;
//
//   auto rv = _rebootIds.emplace(peerState.serverId(), peerState.rebootId());
//   if (rv.second) {
//     // Inserted a new element. There must be no existing entries for this
//     server. TRI_ASSERT(_callbacks.find(peerState.serverId()) ==
//     _callbacks.end()); success = true;
//   } else {
//     // Existing element. Now check whether the ID is more recent:
//     if (rv.first->second != peerState.rebootId()) {
//       // tryScheduleCallbacksFor could throw
//       success = tryScheduleCallbacksFor(peerState.serverId());
//       if (success) {
//         // Update reboot ID if and only if we successfully fired the
//         callbacks rv.first->second = peerState.rebootId();
//       }
//     } else {
//       success = true;
//     }
//   }
//
//   return success;
// }

CallbackGuard RebootTracker::callMeOnChange(RebootTracker::PeerState const& peerState,
                                            RebootTracker::Callback callback,
                                            std::string const& callbackDescription) {
  MUTEX_LOCKER(guard, _mutex);
  return {};
  //
  // // Check whether the last known reboot id is older than the passed one;
  // // if so, schedule callbacks and update the reboot id.
  // if (!notifyChange(peerState)) {
  //   std::stringstream error;
  //   error << "Failed to schedule cleanup handlers due to changed rebootId on "
  //            "server "
  //         << peerState.serverId()
  //         << " while trying to register this handler: " << callbackDescription;
  //   THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUEUE_FULL, error.str());
  // }
  //
  // auto const it = _rebootIds.find(peerState.serverId());
  // // Now we must have an entry for the given server, and its last known reboot
  // // id must be at least as recent as the passed one.
  // TRI_ASSERT(it != _rebootIds.end());
  // TRI_ASSERT(it->second >= peerState.rebootId());
  //
  // // Check whether the last known reboot id is newer than the passed one
  // if (it->second > peerState.rebootId()) {
  //   // The server already rebooted, call the failure handler immediately.
  //   // This can throw.
  //   bool const success = queueCallbacks(std::make_shared<std::vector<Callback>>(
  //       std::vector<Callback>{std::move(callback)}));
  //   if (!success) {
  //     std::stringstream error;
  //     error << "Failed to immediately execute cleanup handler for server "
  //           << peerState.serverId()
  //           << " while trying to register this handler: " << callbackDescription;
  //     THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUEUE_FULL, error.str());
  //   }
  // } else {
  //   TRI_ASSERT(it->second == peerState.rebootId());
  //
  //   // We've handled the "unusual" cases already; now, just try to insert the callback
  //   auto const rv = _callbacks.emplace(peerState.serverId(), nullptr);
  //   auto const iterator = rv.first;
  //   bool const inserted = rv.second;
  //   TRI_ASSERT(iterator != _callbacks.end());
  //   auto& callbacksPtr = iterator->second;
  //   // We did an insert if and only if the value is null
  //   TRI_ASSERT(inserted == (callbacksPtr == nullptr));
  //   if (callbacksPtr != nullptr) {
  //     // Try to add callback to the existing array
  //     callbacksPtr->emplace_back(std::move(callback));
  //   } else {
  //     // We didn't have a callbacks array for this server, try to create a new
  //     // one.
  //     try {
  //       callbacksPtr = std::make_shared<std::vector<Callback>>(
  //           std::vector<Callback>{std::move(callback)});
  //     } catch (...) {
  //       // Remove the new entry if we failed to allocate a vector,
  //       // we may never leave null pointers in here!
  //       _callbacks.erase(iterator);
  //       throw;
  //     }
  //   }
  // }
  //
  // return CallbackGuard{*this, callbackId};
}

// Returns false iff callbacks have to be scheduled, but putting them on the
// queue failed.
bool RebootTracker::tryScheduleCallbacksFor(ServerID const& serverId) {
  _mutex.assertLockedByCurrentThread();

  return false;

  // bool success;
  //
  // auto it = _callbacks.find(serverId);
  // if (it != _callbacks.end()) {
  //   // could throw
  //   success = queueCallbacks(it->second);
  //
  //   // If and only if we successfully scheduled all callbacks, we erase them
  //   // from the registry.
  //   if (success) {
  //     // cannot throw
  //     _callbacks.erase(it);
  //   }
  // } else {
  //   success = true;
  // }
  //
  // return success;
}

RebootTracker::Callback RebootTracker::createSchedulerCallback(
    std::shared_ptr<std::vector<Callback>> callbacks) {
  return [cbs = std::move(callbacks)]() {
    TRI_ASSERT(cbs != nullptr);
    for (auto const& it : *cbs) {
      try {
        it();
      } catch (arangodb::basics::Exception const& ex) {
        LOG_TOPIC("88a63", INFO, Logger::CLUSTER)
            << "Failed to execute reboot callback: "
            << "[" << ex.code() << "] " << ex.what();
      } catch (std::exception const& ex) {
        LOG_TOPIC("88a63", INFO, Logger::CLUSTER)
            << "Failed to execute reboot callback: " << ex.what();
      } catch (...) {
        LOG_TOPIC("88a63", INFO, Logger::CLUSTER)
            << "Failed to execute reboot callback: Unknown error.";
      }
    }
  };
}

bool RebootTracker::queueCallbacks(std::shared_ptr<std::vector<RebootTracker::Callback>> callbacks) {
  TRI_ASSERT(callbacks != nullptr);
  return false;

  // auto cb = createSchedulerCallback(std::move(callbacks));
  //
  // // TODO which lane should we use?
  // // could throw
  // return SchedulerFeature::SCHEDULER->queue(RequestLane::CLUSTER_INTERNAL, std::move(cb));
}

void RebootTracker::unregisterCallback(RebootTracker::CallbackId callbackId) {
  // TODO unregister callback
}

CallbackGuard::CallbackGuard() : _callback(nullptr) {}

CallbackGuard::CallbackGuard(std::function<void(void)> callback)
    : _callback(std::move(callback)) {}

CallbackGuard::CallbackGuard(CallbackGuard&& other)
    : _callback(std::move(other._callback)) {
  other._callback = nullptr;
}

CallbackGuard& CallbackGuard::operator=(CallbackGuard&& other) {
  call();
  _callback = std::move(other._callback);
  other._callback = nullptr;
  return *this;
}

CallbackGuard::~CallbackGuard() { call(); }

void CallbackGuard::callAndClear() {
  call();
  _callback = nullptr;
}

void CallbackGuard::call() {
  if (_callback) {
    _callback();
  }
}
