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

#include "FailedServer.h"
#include "JobContext.h"

#include "Agency/AgentInterface.h"
#include "Agency/FailedFollower.h"
#include "Agency/FailedLeader.h"
#include "Agency/Job.h"
#include "Basics/StaticStrings.h"

using namespace arangodb::consensus;

FailedServer::FailedServer(Node const& snapshot, AgentInterface* agent,
                           std::string const& jobId, std::string const& creator,
                           std::string const& server)
    : Job(NOTFOUND, snapshot, agent, jobId, creator), _server(server) {}

FailedServer::FailedServer(Node const& snapshot, AgentInterface* agent,
                           JOB_STATUS status, std::string const& jobId)
    : Job(status, snapshot, agent, jobId) {
  // Get job details from jobId:
  std::string path = pos[status] + _jobId + "/";
  auto tmp_server = _snapshot.hasAsString(path + "server");
  auto tmp_creator = _snapshot.hasAsString(path + "creator");

  if (tmp_server.second && tmp_creator.second) {
    _server = tmp_server.first;
    _creator = tmp_creator.first;
  } else {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency.";
    LOG_TOPIC("ac06a", ERR, Logger::SUPERVISION) << err.str();
    finish(tmp_server.first, "", false, err.str());
    _status = FAILED;
  }
}

FailedServer::~FailedServer() = default;

void FailedServer::run(bool& aborts) { runHelper(_server, "", aborts); }

