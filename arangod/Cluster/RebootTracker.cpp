////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "RebootTracker.h"

#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <algorithm>

using namespace arangodb;
using namespace arangodb::cluster;

static_assert(
    std::is_same_v<decltype(SchedulerFeature::SCHEDULER), RebootTracker::SchedulerPointer>,
    "Type of SchedulerPointer must match SchedulerFeature::SCHEDULER");
static_assert(std::is_pointer<RebootTracker::SchedulerPointer>::value,
              "If SCHEDULER is changed to a non-pointer type, this class "
              "might have to be adapted");
static_assert(
    std::is_base_of<Scheduler, std::remove_pointer<RebootTracker::SchedulerPointer>::type>::value,
    "SchedulerPointer is expected to point to an instance of Scheduler");

RebootTracker::RebootTracker(RebootTracker::SchedulerPointer scheduler)
    : _scheduler(scheduler) {
  // All the mocked application servers in the catch tests that use the
  // ClusterFeature, which at some point instantiates this, do not start the
  // SchedulerFeature. Thus this dies. However, we will be able to fix that at
  // a central place later, as there is some refactoring going on there. Then
  // this #ifdef can be removed.
#ifndef ARANGODB_USE_GOOGLE_TESTS
  TRI_ASSERT(_scheduler != nullptr);
#endif
}

void RebootTracker::updateServerState(std::unordered_map<ServerID, RebootId> const& state) {
  MUTEX_LOCKER(guard, _mutex);
    
  // FIXME: can't get this log message to compile in some build environments...
  // ostreaming _rebootIds causes a compiler failure because operator<< is not visible
  // here anymore for some reason, which is very likely due to a changed order of include
  // files or some other change in declaration order.
  // leaving the log message here because it can be compiled in some environments and
  // maybe someone can fix it in the future
  //
  // LOG_TOPIC("77a6e", DEBUG, Logger::CLUSTER)
  //     << "updating reboot server state from " << _rebootIds << " to " << state;

  // Call cb for each iterator.
  auto for_each_iter = [](auto begin, auto end, auto cb) {
    auto it = begin;
    decltype(it) next;
    while (it != end) {
      // save next iterator now, in case cb invalidates it.
      next = std::next(it);
      cb(it);
      it = next;
    }
  };

  // For all known servers, look whether they are changed or were removed
  for_each_iter(_rebootIds.begin(), _rebootIds.end(), [&](auto const curIt) {
    auto const& serverId = curIt->first;
    auto& oldRebootId = curIt->second;
    auto const& newIt = state.find(serverId);

    if (newIt == state.end()) {
      // Try to schedule all callbacks for serverId.
      // If that didn't throw, erase the entry.
      LOG_TOPIC("88858", INFO, Logger::CLUSTER)
          << "Server " << serverId << " removed, aborting its old jobs now.";
      scheduleAllCallbacksFor(serverId);
      auto it = _callbacks.find(serverId);
      if (it != _callbacks.end()) {
        TRI_ASSERT(it->second.empty());
        _callbacks.erase(it);
      }
      _rebootIds.erase(curIt);
    } else {
      TRI_ASSERT(serverId == newIt->first);
      auto const& newRebootId = newIt->second;
      TRI_ASSERT(oldRebootId <= newRebootId);
      if (oldRebootId < newRebootId) {
        LOG_TOPIC("88857", INFO, Logger::CLUSTER)
            << "Server " << serverId << " gone or rebooted, aborting its old jobs now.";
        // Try to schedule all callbacks for serverId older than newRebootId.
        // If that didn't throw, erase the entry.
        scheduleCallbacksFor(serverId, newRebootId);
        oldRebootId = newRebootId;
      }
    }
  });

  // Look whether there are servers that are still unknown
  // (note: we could shortcut this and return if the sizes are equal, as at
  // this point, all entries in _rebootIds are also in state)
  for (auto const& newIt : state) {
    auto const& serverId = newIt.first;
    auto const& rebootId = newIt.second;
    auto const rv = _rebootIds.try_emplace(serverId, rebootId);
    auto const inserted = rv.second;
    // If we inserted a new server, we may NOT already have any callbacks for
    // it!
    TRI_ASSERT(!inserted || _callbacks.find(serverId) == _callbacks.end());
  }
}

