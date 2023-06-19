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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#include "DebugRaceController.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ScopeGuard.h"

using namespace arangodb;
using namespace arangodb::basics;

static DebugRaceController Instance{};

DebugRaceController& DebugRaceController::sharedInstance() { return Instance; }

void DebugRaceController::reset() {
  {
    auto guard = std::scoped_lock<std::mutex>(_mutex);
    _data.clear();
  }
  _condVariable.notify_all();
}

bool DebugRaceController::didTrigger(
    std::unique_lock<std::mutex> const& guard,
    std::size_t numberOfThreadsToWaitFor) const {
  TRI_ASSERT(guard.owns_lock());
  TRI_ASSERT(_data.size() <= numberOfThreadsToWaitFor);
  return numberOfThreadsToWaitFor == _data.size();
}

auto DebugRaceController::waitForOthers(
    size_t numberOfThreadsToWaitFor, std::any myData,
    application_features::ApplicationServer const& server)
    -> std::optional<std::vector<std::any>> {
  auto notifyGuard =
      scopeGuard([this]() noexcept { _condVariable.notify_all(); });
  {
    std::unique_lock<std::mutex> guard(_mutex);

    if (didTrigger(guard, numberOfThreadsToWaitFor)) {
      return std::nullopt;
    }

    _data.reserve(numberOfThreadsToWaitFor);
    _data.emplace_back(std::move(myData));
    _condVariable.wait(guard, [&] {
      // check empty to continue after being reset
      return _data.empty() || _data.size() == numberOfThreadsToWaitFor ||
             server.isStopping();
    });

    if (didTrigger(guard, numberOfThreadsToWaitFor)) {
      return {_data};
    } else {
      return std::nullopt;
    }
  }
}

#endif
