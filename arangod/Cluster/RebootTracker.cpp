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

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/RebootTracker.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

#include <velocypack/Slice.h>
#include <absl/strings/str_cat.h>
#include <algorithm>

namespace arangodb::cluster {
namespace {

void safeInvoke(RebootTracker::DescriptedCallback& callback) noexcept {
  try {
    LOG_TOPIC("afdfd", DEBUG, Logger::CLUSTER)
        << "Executing callback " << callback.description;
    callback.callback();
  } catch (basics::Exception const& ex) {
    LOG_TOPIC("88a63", INFO, Logger::CLUSTER)
        << "Failed to execute reboot callback: " << callback.description << ": "
        << "[" << ex.code() << "] " << ex.what();
  } catch (std::exception const& ex) {
    LOG_TOPIC("3d935", INFO, Logger::CLUSTER)
        << "Failed to execute reboot callback: " << callback.description << ": "
        << ex.what();
  } catch (...) {
    LOG_TOPIC("f7427", INFO, Logger::CLUSTER)
        << "Failed to execute reboot callback: " << callback.description << ": "
        << "Unknown error.";
  }
}

void safeInvokes(RebootTracker::RebootIds::node_type& callbacks) noexcept {
  for (auto& callback : callbacks.mapped()) {
    safeInvoke(callback.second);
  }
}

void safeInvokes(RebootTracker::RebootIds::value_type& callbacks) noexcept {
  for (auto& callback : callbacks.second) {
    safeInvoke(callback.second);
  }
}

}  // namespace

static_assert(
    std::is_same_v<decltype(SchedulerFeature::SCHEDULER),
                   RebootTracker::SchedulerPointer>,
    "Type of SchedulerPointer must match SchedulerFeature::SCHEDULER");
static_assert(std::is_pointer<RebootTracker::SchedulerPointer>::value,
              "If SCHEDULER is changed to a non-pointer type, this class "
              "might have to be adapted");
static_assert(
    std::is_base_of<
        Scheduler,
        std::remove_pointer<RebootTracker::SchedulerPointer>::type>::value,
    "SchedulerPointer is expected to point to an instance of Scheduler");

RebootTracker::RebootTracker(RebootTracker::SchedulerPointer scheduler)
    : _scheduler{scheduler} {
  // All the mocked application servers in the catch tests that use the
  // ClusterFeature, which at some point instantiates this, do not start the
  // SchedulerFeature. Thus this dies. However, we will be able to fix that at
  // a central place later, as there is some refactoring going on there. Then
  // this #ifdef can be removed.
#ifndef ARANGODB_USE_GOOGLE_TESTS
  TRI_ASSERT(_scheduler != nullptr);
#endif
}

void RebootTracker::updateServerState(State state) {
  std::lock_guard guard{_mutex};
  LOG_TOPIC("77a6e", TRACE, Logger::CLUSTER)
      << "updating reboot server state from " << _state << " to " << state;
  // We iterate for _state, not for _callbacks,
  // because we want to log all gone or rebooted server
  for (auto const& [serverId, oldRebootId] : _state) {
    auto it = state.find(serverId);
    auto const newRebootId = it != state.end() ? it->second : RebootId::max();
    if (oldRebootId != newRebootId) {
      TRI_ASSERT(oldRebootId < newRebootId);
      LOG_TOPIC("88857", INFO, Logger::CLUSTER)
          << "Server " << serverId
          << " gone or rebooted, aborting its old jobs now.";
      queueCallbacks(serverId, newRebootId);
    }
  }
  _state = std::move(state);
}

CallbackGuard RebootTracker::callMeOnChange(std::string_view serverId,
                                            RebootId rebootId,
                                            Callback callback,
                                            std::string_view description) {
  std::lock_guard guard{_mutex};
  auto const it = _state.find(serverId);
  // We MUST NOT insert something in _callbacks[serverId]
  // unless _state[serverId] exists!
  if (it == _state.end()) {
    auto const error =
        absl::StrCat("When trying to register callback '", description,
                     "': The server ", serverId,
                     " is not known. If this server joined the cluster in the "
                     "last seconds, this can happen.");
    LOG_TOPIC("76abc", INFO, Logger::CLUSTER) << error;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_SERVER_UNKNOWN, error);
  }
  auto const currRebootId = it->second;
  if (rebootId < currRebootId) {
    // If this ID is already older, schedule the callback immediately.
    queueCallback({std::move(callback), std::string{description}});
    return CallbackGuard{};
  }

  // For the given server, get the existing rebootId => [callbacks] map,
  // or create a new one
  auto& rebootsMap = _callbacks[serverId];
  // For the given rebootId, get the existing callbacks map, or create a new one
  auto& callbacksMap = rebootsMap[rebootId];
  auto const callbackId = ++_callbackId;
  auto [_, inserted] = callbacksMap.try_emplace(
      callbackId,
      DescriptedCallback{std::move(callback), std::string{description}});
  TRI_ASSERT(inserted);
  return CallbackGuard{[this, serverId = std::string{serverId}, rebootId,
                        callbackId]() noexcept {
    unregisterCallback(serverId, rebootId, callbackId);
  }};
}

void RebootTracker::queueCallbacks(std::string_view serverId, RebootId to) {
  auto it = _callbacks.find(serverId);
  if (it == _callbacks.end()) {
    return;
  }
  auto schedule = [&](auto&& batch) noexcept {
    _scheduler->queue(RequestLane::CLUSTER_INTERNAL,
                      [batch = std::move(batch)]() mutable noexcept {
                        for (auto& callbacks : batch) {
                          safeInvokes(callbacks);
                        }
                      });
  };
  auto& reboots = it->second;
  TRI_ASSERT(!reboots.empty());
  if (to > reboots.crbegin()->first) {
    schedule(std::move(it->second));
    _callbacks.erase(it);
    return;
  }
  std::vector<RebootIds::node_type> callbacks;
  for (auto rbIt = reboots.begin(); rbIt->first < to;) {
    TRI_ASSERT(rbIt != reboots.end());
    callbacks.push_back(reboots.extract(rbIt++));
  }
  TRI_ASSERT(!reboots.empty());
  schedule(std::move(callbacks));
}

void RebootTracker::queueCallback(DescriptedCallback&& callback) noexcept {
  _scheduler->queue(RequestLane::CLUSTER_INTERNAL,
                    [callback = std::move(callback)]() mutable noexcept {
                      safeInvoke(callback);
                    });
}

void RebootTracker::unregisterCallback(std::string_view serverId,
                                       RebootId rebootId,
                                       CallbackId callbackId) noexcept {
  std::lock_guard guard{_mutex};
  if (auto const it = _callbacks.find(serverId); it != _callbacks.end()) {
    auto& reboots = it->second;
    if (auto const rbIt = reboots.find(rebootId); rbIt != reboots.end()) {
      auto& callbacks = rbIt->second;
      callbacks.erase(callbackId);
      if (callbacks.empty()) {
        reboots.erase(rbIt);
        if (reboots.empty()) {
          _callbacks.erase(it);
        }
      }
    }
  }
}

}  // namespace arangodb::cluster
