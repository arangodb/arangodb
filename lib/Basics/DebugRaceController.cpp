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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#include "DebugRaceController.h"
#include "ApplicationFeatures/ApplicationServer.h"

using namespace arangodb;
using namespace arangodb::basics;

static DebugRaceController Instance{};

DebugRaceController::DebugRaceController() {}

DebugRaceController& DebugRaceController::sharedInstance() { return Instance; }

void DebugRaceController::reset() {
  std::unique_lock<std::mutex> guard(_mutex);
  _condVariable.notify_all();
  _data.clear();
  _didTrigger = false;
}

bool DebugRaceController::didTrigger() const { 
  std::unique_lock<std::mutex> guard(_mutex);
  return _didTrigger; 
}

std::vector<std::any> DebugRaceController::data() const { 
  std::unique_lock<std::mutex> guard(_mutex);
  return _data; 
}

void DebugRaceController::waitForOthers(size_t numberOfThreadsToWaitFor, std::any myData,
                                        arangodb::application_features::ApplicationServer const& server) {
  std::unique_lock<std::mutex> guard(_mutex);
  _data.emplace_back(std::move(myData));
  _condVariable.wait(guard, [&] {
    return _data.size() == numberOfThreadsToWaitFor || server.isStopping();
  });
  _didTrigger = true;
  _condVariable.notify_all();
}

#endif
