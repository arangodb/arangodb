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

#include "FailedServer.h"

#include "Agency/Agent.h"
#include "Agency/FailedLeader.h"
#include "Agency/FailedFollower.h"
#include "Agency/Job.h"
#include "Agency/UnassumedLeadership.h"

using namespace arangodb::consensus;

FailedServer::FailedServer(Node const& snapshot, Agent* agent,
                           std::string const& jobId, std::string const& creator,
                           std::string const& agencyPrefix,
                           std::string const& server)
    : Job(snapshot, agent, jobId, creator, agencyPrefix), _server(server) {
  try {
    JOB_STATUS js = status();
    if (js == TODO) {
      start();
    } else if (js == NOTFOUND) {
      if (create()) {
        start();
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
    finish("DBServers/" + _server, false, e.what());
  }
}

FailedServer::~FailedServer() {}

bool FailedServer::start() {
  LOG_TOPIC(INFO, Logger::AGENCY)
      << "Trying to start FailedServer job" + _jobId + " for server " + _server;

  // Copy todo to pending
  Builder todo, pending;

  // Get todo entry
  todo.openArray();
  if (_jb == nullptr) {
    try {
      _snapshot(toDoPrefix + _jobId).toBuilder(todo);
    } catch (std::exception const&) {
      LOG_TOPIC(INFO, Logger::AGENCY) << "Failed to get key " + toDoPrefix +
                                             _jobId + " from agency snapshot";
      return false;
    }
  } else {
    todo.add(_jb->slice()[0].get(_agencyPrefix + toDoPrefix + _jobId));
  }
  todo.close();

  // Prepare peding entry, block toserver
  pending.openArray();

  // --- Add pending
  pending.openObject();
  pending.add(_agencyPrefix + pendingPrefix + _jobId,
              VPackValue(VPackValueType::Object));
  pending.add("timeStarted",
              VPackValue(timepointToString(std::chrono::system_clock::now())));
  for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
    pending.add(obj.key.copyString(), obj.value);
  }
  pending.close();

  // --- Delete todo
  pending.add(_agencyPrefix + toDoPrefix + _jobId,
              VPackValue(VPackValueType::Object));
  pending.add("op", VPackValue("delete"));
  pending.close();

  // --- Block toServer
  pending.add(_agencyPrefix + blockedServersPrefix + _server,
              VPackValue(VPackValueType::Object));
  pending.add("jobId", VPackValue(_jobId));
  pending.close();

  pending.close();

  // Preconditions
  // --- Check that toServer not blocked
  pending.openObject();
  pending.add(_agencyPrefix + blockedServersPrefix + _server,
              VPackValue(VPackValueType::Object));
  pending.add("oldEmpty", VPackValue(true));
  pending.close();

  pending.close();
  pending.close();

  // Transact to agency
  write_ret_t res = transact(_agent, pending);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC(INFO, Logger::AGENCY)
      << "Pending job for failed DB Server " << _server;

    auto const& databases = _snapshot("/Plan/Collections").children();
    auto const& current = _snapshot("/Current/Collections").children();

    size_t sub = 0;

    for (auto const& database : databases) {
      auto cdatabase = current.at(database.first)->children();

      for (auto const& collptr : database.second->children()) {
        Node const& collection = *(collptr.second);
        
        if (!cdatabase.find(collptr.first)->second->children().empty()) {
          Node const& collection = *(collptr.second);
          Node const& replicationFactor = collection("replicationFactor");
          if (replicationFactor.slice().getUInt() > 1) {
            auto available = availableServers();
            
              
            for (auto const& shard : collection("shards").children()) {

              size_t pos = 0;
              bool found = false;
              
              for (auto const& it : VPackArrayIterator(shard.second->slice())) {

                auto dbs = it.copyString();

                available.erase(
                  std::remove(available.begin(), available.end(), dbs),
                  available.end());

                if (dbs == _server) {
                  if (pos == 0) {
                    FailedLeader(
                      _snapshot, _agent, _jobId + "-" + std::to_string(sub++),
                      _jobId, _agencyPrefix, database.first, collptr.first,
                      shard.first, _server, shard.second->slice()[1].copyString());
                    continue;
                  } else {
                    found = true;
                  }
                }
                
                ++pos;
              }

              if (found && available.size() > 0) {
                auto randIt = available.begin();
                std::advance(randIt, std::rand() % available.size());
                FailedFollower(
                  _snapshot, _agent, _jobId + "-" + std::to_string(sub++),
                  _jobId, _agencyPrefix, database.first, collptr.first,
                  shard.first, _server, *randIt);
              }
            }
          }
        } else {
          for (auto const& shard : collection("shards").children()) {
            UnassumedLeadership(
              _snapshot, _agent, _jobId + "-" + std::to_string(sub++), _jobId,
              _agencyPrefix, database.first, collptr.first, shard.first, _server);
          }
        }
      }
    }

    return true;
  }

  LOG_TOPIC(INFO, Logger::AGENCY)
      << "Precondition failed for starting job " + _jobId;

  return false;
}

