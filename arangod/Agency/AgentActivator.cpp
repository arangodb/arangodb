////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "AgentActivator.h"

#include <chrono>
#include <thread>

#include "Agency/Agent.h"
#include "Agency/ActivationCallback.h"
#include "Basics/ConditionLocker.h"

using namespace arangodb::consensus;
using namespace std::chrono;

AgentActivator::AgentActivator() : Thread("AgentActivator"), _agent(nullptr) {}

AgentActivator::AgentActivator(Agent* agent, std::string const& failed,
                               std::string const& replacement)
  : Thread("AgentActivator"),
    _agent(agent),
    _failed(failed),
    _replacement(replacement) {}

// Shutdown if not already
AgentActivator::~AgentActivator() {
  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Done activating " << _replacement;
  shutdown();
}

void AgentActivator::run() {

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting activation of " << _replacement;

  std::string const path = privApiPrefix + "activate";
  auto const started = system_clock::now();
  auto const timeout = seconds(60);
  auto const endpoint = _agent->config().pool().at(_replacement);

  CONDITION_LOCKER(guard, _cv);
 
  while (!this->isStopping()) {

    // All snapshots and all logs
    query_t allLogs = _agent->allLogs();

    auto headerFields =
      std::make_unique<std::unordered_map<std::string, std::string>>();
    arangodb::ClusterComm::instance()->asyncRequest(
      "1", 1, endpoint, rest::RequestType::POST, path,
      std::make_shared<std::string>(allLogs->toJson()), headerFields,
      std::make_shared<ActivationCallback>(_agent, _failed, _replacement),
      5.0, true, 1.0);

    _cv.wait(10000000); // 10 sec

    if ((std::chrono::system_clock::now() - started) > timeout) {
      _agent->failedActivation(_failed, _replacement);
      LOG_TOPIC(WARN, Logger::AGENCY)
        << "Timed out while activating agent " << _replacement;
      break;
    }

  }
}

void AgentActivator::beginShutdown() {
  Thread::beginShutdown();
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}
