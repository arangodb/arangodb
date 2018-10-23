////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ActiveFailoverJob.h"

#include "Agency/AgentInterface.h"
#include "Agency/Job.h"
#include "Agency/JobContext.h"
#include "Agency/Store.h"
#include "Cluster/ClusterHelpers.h"
#include "VocBase/voc-types.h"

using namespace arangodb;
using namespace arangodb::consensus;

ActiveFailoverJob::ActiveFailoverJob(Node const& snapshot, AgentInterface* agent,
                                     std::string const& jobId, std::string const& creator,
                                     std::string const& failed)
    : Job(NOTFOUND, snapshot, agent, jobId, creator),
      _server(failed) { }

ActiveFailoverJob::ActiveFailoverJob(Node const& snapshot, AgentInterface* agent,
                                     JOB_STATUS status, std::string const& jobId)
    : Job(status, snapshot, agent, jobId) {
  // Get job details from agency:
  std::string path = pos[status] + _jobId + "/";
  auto tmp_server = _snapshot.hasAsString(path + "server");
  auto tmp_creator = _snapshot.hasAsString(path + "creator");
  
  if (tmp_server.second && tmp_creator.second) {
    _server = tmp_server.first;
    _creator = tmp_creator.first;
  } else {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency.";
    LOG_TOPIC(ERR, Logger::SUPERVISION) << err.str();
    finish(tmp_server.first, "", false, err.str());
    _status = FAILED;
  }
}

ActiveFailoverJob::~ActiveFailoverJob() {}

void ActiveFailoverJob::run() {
  runHelper(_server, "");
}

bool ActiveFailoverJob::create(std::shared_ptr<VPackBuilder> envelope) {

  LOG_TOPIC(DEBUG, Logger::SUPERVISION)
    << "Todo: Handle failover for leader " + _server;
  
  bool selfCreate = (envelope == nullptr); // Do we create ourselves?

  if (selfCreate) {
    _jb = std::make_shared<Builder>();
  } else {
    _jb = envelope;
  }
  
  auto now = timepointToString(std::chrono::system_clock::now());  
  { VPackArrayBuilder transaction(_jb.get());
    { VPackObjectBuilder operations(_jb.get());
      // Todo entry
      _jb->add(VPackValue(toDoPrefix + _jobId));
      { VPackObjectBuilder todo(_jb.get());
        _jb->add("creator", VPackValue(_creator));
        _jb->add("type", VPackValue("activeFailover"));
        _jb->add("server", VPackValue(_server));
        _jb->add("jobId", VPackValue(_jobId));
        _jb->add("timeCreated", VPackValue(now));
      } // todo
      
      // FailedServers entry []
      _jb->add(VPackValue(failedServersPrefix + "/" + _server));
      { VPackArrayBuilder failedServers(_jb.get()); }
    } // Operations
    
    //Preconditions
    { VPackObjectBuilder health(_jb.get());
      // Status should still be BAD
      addPreconditionServerHealth(*_jb, _server, Supervision::HEALTH_STATUS_BAD);
      // Target/FailedServers does not already include _server
      _jb->add(VPackValue(failedServersPrefix + "/" + _server));
      { VPackObjectBuilder old(_jb.get());
        _jb->add("oldEmpty", VPackValue(true)); }
      // Target/FailedServers is still as in the snapshot
      _jb->add(VPackValue(failedServersPrefix));
      { VPackObjectBuilder old(_jb.get());
        _jb->add("old", _snapshot(failedServersPrefix).toBuilder().slice());}
    } // Preconditions
  } // transactions
  
  _status = TODO;
  
  if (!selfCreate) {
    return true;
  }
  
  write_ret_t res = singleWriteTransaction(_agent, *_jb);
  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return true;
  }
  
  _status = NOTFOUND;
  
  LOG_TOPIC(INFO, Logger::SUPERVISION) << "Failed to insert job " + _jobId;
  return false;
}

bool ActiveFailoverJob::start() {
  // If anything throws here, the run() method catches it and finishes
  // the job.
  
  // Fail job, if Health back to not FAILED
  if (checkServerHealth(_snapshot, _server) != Supervision::HEALTH_STATUS_FAILED) {
    std::string reason = "Server " + _server + " is no longer failed. " +
                         "Not starting ActiveFailoverJob job";
    LOG_TOPIC(INFO, Logger::SUPERVISION) << reason;
    return finish(_server, "", true, reason); // move to /Target/Finished
  }
  
  auto leader = _snapshot.hasAsSlice(asyncReplLeader);
  if (!leader.second || leader.first.compareString(_server) != 0) {
    std::string reason = "Server " + _server + " is not the current replication leader";
    LOG_TOPIC(INFO, Logger::SUPERVISION) << reason;
    return finish(_server, "", true, reason); // move to /Target/Finished
  }
  
  // Abort job blocking server if abortable
  auto jobId = _snapshot.hasAsString(blockedServersPrefix + _server);
  if (jobId.second && !abortable(_snapshot, jobId.first)) {
    return false;
  } else if (jobId.second) {
    JobContext(PENDING, jobId.first, _snapshot, _agent).abort();
  }
  
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
  
  std::string newLeader = findBestFollower();
  if (newLeader.empty() || _server == newLeader) {
    LOG_TOPIC(INFO, Logger::SUPERVISION) << "No follower for fail-over available, will retry";
    return false; // job will retry later
  }
  LOG_TOPIC(INFO, Logger::SUPERVISION) << "Selected '" << newLeader << "' as leader";
  
  // Enter pending, remove todo
  Builder pending;
  { VPackArrayBuilder listOfTransactions(&pending);
    
    { VPackObjectBuilder operations(&pending);
      addPutJobIntoSomewhere(pending, "Finished", todo.slice()[0]);
      addRemoveJobFromSomewhere(pending, "ToDo", _jobId);
      pending.add(asyncReplLeader, VPackValue(newLeader));
    }  // mutation part of transaction done
    
    // Preconditions
    { VPackObjectBuilder precondition(&pending);
      // Failed condition persists
      addPreconditionServerHealth(pending, _server, Supervision::HEALTH_STATUS_FAILED);
      // Destination server still in good condition
      addPreconditionServerHealth(pending, newLeader, Supervision::HEALTH_STATUS_GOOD);
      // Destination server should not be blocked by another job
      addPreconditionServerNotBlocked(pending, newLeader);
      // AsyncReplication leader must be the failed server
      addPreconditionUnchanged(pending, asyncReplLeader, leader.first);
    }   // precondition done
    
  }  // array for transaction done
  
  // Transact to agency
  write_ret_t res = singleWriteTransaction(_agent, pending);
  
  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    _status = FINISHED;
    LOG_TOPIC(INFO, Logger::SUPERVISION)
    << "Finished: ActiveFailoverJob server " << _server << " failover to " << newLeader;
    return true;
  }
  
  LOG_TOPIC(INFO, Logger::SUPERVISION) << "Precondition failed for ActiveFailoverJob " + _jobId;
  return false;
}

