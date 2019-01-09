////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "JobContext.h"

#include "Agency/ActiveFailoverJob.h"
#include "Agency/AddFollower.h"
#include "Agency/CleanOutServer.h"
#include "Agency/FailedFollower.h"
#include "Agency/FailedLeader.h"
#include "Agency/FailedServer.h"
#include "Agency/MoveShard.h"
#include "Agency/RemoveFollower.h"

using namespace arangodb::consensus;

JobContext::JobContext(JOB_STATUS status, std::string id, Node const& snapshot,
                       AgentInterface* agent)
    : _job(nullptr) {
  std::string path = pos[status] + id;
  auto typePair = snapshot.hasAsString(path + "/type");
  std::string type;

  if (typePair.second) {
    type = typePair.first;
  }  // if

  if (type == "failedLeader") {
    _job = std::make_unique<FailedLeader>(snapshot, agent, status, id);
  } else if (type == "failedFollower") {
    _job = std::make_unique<FailedFollower>(snapshot, agent, status, id);
  } else if (type == "failedServer") {
    _job = std::make_unique<FailedServer>(snapshot, agent, status, id);
  } else if (type == "cleanOutServer") {
    _job = std::make_unique<CleanOutServer>(snapshot, agent, status, id);
  } else if (type == "moveShard") {
    _job = std::make_unique<MoveShard>(snapshot, agent, status, id);
  } else if (type == "addFollower") {
    _job = std::make_unique<AddFollower>(snapshot, agent, status, id);
  } else if (type == "removeFollower") {
    _job = std::make_unique<RemoveFollower>(snapshot, agent, status, id);
  } else if (type == "activeFailover") {
    _job = std::make_unique<ActiveFailoverJob>(snapshot, agent, status, id);
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY)
        << "Failed to run supervision job " << type << " with id " << id;
  }
}

void JobContext::create(std::shared_ptr<VPackBuilder> b) {
  if (_job != nullptr) {
    _job->create(b);
  }
}

void JobContext::start() {
  if (_job != nullptr) {
    _job->start();
  }
}

void JobContext::run() {
  if (_job != nullptr) {
    _job->run();
  }
}

void JobContext::abort() {
  if (_job != nullptr) {
    _job->abort();
  }
}
