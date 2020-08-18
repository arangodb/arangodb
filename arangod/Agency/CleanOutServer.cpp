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

#include "CleanOutServer.h"

#include "Agency/AgentInterface.h"
#include "Agency/Job.h"
#include "Agency/JobContext.h"
#include "Agency/MoveShard.h"
#include "Basics/StaticStrings.h"
#include "Random/RandomGenerator.h"

using namespace arangodb::consensus;

CleanOutServer::CleanOutServer(Node const& snapshot, AgentInterface* agent,
                               std::string const& jobId, std::string const& creator,
                               std::string const& server)
    : Job(NOTFOUND, snapshot, agent, jobId, creator), _server(id(server)) {}

CleanOutServer::CleanOutServer(Node const& snapshot, AgentInterface* agent,
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
    LOG_TOPIC("38962", ERR, Logger::SUPERVISION) << err.str();
    finish(tmp_server.first, "", false, err.str());
    _status = FAILED;
  }
}

CleanOutServer::~CleanOutServer() = default;

void CleanOutServer::run(bool& aborts) { runHelper(_server, "", aborts); }

JOB_STATUS CleanOutServer::status() {
  if (_status != PENDING) {
    return _status;
  }

  Node::Children const& todos = _snapshot.hasAsChildren(toDoPrefix).first;
  Node::Children const& pends = _snapshot.hasAsChildren(pendingPrefix).first;
  size_t found = 0;

  for (auto const& subJob : todos) {
    if (!subJob.first.compare(0, _jobId.size() + 1, _jobId + "-")) {
      found++;
    }
  }
  for (auto const& subJob : pends) {
    if (!subJob.first.compare(0, _jobId.size() + 1, _jobId + "-")) {
      found++;
    }
  }

  if (found > 0) {  // some subjob still running
    // timeout here:
    auto tmp_time =
        _snapshot.hasAsString(pendingPrefix + _jobId + "/timeCreated");
    std::string timeCreatedString = tmp_time.first;
    Supervision::TimePoint timeCreated = stringToTimepoint(timeCreatedString);
    Supervision::TimePoint now(std::chrono::system_clock::now());
    if (now - timeCreated > std::chrono::duration<double>(86400.0)) { // 1 day
      abort("job timed out");
      return FAILED;
    }
    return PENDING;
  }

  Node::Children const& failed = _snapshot.hasAsChildren(failedPrefix).first;
  size_t failedFound = 0;
  for (auto const& subJob : failed) {
    if (!subJob.first.compare(0, _jobId.size() + 1, _jobId + "-")) {
      failedFound++;
    }
  }

  if (failedFound > 0) {
    abort("child job failed");
    return FAILED;
  }

  // all subjobs done:

  // Put server in /Target/CleanedServers:
  Builder reportTrx;
  {
    VPackArrayBuilder arrayGuard(&reportTrx);
    {
      VPackObjectBuilder objectGuard(&reportTrx);
      reportTrx.add(VPackValue("/Target/CleanedServers"));
      {
        VPackObjectBuilder guard4(&reportTrx);
        reportTrx.add("op", VPackValue("push"));
        reportTrx.add("new", VPackValue(_server));
      }
      reportTrx.add(VPackValue(toBeCleanedPrefix));
      {
        VPackObjectBuilder guard4(&reportTrx);
        reportTrx.add("op", VPackValue("erase"));
        reportTrx.add("val", VPackValue(_server));
      }
      addRemoveJobFromSomewhere(reportTrx, "Pending", _jobId);
      Builder job;
      _snapshot.hasAsBuilder(pendingPrefix + _jobId, job);
      addPutJobIntoSomewhere(reportTrx, "Finished", job.slice(), "");
      addReleaseServer(reportTrx, _server);
    }
  }

  // Transact to agency
  write_ret_t res = singleWriteTransaction(_agent, reportTrx, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0] != 0) {
    LOG_TOPIC("dd49e", DEBUG, Logger::SUPERVISION)
        << "Have reported " << _server << " in /Target/CleanedServers";
    return FINISHED;
  }

  LOG_TOPIC("21acb", ERR, Logger::SUPERVISION)
      << "Failed to report " << _server << " in /Target/CleanedServers";
  return FAILED;
}