CallbackGuard RebootTracker::callMeOnChange(RebootTracker::PeerState const& peerState,
                                            RebootTracker::Callback callback,
                                            std::string callbackDescription) {
  MUTEX_LOCKER(guard, _mutex);

  auto const rebootIdIt = _rebootIds.find(peerState.serverId());

  // We MUST NOT insert something in _callbacks[serverId] unless _rebootIds[serverId] exists!
  if (rebootIdIt == _rebootIds.end()) {
    std::string const error = [&]() {
      std::stringstream strstream;
      strstream << "When trying to register callback '" << callbackDescription << "': "
                << "The server " << peerState.serverId() << " is not known. "
                << "If this server joined the cluster in the last seconds, "
                   "this can happen.";
      return strstream.str();
    }();
    LOG_TOPIC("76abc", INFO, Logger::CLUSTER) << error;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_SERVER_UNKNOWN, error);
  }

  auto const currentRebootId = rebootIdIt->second;

  if (peerState.rebootId() < currentRebootId) {
    // If this ID is already older, schedule the callback immediately.
    queueCallback(DescriptedCallback{std::move(callback), std::move(callbackDescription)});
    return CallbackGuard{nullptr};
  }

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

  // The guard constructor might, theoretically, throw, and so can constructing
  // the std::function. So we need to construct it before emplacing the callback.
  auto callbackGuard = CallbackGuard([this, peerState, callbackId]() {
    unregisterCallback(peerState, callbackId);
  });

  auto const [iterator, inserted] =
      callbackMap.try_emplace(callbackId, DescriptedCallback{std::move(callback),
                                                         std::move(callbackDescription)});
  TRI_ASSERT(inserted);
  TRI_ASSERT(callbackId == iterator->first);

  return callbackGuard;
}

void RebootTracker::scheduleAllCallbacksFor(ServerID const& serverId) {
  scheduleCallbacksFor(serverId, RebootId::max());
  // Now the rebootId map of this server, if it exists, must be empty.
  TRI_ASSERT(_callbacks.find(serverId) == _callbacks.end() ||
             _callbacks.find(serverId)->second.empty());
}

// This function may throw.
// If (and only if) it returns, it has scheduled all affected callbacks, and
// removed them from the registry.
// Otherwise the state is unchanged.
void RebootTracker::scheduleCallbacksFor(ServerID const& serverId, RebootId rebootId) {
  _mutex.assertLockedByCurrentThread();

  auto serverIt = _callbacks.find(serverId);
  if (serverIt != _callbacks.end()) {
    auto& rebootMap = serverIt->second;
    auto const begin = rebootMap.begin();
    // lower_bounds returns the first iterator that is *not less than* rebootId
    auto const end = rebootMap.lower_bound(rebootId);

    std::vector<decltype(begin->second)> callbackSets;
    callbackSets.reserve(std::distance(begin, end));

    std::for_each(begin, end, [&callbackSets](auto it) {
      callbackSets.emplace_back(it.second);
    });

    // could throw
    queueCallbacks(std::move(callbackSets));

    // If and only if we successfully scheduled all callbacks, we erase them
    // from the registry.
    rebootMap.erase(begin, end);
  }
}

RebootTracker::Callback RebootTracker::createSchedulerCallback(
    std::vector<std::shared_ptr<std::unordered_map<CallbackId, DescriptedCallback>>> callbacks) {
  TRI_ASSERT(!callbacks.empty());
  TRI_ASSERT(std::none_of(callbacks.cbegin(), callbacks.cend(),
                          [](auto it) { return it == nullptr; }));
  TRI_ASSERT(std::none_of(callbacks.cbegin(), callbacks.cend(),
                          [](auto it) { return it->empty(); }));

  return [callbacks = std::move(callbacks)]() {
    LOG_TOPIC("80dfe", DEBUG, Logger::CLUSTER)
        << "Executing scheduled reboot callbacks";
    TRI_ASSERT(!callbacks.empty());
    for (auto const& callbacksPtr : callbacks) {
      TRI_ASSERT(callbacksPtr != nullptr);
      TRI_ASSERT(!callbacksPtr->empty());
      for (auto const& it : *callbacksPtr) {
        auto const& cb = it.second.callback;
        auto const& descr = it.second.description;
        LOG_TOPIC("afdfd", DEBUG, Logger::CLUSTER)
            << "Executing callback " << it.second.description;
        try {
          cb();
        } catch (arangodb::basics::Exception const& ex) {
          LOG_TOPIC("88a63", INFO, Logger::CLUSTER)
              << "Failed to execute reboot callback: " << descr << ": "
              << "[" << ex.code() << "] " << ex.what();
        } catch (std::exception const& ex) {
          LOG_TOPIC("3d935", INFO, Logger::CLUSTER)
              << "Failed to execute reboot callback: " << descr << ": " << ex.what();
        } catch (...) {
          LOG_TOPIC("f7427", INFO, Logger::CLUSTER)
              << "Failed to execute reboot callback: " << descr << ": "
              << "Unknown error.";
        }
      }
    }
  };
}