bool FailedServer::create() {
  LOG_TOPIC(INFO, Logger::AGENCY) << "Todo: DB Server " + _server + " failed.";

  std::string path = _agencyPrefix + toDoPrefix + _jobId;

  _jb = std::make_shared<Builder>();
  _jb->openArray();
  _jb->openObject();

  // Job entry
  _jb->add(path, VPackValue(VPackValueType::Object));
  _jb->add("type", VPackValue("failedServer"));
  _jb->add("server", VPackValue(_server));
  _jb->add("jobId", VPackValue(_jobId));
  _jb->add("creator", VPackValue(_creator));
  _jb->add("timeCreated",
           VPackValue(timepointToString(std::chrono::system_clock::now())));
  _jb->close();

  // Failed server entry
  path = _agencyPrefix + failedServersPrefix + "/" + _server;
  _jb->add(path, VPackValue(VPackValueType::Array));
  _jb->close();

  // Raise plan version
  path = _agencyPrefix + planVersion;
  _jb->add(path, VPackValue(VPackValueType::Object));
  _jb->add("op", VPackValue("increment"));
  _jb->close();

  _jb->close();
  _jb->close();

  write_ret_t res = transact(_agent, *_jb);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return true;
  }

  LOG_TOPIC(INFO, Logger::AGENCY) << "Failed to insert job " + _jobId;
  return false;
}

JOB_STATUS FailedServer::status() {
  auto status = exists();

  if (status != NOTFOUND) {  // Get job details from agency

    try {
      _server = _snapshot(pos[status] + _jobId + "/server").getString();
    } catch (std::exception const& e) {
      std::stringstream err;
      err << "Failed to find job " << _jobId << " in agency: " << e.what();
      LOG_TOPIC(ERR, Logger::AGENCY) << err.str();
      finish("DBServers/" + _server, false, err.str());
      return FAILED;
    }
  }

  if (status == PENDING) {
    auto const& serverHealth = _snapshot(healthPrefix + _server + "/Status").getString();

    // mop: ohhh...server is healthy again!
    bool serverHealthy = serverHealth == Supervision::HEALTH_STATUS_GOOD;

    std::shared_ptr<Builder> deleteTodos;

    Node::Children const todos = _snapshot(toDoPrefix).children();
    Node::Children const pends = _snapshot(pendingPrefix).children();
    bool hasOpenChildTasks = false;

    for (auto const& subJob : todos) {
      if (!subJob.first.compare(0, _jobId.size() + 1, _jobId + "-")) {
        if (serverHealthy) {
          if (!deleteTodos) {
            deleteTodos.reset(new Builder());
            deleteTodos->openArray();
            deleteTodos->openObject();
          }
          deleteTodos->add(_agencyPrefix + toDoPrefix + subJob.first, VPackValue(VPackValueType::Object));
          deleteTodos->add("op", VPackValue("delete"));
          deleteTodos->close();
        } else {
          hasOpenChildTasks = true;
        }
      }
    }

    for (auto const& subJob : pends) {
      if (!subJob.first.compare(0, _jobId.size() + 1, _jobId + "-")) {
        hasOpenChildTasks = true;
      }
    }

    if (deleteTodos) {
      LOG_TOPIC(INFO, Logger::AGENCY) << "Server " << _server << " is healthy again. Will try to delete any jobs which have not yet started!";
      deleteTodos->close();
      deleteTodos->close();
      // Transact to agency
      write_ret_t res = transact(_agent, *deleteTodos);

      if (!res.accepted || res.indices.size() != 1 || !res.indices[0]) {
        LOG_TOPIC(WARN, Logger::AGENCY)
          << "Server was healthy. Tried deleting subjobs but failed :(";
        return status;
      }
    }

    if (!hasOpenChildTasks) {
      if (finish("DBServers/" + _server)) {
        return FINISHED;
      }
    }
  }

  return status;
}