bool FailedServer::start(bool& aborts) {
  using namespace std::chrono;

  // Fail job, if Health back to not FAILED
  auto status = _snapshot.hasAsString(healthPrefix + _server + "/Status");
  if (status.second && status.first != "FAILED") {
    std::stringstream reason;
    reason << "Server " << _server << " is no longer failed. Not starting FailedServer job";
    LOG_TOPIC("a04da", INFO, Logger::SUPERVISION) << reason.str();
    finish(_server, "", false, reason.str());
    return false;
  } else if (!status.second) {
    std::stringstream reason;
    reason << "Server " << _server << " no longer in health. Already removed. Abort.";
    LOG_TOPIC("1479a", INFO, Logger::SUPERVISION) << reason.str();
    finish(_server, "", false, reason.str());  // Finish or abort?
    return false;
  }

  auto abortJob = [&aborts, this](Slice s) {
    auto jobId = s.copyString();
    if (!abortable(_snapshot, jobId)) {
      return false;
    }
    JobContext(PENDING, s.copyString(), _snapshot, _agent)
        .abort("failed server");
    aborts = true;
    return true;
  };

  std::pair<Slice, bool> dbserverLock =
      _snapshot.hasAsSlice(blockedServersPrefix + _server);
  if (auto const& s = dbserverLock.first; dbserverLock.second) {
    if (s.isArray()) {
      for (auto const& m : VPackArrayIterator(s)) {
        if (m.isString()) {
          if (!abortJob(m)) {
            return false;
          }
        } else {
          LOG_TOPIC("1479b", ERR, Logger::SUPERVISION)
              << "bad value in lock for server " << _server;
          TRI_ASSERT(false);
        }
      }
    } else if (s.isString()) {
      if (!abortJob(s)) {
        return false;
      }
    } else {
      LOG_TOPIC("1479c", ERR, Logger::SUPERVISION)
          << "bad value in lock for server " << _server;
      TRI_ASSERT(false);
    }
  }

  if (aborts) {
    return false;
  }

  // Todo entry
  Builder todo;
  {
    VPackArrayBuilder t(&todo);
    if (_jb == nullptr) {
      auto const& toDoJob = _snapshot.hasAsNode(toDoPrefix + _jobId);
      if (toDoJob.second) {
        toDoJob.first.toBuilder(todo);
      } else {
        LOG_TOPIC("729c3", INFO, Logger::SUPERVISION)
            << "Failed to get key " + toDoPrefix + _jobId +
                   " from agency snapshot";
        return false;
      }
    } else {
      todo.add(_jb->slice()[0].get(toDoPrefix + _jobId));
    }
  }  // Todo entry

  auto transactions = std::make_shared<VPackBuilder>();
  {
    VPackArrayBuilder a(transactions.get());

    // Operations -------------->
    {
      VPackObjectBuilder oper(transactions.get());
      // Add pending

      auto const& databases = _snapshot.hasAsChildren("/Plan/Collections").first;
      // auto const& current = _snapshot.hasAsChildren("/Current/Collections").first;

      size_t sub = 0;

      // FIXME: looks OK, but only the non-clone shards are put into the job
      for (auto const& database : databases) {
        // dead code   auto cdatabase = current.at(database.first)->children();

        for (auto const& collptr : database.second->children()) {
          auto const& collection = *(collptr.second);

          auto const& replicationFactorPair =
              collection.hasAsNode(StaticStrings::ReplicationFactor);
          if (replicationFactorPair.second) {
            VPackSlice const replicationFactor = replicationFactorPair.first.slice();
            uint64_t number = 1;
            bool isSatellite = false;

            if (replicationFactor.isString() &&
                replicationFactor.compareString(StaticStrings::Satellite) == 0) {
              isSatellite = true;  // do nothing - number = Job::availableServers(_snapshot).size();
            } else if (replicationFactor.isNumber()) {
              try {
                number = replicationFactor.getNumber<uint64_t>();
              } catch (...) {
                LOG_TOPIC("f5290", ERR, Logger::SUPERVISION)
                    << "Failed to read replicationFactor. job: " << _jobId
                    << " " << collection.hasAsString("id").first;
                continue;
              }

              if (number == 1) {
                continue;
              }
            } else {
              continue;  // no point to try salvaging unreplicated data
            }

            if (collection.has("distributeShardsLike")) {
              continue;  // we only deal with the master
            }

            for (auto const& shard : collection.hasAsChildren("shards").first) {
              size_t pos = 0;

              for (VPackSlice it : VPackArrayIterator(shard.second->slice())) {
                auto dbs = it.copyString();

                if (dbs == _server || dbs == "_" + _server) {
                  if (pos == 0) {
                    FailedLeader(_snapshot, _agent,
                                 _jobId + "-" + std::to_string(sub++), _jobId,
                                 database.first, collptr.first, shard.first, _server)
                        .create(transactions);
                  } else {
                    if (!isSatellite) {
                      FailedFollower(_snapshot, _agent,
                                     _jobId + "-" + std::to_string(sub++), _jobId,
                                     database.first, collptr.first, shard.first, _server)
                          .create(transactions);
                    } else {
                      LOG_TOPIC("c6c32", DEBUG, Logger::SUPERVISION)
                          << "Do intentionally nothing for failed follower of "
                             "SatelliteCollection. job: "
                          << _jobId;
                    }
                  }
                }
                pos++;
              }
            }
          }
        }
      }

      transactions->add(VPackValue(pendingPrefix + _jobId));
      {
        VPackObjectBuilder ts(transactions.get());
        transactions->add("timeStarted",
                          VPackValue(timepointToString(system_clock::now())));
        for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
          transactions->add(obj.key.copyString(), obj.value);
        }
      }
      // Delete todo
      addRemoveJobFromSomewhere(*transactions, "ToDo", _jobId);
      addBlockServer(*transactions, _server, _jobId);
      // We increment the rebootId of the failed server to let the RebootId
      // tracker do its magic to recognise the failure soon.
      addIncreaseRebootId(*transactions, _server);
    }  // <------------ Operations

    // Preconditions ----------->
    {
      VPackObjectBuilder prec(transactions.get());
      // Check that toServer not blocked
      addPreconditionServerNotBlocked(*transactions, _server);
      // Status should still be FAILED
      addPreconditionServerHealth(*transactions, _server, "FAILED");
    }  // <--------- Preconditions
  }

  // Transact to agency
  write_ret_t res = singleWriteTransaction(_agent, *transactions, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC("bbd90", DEBUG, Logger::SUPERVISION)
        << "Pending job for failed DB Server " << _server;

    return true;
  }

  LOG_TOPIC("a3459", INFO, Logger::SUPERVISION)
      << "Precondition failed for starting FailedServer " + _jobId;

  return false;
}