void RebootTracker::queueCallbacks(
    std::vector<std::shared_ptr<std::unordered_map<CallbackId, DescriptedCallback>>> callbacks) {
  if (callbacks.empty()) {
    return;
  }

  TRI_ASSERT(std::none_of(callbacks.cbegin(), callbacks.cend(),
                          [](auto it) { return it == nullptr; }));
  TRI_ASSERT(std::none_of(callbacks.cbegin(), callbacks.cend(),
                          [](auto it) { return it->empty(); }));

  auto cb = createSchedulerCallback(std::move(callbacks));

  if (!_scheduler->queue(RequestLane::CLUSTER_INTERNAL, std::move(cb))) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUEUE_FULL,
        "No available threads when trying to queue cleanup "
        "callbacks due to a server reboot");
  }
}

void RebootTracker::unregisterCallback(PeerState const& peerState,
                                       RebootTracker::CallbackId callbackId) {
  MUTEX_LOCKER(guard, _mutex);

  auto const cbIt = _callbacks.find(peerState.serverId());
  if (cbIt != _callbacks.end()) {
    auto& rebootMap = cbIt->second;
    auto const rbIt = rebootMap.find(peerState.rebootId());
    if (rbIt != rebootMap.end()) {
      auto& callbackSetPtr = rbIt->second;
      TRI_ASSERT(callbackSetPtr != nullptr);
      callbackSetPtr->erase(callbackId);
      if (callbackSetPtr->empty()) {
        rebootMap.erase(rbIt);
      }
    }
  }
}

RebootTracker::CallbackId RebootTracker::getNextCallbackId() noexcept {
  _mutex.assertLockedByCurrentThread();

  CallbackId nextId = _nextCallbackId;
  ++_nextCallbackId;
  return nextId;
}

void RebootTracker::queueCallback(DescriptedCallback callback) {
  queueCallbacks({std::make_shared<std::unordered_map<CallbackId, DescriptedCallback>>(
      std::unordered_map<CallbackId, DescriptedCallback>{ { getNextCallbackId(), std::move(callback) } }
  )});
}
    
void RebootTracker::PeerState::toVelocyPack(velocypack::Builder& builder) const {
  builder.openObject();
  builder.add(StaticStrings::AttrCoordinatorId, VPackValue(_serverId));
  builder.add(StaticStrings::AttrCoordinatorRebootId, VPackValue(_rebootId.value()));
  builder.close();
}

RebootTracker::PeerState RebootTracker::PeerState::fromVelocyPack(velocypack::Slice slice) {
  TRI_ASSERT(slice.isObject());
  VPackSlice serverIdSlice = slice.get(StaticStrings::AttrCoordinatorId);
  VPackSlice rebootIdSlice = slice.get(StaticStrings::AttrCoordinatorRebootId);

  if (!serverIdSlice.isString() || !rebootIdSlice.isInteger()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid reboot id");
  }
  
  return { serverIdSlice.copyString(), RebootId(rebootIdSlice.getUInt()) };
}

CallbackGuard::CallbackGuard() : _callback(nullptr) {}

CallbackGuard::CallbackGuard(std::function<void(void)> callback)
    : _callback(std::move(callback)) {}

// NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
CallbackGuard::CallbackGuard(CallbackGuard&& other)
    : _callback(std::move(other._callback)) {
  other._callback = nullptr;
}

// NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
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