JOB_STATUS ActiveFailoverJob::status() {
  if (_status != PENDING) {
    return _status;
  }
  
  TRI_ASSERT(false);   // PENDING is not an option for this job, since it
  // travels directly from ToDo to Finished or Failed
  return _status;
}

arangodb::Result ActiveFailoverJob::abort() {

  // We can assume that the job is in ToDo or not there:
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    return Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                  "Failed aborting addFollower job beyond pending stage");
  }
  
  Result result;
  // Can now only be TODO or PENDING
  if (_status == TODO) {
    finish("", "", false, "job aborted");
    return result;
  }
  
  TRI_ASSERT(false);  // cannot happen, since job moves directly to FINISHED
  return result;
  
}

typedef std::pair<std::string, TRI_voc_tick_t> ServerTick;
/// Try to select the follower most in-sync with failed leader
std::string ActiveFailoverJob::findBestFollower() {
  std::vector<std::string> healthy = healthyServers(_snapshot);
  // the failed leader should never appear as healthy
  TRI_ASSERT(std::find(healthy.begin(), healthy.end(), _server) == healthy.end());
  
  // blocked; (not sure if this can even happen)
  try {
    for (auto const& srv : _snapshot(blockedServersPrefix).children()) {
      healthy.erase(std::remove(healthy.begin(), healthy.end(), srv.first), healthy.end());
    }
  } catch (...) {}
  
  std::vector<ServerTick> ticks;
  try {
    // collect tick values from transient state
    query_t trx = std::make_unique<VPackBuilder>();
    {
      VPackArrayBuilder transactions(trx.get());
      VPackArrayBuilder operations(trx.get());
      trx->add(VPackValue("/" + Job::agencyPrefix + asyncReplTransientPrefix));
    }
    trans_ret_t res = _agent->transient(std::move(trx));
    if (!res.accepted) {
      LOG_TOPIC(ERR, Logger::SUPERVISION) << "could not read from transient while"
      << " determining follower ticks";
      return "";
    }
    VPackSlice resp = res.result->slice();
    if (!resp.isArray() || resp.length() == 0) {
      LOG_TOPIC(ERR, Logger::SUPERVISION) << "no follower ticks in transient store";
      return "";
    }
    
    VPackSlice obj = resp.at(0).get<std::string>({ Job::agencyPrefix, std::string("AsyncReplication") });
    for (VPackObjectIterator::ObjectPair pair : VPackObjectIterator(obj)) {
      std::string srvUUID = pair.key.copyString();
      bool isAvailable = std::find(healthy.begin(), healthy.end(), srvUUID) != healthy.end();
      if (!isAvailable) {
        continue; // skip inaccessible servers
      }
      TRI_ASSERT(srvUUID != _server); // assumption: _server is unhealthy
      
      VPackSlice leader = pair.value.get("leader"); // broken leader
      VPackSlice lastTick = pair.value.get("lastTick");
      if (leader.isString() && leader.isEqualString(_server) &&
          lastTick.isNumber()) {
        ticks.emplace_back(std::move(srvUUID), lastTick.getUInt());
      }
    }
  } catch (basics::Exception const& e) {
    LOG_TOPIC(ERR, Logger::SUPERVISION) << "could not determine follower: " << e.message();
  } catch(std::exception const& e) {
    LOG_TOPIC(ERR, Logger::SUPERVISION) << "could not determine follower: " << e.what();
  } catch(...) {
    LOG_TOPIC(ERR, Logger::SUPERVISION) << "internal error while determining best follower";
  }
  
  std::sort(ticks.begin(), ticks.end(), [&](ServerTick const& a,
                                            ServerTick const& b) {
    return a.second > b.second;
  });
  if (!ticks.empty()) {
    TRI_ASSERT(ticks.size() == 1 || ticks[0].second >= ticks[1].second);
    return ticks[0].first;
  }
  LOG_TOPIC(ERR, Logger::SUPERVISION) << "no follower ticks available";
  return ""; // fallback to any available server
}