bool CleanOutServer::create(std::shared_ptr<VPackBuilder> envelope) {
  LOG_TOPIC("8a94c", DEBUG, Logger::SUPERVISION)
      << "Todo: Clean out server " + _server + " for shrinkage";

  bool selfCreate = (envelope == nullptr);  // Do we create ourselves?

  if (selfCreate) {
    _jb = std::make_shared<Builder>();
  } else {
    _jb = envelope;
  }

  std::string path = toDoPrefix + _jobId;

  {
    VPackArrayBuilder guard(_jb.get());
    VPackObjectBuilder guard2(_jb.get());
    _jb->add(VPackValue(path));
    {
      VPackObjectBuilder guard3(_jb.get());
      _jb->add("type", VPackValue("cleanOutServer"));
      _jb->add("server", VPackValue(_server));
      _jb->add("jobId", VPackValue(_jobId));
      _jb->add("creator", VPackValue(_creator));
      _jb->add("timeCreated",
               VPackValue(timepointToString(std::chrono::system_clock::now())));
    }
  }

  _status = TODO;

  if (!selfCreate) {
    return true;
  }

  write_ret_t res = singleWriteTransaction(_agent, *_jb, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return true;
  }

  _status = NOTFOUND;

  LOG_TOPIC("525fa", INFO, Logger::SUPERVISION) << "Failed to insert job " + _jobId;
  return false;
}

bool CleanOutServer::start(bool& aborts) {
  // If anything throws here, the run() method catches it and finishes
  // the job.

  // Check if the server exists:
  if (!_snapshot.has(plannedServers + "/" + _server)) {
    finish("", "", false, "server does not exist as DBServer in Plan");
    return false;
  }

  // Check that the server is not locked:
  if (_snapshot.has(blockedServersPrefix + _server)) {
    LOG_TOPIC("7a453", DEBUG, Logger::SUPERVISION)
        << "server " << _server
        << " is currently locked, not starting CleanOutServer job " << _jobId;
    return false;
  }

  // Check that the server is in state "GOOD":
  std::string health = checkServerHealth(_snapshot, _server);
  if (health != "GOOD") {
    LOG_TOPIC("a7580", DEBUG, Logger::SUPERVISION)
        << "server " << _server << " is currently " << health
        << ", not starting CleanOutServer job " << _jobId;
    return false;
  }

  // Check that _to is not in `Target/CleanedServers`:
  VPackBuilder cleanedServersBuilder;
  auto const& cleanedServersNode = _snapshot.hasAsNode(cleanedPrefix);
  if (cleanedServersNode.second) {
    cleanedServersNode.first.toBuilder(cleanedServersBuilder);
  } else {
    // ignore this check
    cleanedServersBuilder.clear();
    { VPackArrayBuilder guard(&cleanedServersBuilder); }
  }
  VPackSlice cleanedServers = cleanedServersBuilder.slice();
  if (cleanedServers.isArray()) {
    for (VPackSlice x : VPackArrayIterator(cleanedServers)) {
      if (x.isString() && x.copyString() == _server) {
        finish("", "", false, "server must not be in `Target/CleanedServers`");
        return false;
      }
    }
  }

  // Check that _to is not in `Target/FailedServers`:
  //  (this node is expected to NOT exists, so make test before processing
  //   so that hasAsNode does not generate a warning log message)
  VPackBuilder failedServersBuilder;
  if (_snapshot.has(failedServersPrefix)) {
    auto const& failedServersNode = _snapshot.hasAsNode(failedServersPrefix);
    if (failedServersNode.second) {
      failedServersNode.first.toBuilder(failedServersBuilder);
    } else {
      // ignore this check
      failedServersBuilder.clear();
      { VPackObjectBuilder guard(&failedServersBuilder); }
    }
  } else {
    // ignore this check
    failedServersBuilder.clear();
    { VPackObjectBuilder guard(&failedServersBuilder); }
  } // if

  VPackSlice failedServers = failedServersBuilder.slice();
  if (failedServers.isObject()) {
    Slice found = failedServers.get(_server);
    if (!found.isNone()) {
      finish("", "", false, "server must not be in `Target/FailedServers`");
      return false;
    }
  }

  // Check if we can get things done in the first place
  if (!checkFeasibility()) {
    finish("", "", false, "server " + _server + " cannot be cleaned out");
    return false;
  }

  // Copy todo to pending
  auto pending = std::make_shared<Builder>();
  Builder todo;

  // Get todo entry
  {
    VPackArrayBuilder guard(&todo);
    // When create() was done with the current snapshot, then the job object
    // will not be in the snapshot under ToDo, but in this case we find it
    // in _jb:
    if (_jb == nullptr) {
      auto tmp_todo = _snapshot.hasAsBuilder(toDoPrefix + _jobId, todo);
      if (!tmp_todo.second) {
        // Just in case, this is never going to happen, since we will only
        // call the start() method if the job is already in ToDo.
        LOG_TOPIC("1e9a9", INFO, Logger::SUPERVISION) << "Failed to get key " + toDoPrefix + _jobId +
                                                    " from agency snapshot";
        return false;
      }
    } else {
      try {
        todo.add(_jb->slice()[0].get(toDoPrefix + _jobId));
      } catch (std::exception const& e) {
        // Just in case, this is never going to happen, since when _jb is
        // set, then the current job is stored under ToDo.
        LOG_TOPIC("7b79a", WARN, Logger::SUPERVISION)
            << e.what() << ": " << __FILE__ << ":" << __LINE__;
        return false;
      }
    }
  }

  // Enter pending, remove todo, block toserver
  {
    VPackArrayBuilder listOfTransactions(pending.get());

    {
      VPackObjectBuilder objectForMutation(pending.get());

      addPutJobIntoSomewhere(*pending, "Pending", todo.slice()[0]);
      addRemoveJobFromSomewhere(*pending, "ToDo", _jobId);

      addBlockServer(*pending, _server, _jobId);

      // Put ourselves in list of servers to be cleaned:
      pending->add(VPackValue(toBeCleanedPrefix));
      {
        VPackObjectBuilder guard4(pending.get());
        pending->add("op", VPackValue("push"));
        pending->add("new", VPackValue(_server));
      }

      // Schedule shard relocations
      if (!scheduleMoveShards(pending)) {
        finish("", "", false, "Could not schedule MoveShard.");
        return false;
      }

    }  // mutation part of transaction done

    // Preconditions
    {
      VPackObjectBuilder objectForPrecondition(pending.get());
      addPreconditionServerNotBlocked(*pending, _server);
      addPreconditionServerHealth(*pending, _server, "GOOD");
      addPreconditionUnchanged(*pending, failedServersPrefix, failedServers);
      addPreconditionUnchanged(*pending, cleanedPrefix, cleanedServers);
      addPreconditionUnchanged(*pending, planVersion, _snapshot(planVersion).slice());
    }
  }  // array for transaction done

  // Transact to agency
  write_ret_t res = singleWriteTransaction(_agent, *pending, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC("e341c", DEBUG, Logger::SUPERVISION) << "Pending: Clean out server " + _server;

    return true;
  }

  LOG_TOPIC("3a348", INFO, Logger::SUPERVISION)
      << "Precondition failed for starting CleanOutServer job " + _jobId;

  return false;
}

