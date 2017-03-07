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

#include "JobContext.h"

#include "Agency/AddFollower.h"
#include "Agency/CleanOutServer.h"
#include "Agency/FailedFollower.h"
#include "Agency/FailedLeader.h"
#include "Agency/FailedServer.h"
#include "Agency/MoveShard.h"
#include "Agency/RemoveServer.h"
#include "Agency/UnassumedLeadership.h"

using namespace arangodb::consensus;

JobContext::JobContext (
  std::string const& type, Node const& snapshot, Agent* agent,
  std::string const& id, std::string const& creator)
  : _job(nullptr) {

  if (type == "failedLeader") {
    _job =
      std::unique_ptr<FailedLeader>(
        new FailedLeader(snapshot, agent, id, creator));
  } else if (type == "failedFollower") {
    _job =
      std::unique_ptr<FailedFollower>(
        new FailedFollower(snapshot, agent, id, creator));
  } else if (type == "failedServer") {
    _job =
      std::unique_ptr<FailedServer>(
        new FailedServer(snapshot, agent, id, creator));
  } else if (type == "cleanOutServer") {
    _job =
      std::unique_ptr<CleanOutServer>(
        new CleanOutServer(snapshot, agent, id, creator));
  } else if (type == "removeServer") {
    _job =
      std::unique_ptr<RemoveServer>(
        new RemoveServer(snapshot, agent, id, creator));
  } else if (type == "moveShard") {
    _job =
      std::unique_ptr<MoveShard>(
        new MoveShard(snapshot, agent, id, creator));
  } else if (type == "addFollower") {
    _job =
      std::unique_ptr<AddFollower>(
        new AddFollower(snapshot, agent, id, creator));
  } else if (type == "unassumedLeadership") {
    _job =
      std::unique_ptr<UnassumedLeadership>(
        new UnassumedLeadership( snapshot, agent, id, creator));
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) <<
      "Failed to run supervision job " << type << " with id " << id;
  }

}

JobContext::JobContext (
  std::string const& path, Node const& snapshot,
  Agent* agent) : _job(nullptr) {

  auto const& job     = snapshot(path);
  auto const& type    = job("type").getString();
  auto const& id      = job("id").getString();
  auto const& creator = job("creator").getString();
  
  if        (type == "failedLeader") {
    _job =
      std::unique_ptr<FailedLeader>(
        new FailedLeader(snapshot, agent, id, creator));
  } else if (type == "failedFollower") {
    _job =
      std::unique_ptr<FailedFollower>(
        new FailedFollower(snapshot, agent, id, creator));
  } else if (type == "failedServer") {
    _job =
      std::unique_ptr<FailedServer>(
        new FailedServer(snapshot, agent, id, creator));
  } else if (type == "cleanOutServer") {
    _job =
      std::unique_ptr<CleanOutServer>(
        new CleanOutServer(snapshot, agent, id, creator));
  } else if (type == "removeServer") {
    _job =
      std::unique_ptr<RemoveServer>(
        new RemoveServer(snapshot, agent, id, creator));
  } else if (type == "moveShard") {
    _job =
      std::unique_ptr<MoveShard>(
        new MoveShard(snapshot, agent, id, creator));
  } else if (type == "addFollower") {
    _job =
      std::unique_ptr<AddFollower>(
        new AddFollower(snapshot, agent, id, creator));
  } else if (type == "unassumedLeadership") {
    _job =
      std::unique_ptr<UnassumedLeadership>(
        new UnassumedLeadership( snapshot, agent, id, creator));
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) <<
      "Failed to run supervision job " << type << " with id " << id;
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

