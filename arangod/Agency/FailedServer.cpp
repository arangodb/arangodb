
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
#include "JobContext.h"

#include "Agency/AgentInterface.h"
#include "Agency/FailedLeader.h"
#include "Agency/FailedFollower.h"
#include "Agency/Job.h"

using namespace arangodb::consensus;

FailedServer::FailedServer(
  Node const& snapshot, AgentInterface* agent, std::string const& jobId,
  std::string const& creator, std::string const& server)
  : Job(NOTFOUND, snapshot, agent, jobId, creator), _server(server) {}

FailedServer::FailedServer(Node const& snapshot, AgentInterface* agent,
                           JOB_STATUS status, std::string const& jobId)
    : Job(status, snapshot, agent, jobId) {
  // Get job details from jobId:
  try {
    std::string path = pos[status] + _jobId + "/";
    _server = _snapshot(path + "server").getString();
    _creator = _snapshot(path + "creator").getString();
  } catch (std::exception const& e) {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency: " << e.what();
    LOG_TOPIC(ERR, Logger::SUPERVISION) << err.str();
    finish(_server, "", false, err.str());
    _status = FAILED;
  }
}

FailedServer::~FailedServer() {}

void FailedServer::run() {
  runHelper(_server, "");
}

bool FailedServer::start() {
  
  using namespace std::chrono;

  // Fail job, if Health back to not FAILED
  if (_snapshot(healthPrefix + _server + "/Status").getString() != "FAILED") {
    std::stringstream reason;
    reason
      << "Server " << _server
      << " is no longer failed. Not starting FailedServer job";
    LOG_TOPIC(INFO, Logger::SUPERVISION) << reason.str();
    finish(_server, "", false, reason.str());
    return false;
  }
  
  // Abort job blocking server if abortable
  try {
     std::string jobId = _snapshot(blockedServersPrefix + _server).getString();
    if (!abortable(_snapshot, jobId)) {
      return false;
    } else {
      JobContext(PENDING, jobId, _snapshot, _agent).abort();
    }
  } catch (...) {}
    
  // Todo entry
  Builder todo;
  { VPackArrayBuilder t(&todo);
    if (_jb == nullptr) {
      try {
        _snapshot(toDoPrefix + _jobId).toBuilder(todo);
      } catch (std::exception const&) {
        LOG_TOPIC(INFO, Logger::SUPERVISION)
          << "Failed to get key " + toDoPrefix + _jobId + " from agency snapshot";
        return false;
      }
    } else {
      todo.add(_jb->slice()[0].get(toDoPrefix + _jobId));
    }} // Todo entry
  
  // Pending entry
  Builder pending;
  { VPackArrayBuilder a(&pending);
    
    // Operations -------------->
    { VPackObjectBuilder oper(&pending); 
      // Add pending
      pending.add(VPackValue(pendingPrefix + _jobId));
      { VPackObjectBuilder ts(&pending);
        pending.add("timeStarted",
                    VPackValue(timepointToString(system_clock::now())));
        for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
          pending.add(obj.key.copyString(), obj.value);
        }
      }
      // Delete todo
      addRemoveJobFromSomewhere(pending, "ToDo", _jobId);
      addBlockServer(pending, _server, _jobId);
    } // <------------ Operations
    
    // Preconditions ----------->
    { VPackObjectBuilder prec(&pending);
      // Check that toServer not blocked
      addPreconditionServerNotBlocked(pending, _server);
      // Status should still be FAILED
      addPreconditionServerHealth(pending, _server, "FAILED");
    } // <--------- Preconditions
  }

  
  // Transact to agency
  write_ret_t res = singleWriteTransaction(_agent, pending);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "Pending job for failed DB Server " << _server;

    auto const& databases = _snapshot("/Plan/Collections").children();
    auto const& current = _snapshot("/Current/Collections").children();

    size_t sub = 0;

    // FIXME: looks OK, but only the non-clone shards are put into the job
    for (auto const& database : databases) {
      auto cdatabase = current.at(database.first)->children();

      for (auto const& collptr : database.second->children()) {
        auto const& collection = *(collptr.second);

        auto const& replicationFactor = collection("replicationFactor");

        if (replicationFactor.slice().getUInt() == 1) {
          continue;  // no point to try salvaging unreplicated data
        }

        if (collection.has("distributeShardsLike")) {
          continue;  // we only deal with the master
        }

        for (auto const& shard : collection("shards").children()) {

          size_t pos = 0;

          for (auto const& it : VPackArrayIterator(shard.second->slice())) {

            auto dbs = it.copyString();

            if (dbs == _server) {
              if (pos == 0) {
                FailedLeader(
                  _snapshot, _agent, _jobId + "-" + std::to_string(sub++),
                  _jobId, database.first, collptr.first,
                  shard.first, _server).run();
              } else {
                FailedFollower(
                  _snapshot, _agent, _jobId + "-" + std::to_string(sub++),
                  _jobId, database.first, collptr.first,
                  shard.first, _server).run();
              }
            }
            pos++;
          }
        }
      }
    }

    return true;
  }

  LOG_TOPIC(INFO, Logger::SUPERVISION)
      << "Precondition failed for starting FailedServer " + _jobId;

  return false;
}