bool CleanOutServer::scheduleMoveShards(std::shared_ptr<Builder>& trx) {
  std::vector<std::string> servers = availableServers(_snapshot);

  Node::Children const& databases = _snapshot.hasAsChildren("/Plan/Collections").first;
  size_t sub = 0;

  for (auto const& database : databases) {
    // Find shardsLike dependencies
    for (auto const& collptr : database.second->children()) {
      auto const& collection = *(collptr.second);

      if (collection.has("distributeShardsLike")) {
        continue;
      }

      for (auto const& shard : collection.hasAsChildren("shards").first) {
        // Only shards, which are affected
        int found = -1;
        int count = 0;
        for (VPackSlice dbserver : VPackArrayIterator(shard.second->slice())) {
          if (dbserver.copyString() == _server) {
            found = count;
            break;
          }
          count++;
        }
        if (found == -1) {
          continue;
        }

        auto replicationFactor = collection.hasAsString(StaticStrings::ReplicationFactor);
        bool isSatellite = replicationFactor.second && replicationFactor.first == StaticStrings::Satellite;
        bool isLeader = (found == 0);

        if (isSatellite) {
          if (isLeader) {

            std::string toServer = Job::findNonblockedCommonHealthyInSyncFollower(
              _snapshot, database.first, collptr.first, shard.first, _server);

            MoveShard(_snapshot, _agent, _jobId + "-" + std::to_string(sub++),
                    _jobId, database.first, collptr.first, shard.first, _server,
                    toServer, isLeader, false).withParent(_jobId)
              .create(trx);

          } else {
            // Intentionally do nothing. RemoveServer will remove the failed follower
            LOG_TOPIC("22ca1", DEBUG, Logger::SUPERVISION) <<
              "Do nothing for cleanout of follower of the SatelliteCollection " << collection.hasAsString("id").first;
            continue ;
          }
        } else {
          decltype(servers) serversCopy(servers);  // a copy

          // Only destinations, which are not already holding this shard
          for (VPackSlice dbserver : VPackArrayIterator(shard.second->slice())) {
            serversCopy.erase(std::remove(serversCopy.begin(), serversCopy.end(),
                                          dbserver.copyString()),
                              serversCopy.end());
          }

          // Among those a random destination:
          std::string toServer;
          if (serversCopy.empty()) {
            LOG_TOPIC("3c316", DEBUG, Logger::SUPERVISION)
                << "No servers remain as target for MoveShard";
            return false;
          }

          toServer = serversCopy.at(
              arangodb::RandomGenerator::interval(static_cast<int64_t>(0),
                                                  serversCopy.size() - 1));

          // Schedule move into trx:
          MoveShard(_snapshot, _agent, _jobId + "-" + std::to_string(sub++),
                    _jobId, database.first, collptr.first, shard.first, _server,
                    toServer, isLeader, false).withParent(_jobId)
              .create(trx);
        }
      }
    }
  }

  return true;
}

