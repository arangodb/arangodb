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
#include "lib/Basics/ScopeGuard.h"
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
                                            std::string callbackDescription) {
  MUTEX_LOCKER(guard, _mutex);

  // For the given server, get the existing rebootId => [callbacks] map,
  // or create a new one
  auto& rebootIdMap = _callbacks[peerState.serverId()];
  // For the given rebootId, get the existing callbacks map,
  // or create a new one
  auto& callbackMapPtr = rebootIdMap[peerState.rebootId()];

  if (callbackMapPtr == nullptr) {
    // We must never leave a nullptr in here!
    // Try to create a new map, or remove the entry.
    try {
      callbackMapPtr =
          std::make_shared<std::remove_reference<decltype(callbackMapPtr)>::type::element_type>();
    } catch (...) {
      rebootIdMap.erase(peerState.rebootId());
      throw;
    }
  }

  TRI_ASSERT(callbackMapPtr != nullptr);

  auto& callbackMap = *callbackMapPtr;

  auto const callbackId = getNextCallbackId();

  // The guard constructor might, theoretically, throw. So we need to construct
  // it before emplacing the callback.
  auto callbackGuard =
      CallbackGuard([this, callbackId]() { unregisterCallback(callbackId); });

  auto emplaceRv =
      callbackMap.emplace(callbackId, DescriptedCallback{std::move(callback),
                                                         std::move(callbackDescription)});
  auto const iterator = emplaceRv.first;
  bool const inserted = emplaceRv.second;
  TRI_ASSERT(inserted);
  TRI_ASSERT(callbackId == iterator->first);

  // TODO I'm wondering why this compiles (with clang, at least), as the copy
  //      constructor is deleted. I don't think it should...
  return callbackGuard;
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
RebootTracker::CallbackId RebootTracker::getNextCallbackId() noexcept {
  CallbackId nextId = _nextCallbackId;
  ++_nextCallbackId;
  return nextId;
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