bool FailedServer::create(std::shared_ptr<VPackBuilder> envelope) {

  LOG_TOPIC(DEBUG, Logger::SUPERVISION)
    << "Todo: Handle failover for db server " + _server;

  using namespace std::chrono;
  bool selfCreate = (envelope == nullptr); // Do we create ourselves?

  if (selfCreate) {
    _jb = std::make_shared<Builder>();
  } else {
    _jb = envelope;
  }


  { VPackArrayBuilder a(_jb.get());

    // Operations
    { VPackObjectBuilder operations (_jb.get());
      // ToDo entry
      _jb->add(VPackValue(toDoPrefix + _jobId));
      { VPackObjectBuilder todo(_jb.get());
        _jb->add("type", VPackValue("failedServer"));
        _jb->add("server", VPackValue(_server));
        _jb->add("jobId", VPackValue(_jobId));
        _jb->add("creator", VPackValue(_creator));
        _jb->add("timeCreated",
                 VPackValue(timepointToString(system_clock::now()))); }
      // FailedServers entry []
      _jb->add(VPackValue(failedServersPrefix + "/" + _server));
      { VPackArrayBuilder failedServers(_jb.get()); }} // Operations

    //Preconditions
    { VPackObjectBuilder health(_jb.get());
      // Status should still be BAD
      addPreconditionServerHealth(*_jb, _server, "BAD");
      // Target/FailedServers does not already include _server
      _jb->add(VPackValue(failedServersPrefix + "/" + _server));
      { VPackObjectBuilder old(_jb.get());
        _jb->add("oldEmpty", VPackValue(true)); }
      // Target/FailedServers is still as in the snapshot
      _jb->add(VPackValue(failedServersPrefix));
      { VPackObjectBuilder old(_jb.get());
        _jb->add("old", _snapshot(failedServersPrefix).toBuilder().slice());}
    } // Preconditions
  }

  if (selfCreate) {
    write_ret_t res = singleWriteTransaction(_agent, *_jb);
    if (!res.accepted || res.indices.size() != 1 || res.indices[0] == 0) {
      LOG_TOPIC(INFO, Logger::SUPERVISION) << "Failed to insert job " + _jobId;
      return false;
    }
  }

  return true;

}

JOB_STATUS FailedServer::status() {
  if (_status != PENDING) {
    return _status;
  }

  auto const& serverHealth =
    _snapshot(healthPrefix + _server + "/Status").getString();

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
        deleteTodos->add( toDoPrefix + subJob.first,
          VPackValue(VPackValueType::Object));
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

  // FIXME: sub-jobs should terminate themselves if server "GOOD" again
  // FIXME: thus the deleteTodos here is unnecessary

  if (deleteTodos) {
    LOG_TOPIC(INFO, Logger::SUPERVISION)
      << "Server " << _server << " is healthy again. Will try to delete"
      " any jobs which have not yet started!";
    deleteTodos->close();
    deleteTodos->close();
    // Transact to agency
    write_ret_t res = singleWriteTransaction(_agent, *deleteTodos);

    if (!res.accepted || res.indices.size() != 1 || !res.indices[0]) {
      LOG_TOPIC(WARN, Logger::SUPERVISION)
        << "Server was healthy. Tried deleting subjobs but failed :(";
      return _status;
    }
  }

  // FIXME: what if some subjobs have failed, we should fail then
  if (!hasOpenChildTasks) {
    if (finish(_server, "")) {
      return FINISHED;
    }
  }

  return _status;
}

arangodb::Result FailedServer::abort() {
  Result result;
  return result;
  // FIXME: No abort procedure, simply throw error or so
}

