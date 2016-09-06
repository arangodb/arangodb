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

#include "Agency/Agent.h"
#include "Agency/ActivationCallback.h"
#include "Basics/ConditionLocker.h"

#include <chrono>
#include <thread>

using namespace arangodb::consensus;

AgentActivator::AgentActivator() : Thread("AgentActivator"), _agent(nullptr) {}

AgentActivator::AgentActivator(Agent* agent, std::string const& peerId)
  : Thread("AgentActivator"), _agent(agent), _peerId(peerId) {}

// Shutdown if not already
AgentActivator::~AgentActivator() { shutdown(); }

void AgentActivator::beginShutdown() { Thread::beginShutdown(); }

bool AgentActivator::start() { return Thread::start(); }

void AgentActivator::run() {

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting activation of " << _peerId;

  std::string const path = privApiPrefix + "activate";
  
  while (!this->isStopping()) {

    auto const& pool = _agent->config().pool();
    Builder builder;
    size_t highest = 0;

    auto headerFields =
      std::make_unique<std::unordered_map<std::string, std::string>>();
    arangodb::ClusterComm::instance()->asyncRequest(
      "1", 1, pool.at(_peerId), rest::RequestType::POST,
      path, std::make_shared<std::string>(builder.toJson()), headerFields,
      std::make_shared<ActivationCallback>(_agent, _peerId, highest), 5.0, true,
      1.0);
    
  }

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Done activating " << _peerId;

  
}