bool CleanOutServer::checkFeasibility() {
  std::vector<std::string> availServers = availableServers(_snapshot);

  // Minimum 1 DB server must remain:
  if (availServers.size() == 1) {
    LOG_TOPIC("ac9b8", ERR, Logger::SUPERVISION)
        << "DB server " << _server << " is the last standing db server.";
    return false;
  }

  // Remaining after clean out:
  uint64_t numRemaining = availServers.size() - 1;

  // Find conflicting collections:
  uint64_t maxReplFact = 1;
  std::vector<std::string> tooLargeCollections;
  std::vector<uint64_t> tooLargeFactors;
  Node::Children const& databases = _snapshot.hasAsChildren("/Plan/Collections").first;
  for (auto const& database : databases) {
    for (auto const& collptr : database.second->children()) {
      try {
        uint64_t replFact = (*collptr.second).hasAsUInt("replicationFactor").first;
        if (replFact > numRemaining) {
          tooLargeCollections.push_back(collptr.first);
          tooLargeFactors.push_back(replFact);
        }
        if (replFact > maxReplFact) {
          maxReplFact = replFact;
        }
      } catch (...) {
      }
    }
  }

  // Report if problems exist
  if (maxReplFact > numRemaining) {
    std::stringstream collections;
    std::stringstream factors;

    for (auto const& collection : tooLargeCollections) {
      collections << collection << " ";
    }
    for (auto const factor : tooLargeFactors) {
      factors << std::to_string(factor) << " ";
    }

    LOG_TOPIC("e598a", ERR, Logger::SUPERVISION)
        << "Cannot accomodate shards " << collections.str() << "with replication factors "
        << factors.str() << "after cleaning out server " << _server;
    return false;
  }

  return true;
}

arangodb::Result CleanOutServer::abort(std::string const& reason) {
  // We can assume that the job is either in ToDo or in Pending.
  Result result;

  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    result = Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                    "Failed aborting failedServer beyond pending stage");
    return result;
  }

  // Can now only be TODO or PENDING
  if (_status == TODO) {
    finish("", "", false, "job aborted:" + reason);
    return result;
  }

  // Abort all our subjobs:
  Node::Children const& todos = _snapshot.hasAsChildren(toDoPrefix).first;
  Node::Children const& pends = _snapshot.hasAsChildren(pendingPrefix).first;

  std::string childAbortReason = "parent job aborted - reason: " + reason;

  for (auto const& subJob : todos) {
    if (subJob.first.compare(0, _jobId.size() + 1, _jobId + "-") == 0) {
      JobContext(TODO, subJob.first, _snapshot, _agent).abort(childAbortReason);
    }
  }
  for (auto const& subJob : pends) {
    if (subJob.first.compare(0, _jobId.size() + 1, _jobId + "-") == 0) {
      JobContext(PENDING, subJob.first, _snapshot, _agent).abort(childAbortReason);
    }
  }

  auto payload = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder p(payload.get());
    payload->add(VPackValue(toBeCleanedPrefix));
    {
      VPackObjectBuilder pp(payload.get());
      payload->add("op", VPackValue("erase"));
      payload->add("val", VPackValue(_server));
    }
  }

  finish(_server, "", false, "job aborted: " + reason, payload);

  return result;
}