bool FailedServer::create(std::shared_ptr<VPackBuilder> envelope) {
  LOG_TOPIC("352fa", DEBUG, Logger::SUPERVISION)
      << "Todo: Handle failover for db server " + _server;

  using namespace std::chrono;
  bool selfCreate = (envelope == nullptr);  // Do we create ourselves?

  if (selfCreate) {
    _jb = std::make_shared<Builder>();
  } else {
    _jb = envelope;
  }

  {
    VPackArrayBuilder a(_jb.get());

    // Operations
    {
      VPackObjectBuilder operations(_jb.get());
      // ToDo entry
      _jb->add(VPackValue(toDoPrefix + _jobId));
      {
        VPackObjectBuilder todo(_jb.get());
        _jb->add("type", VPackValue("failedServer"));
        _jb->add("server", VPackValue(_server));
        _jb->add("jobId", VPackValue(_jobId));
        _jb->add("creator", VPackValue(_creator));
        _jb->add("timeCreated", VPackValue(timepointToString(system_clock::now())));
      }
      // FailedServers entry []
      _jb->add(VPackValue(failedServersPrefix + "/" + _server));
      { VPackArrayBuilder failedServers(_jb.get()); }
    }  // Operations

    // Preconditions
    {
      VPackObjectBuilder health(_jb.get());
      // Status should still be BAD
      addPreconditionServerHealth(*_jb, _server, "BAD");
      // Target/FailedServers does not already include _server
      _jb->add(VPackValue(failedServersPrefix + "/" + _server));
      {
        VPackObjectBuilder old(_jb.get());
        _jb->add("oldEmpty", VPackValue(true));
      }
      // Target/FailedServers is still as in the snapshot
      _jb->add(VPackValue(failedServersPrefix));
      {
        VPackObjectBuilder old(_jb.get());
        _jb->add("old", _snapshot.hasAsBuilder(failedServersPrefix).first.slice());
      }
    }  // Preconditions
  }

  if (selfCreate) {
    write_ret_t res = singleWriteTransaction(_agent, *_jb, false);
    if (!res.accepted || res.indices.size() != 1 || res.indices[0] == 0) {
      LOG_TOPIC("70ce1", INFO, Logger::SUPERVISION) << "Failed to insert job " + _jobId;
      return false;
    }
  }

  return true;
}

JOB_STATUS FailedServer::status() {
  if (_status != PENDING) {
    return _status;
  }

  auto serverHealth = _snapshot.hasAsString(healthPrefix + _server + "/Status");

  // mop: ohhh...server is healthy again!
  bool serverHealthy = serverHealth.second && serverHealth.first == Supervision::HEALTH_STATUS_GOOD;

  std::shared_ptr<Builder> deleteTodos;

  Node::Children const& todos = _snapshot.hasAsChildren(toDoPrefix).first;
  Node::Children const& pends = _snapshot.hasAsChildren(pendingPrefix).first;
  bool hasOpenChildTasks = false;

  for (auto const& subJob : todos) {
    if (!subJob.first.compare(0, _jobId.size() + 1, _jobId + "-")) {
      if (serverHealthy) {
        if (!deleteTodos) {
          deleteTodos.reset(new Builder());
          deleteTodos->openArray();
          deleteTodos->openObject();
        }
        deleteTodos->add(toDoPrefix + subJob.first, VPackValue(VPackValueType::Object));
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
    LOG_TOPIC("7a010", INFO, Logger::SUPERVISION)
        << "Server " << _server
        << " is healthy again. Will try to delete"
           " any jobs which have not yet started!";
    deleteTodos->close();
    deleteTodos->close();
    // Transact to agency
    write_ret_t res = singleWriteTransaction(_agent, *deleteTodos, false);

    if (!res.accepted || res.indices.size() != 1 || !res.indices[0]) {
      LOG_TOPIC("e8735", WARN, Logger::SUPERVISION)
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

arangodb::Result FailedServer::abort(std::string const& reason) {
  return Result{};
  // FIXME: No abort procedure, simply throw error or so
  // ??????????????
}
