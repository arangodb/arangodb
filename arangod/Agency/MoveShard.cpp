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

#include "MoveShard.h"

#include "Agent.h"
#include "Job.h"

using namespace arangodb::consensus;

MoveShard::MoveShard(Node const& snapshot, Agent* agent,
                     std::string const& jobId, std::string const& creator,
                     std::string const& prefix, std::string const& database,
                     std::string const& collection, std::string const& shard,
                     std::string const& from, std::string const& to)
    : Job(snapshot, agent, jobId, creator, prefix),
      _database(database),
      _collection(collection),
      _shard(shard),
      _from(from),
      _to(to) {
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
    LOG_TOPIC(WARN, Logger::AGENCY) << e.what() << __FILE__ << __LINE__;
    finish("Shards/" + _shard, false, e.what());
  }
}

MoveShard::~MoveShard() {}

bool MoveShard::create() {
  LOG_TOPIC(INFO, Logger::AGENCY)
      << "Todo: Move shard " + _shard + " from " + _from + " to " << _to;

  std::string path, now(timepointToString(std::chrono::system_clock::now()));

  // DBservers
  std::string curPath =
      curColPrefix + _database + "/" + _collection + "/" + _shard + "/servers";

  Slice current = _snapshot(curPath).slice();
  TRI_ASSERT(current.isArray());
  TRI_ASSERT(current[0].isString());

  _jb = std::make_shared<Builder>();
  _jb->openArray();
  _jb->openObject();

  if (_from == _to) {
    path = _agencyPrefix + failedPrefix + _jobId;
    _jb->add("timeFinished", VPackValue(now));
    _jb->add(
        "result",
        VPackValue("Source and destination of moveShard must be different"));
  } else {
    path = _agencyPrefix + toDoPrefix + _jobId;
  }

  _jb->add(path, VPackValue(VPackValueType::Object));
  _jb->add("creator", VPackValue(_creator));
  _jb->add("type", VPackValue("moveShard"));
  _jb->add("database", VPackValue(_database));
  _jb->add("collection", VPackValue(_collection));
  _jb->add("shard", VPackValue(_shard));
  _jb->add("fromServer", VPackValue(_from));
  _jb->add("toServer", VPackValue(_to));
  _jb->add("isLeader", VPackValue(current[0].copyString() == _from));
  _jb->add("jobId", VPackValue(_jobId));
  _jb->add("timeCreated", VPackValue(now));

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

bool MoveShard::start() {
  // DBservers
  std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  std::string curPath =
      curColPrefix + _database + "/" + _collection + "/" + _shard + "/servers";

  Slice current = _snapshot(curPath).slice();
  Slice planned = _snapshot(planPath).slice();

  TRI_ASSERT(current.isArray());
  TRI_ASSERT(planned.isArray());

  for (auto const& srv : VPackArrayIterator(current)) {
    TRI_ASSERT(srv.isString());
    if (srv.copyString() == _to) {
      finish("Shards/" + _shard, false,
             "ToServer must not be already holding the shard.");
      return false;
    }
  }
  for (auto const& srv : VPackArrayIterator(planned)) {
    TRI_ASSERT(srv.isString());
    if (srv.copyString() == _to) {
      finish("Shards/" + _shard, false,
             "ToServer must not be planned for shard already.");
      return false;
    }
  }

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
    todo.add(_jb->slice()[0].valueAt(0));
  }
  todo.close();

  // Enter pending, remove todo, block toserver
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

  // --- Block shard
  pending.add(_agencyPrefix + blockedShardsPrefix + _shard,
              VPackValue(VPackValueType::Object));
  pending.add("jobId", VPackValue(_jobId));
  pending.close();

  // --- Plan changes
  pending.add(_agencyPrefix + planPath, VPackValue(VPackValueType::Array));
  if (planned[0].copyString() == _from) {  // Leader
    pending.add(planned[0]);
    pending.add(VPackValue(_to));
    for (size_t i = 1; i < planned.length(); ++i) {
      pending.add(planned[i]);
    }
  } else {  // Follower
    for (auto const& srv : VPackArrayIterator(planned)) {
      pending.add(srv);
    }
    pending.add(VPackValue(_to));
  }
  pending.close();

  // --- Increment Plan/Version
  pending.add(_agencyPrefix + planVersion, VPackValue(VPackValueType::Object));
  pending.add("op", VPackValue("increment"));
  pending.close();

  pending.close();

  // Preconditions
  // --- Check that Current servers are as we expect
  pending.openObject();
  pending.add(_agencyPrefix + curPath, VPackValue(VPackValueType::Object));
  pending.add("old", current);
  pending.close();

  // --- Check if shard is not blocked
  pending.add(_agencyPrefix + blockedShardsPrefix + _shard,
              VPackValue(VPackValueType::Object));
  pending.add("oldEmpty", VPackValue(true));
  pending.close();

  pending.close();
  pending.close();

  // Transact to agency
  write_ret_t res = transact(_agent, pending);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC(INFO, Logger::AGENCY)
        << "Pending: Move shard " + _shard + " from " + _from + " to " + _to;
    return true;
  }

  LOG_TOPIC(INFO, Logger::AGENCY) << "Start precondition failed for " + _jobId;
  return false;
}

JOB_STATUS MoveShard::status() {
  auto status = exists();

  if (status != NOTFOUND) {  // Get job details from agency

    try {
      _database = _snapshot(pos[status] + _jobId + "/database").getString();
      _collection = _snapshot(pos[status] + _jobId + "/collection").getString();
      _from = _snapshot(pos[status] + _jobId + "/fromServer").getString();
      _to = _snapshot(pos[status] + _jobId + "/toServer").getString();
      _shard = _snapshot(pos[status] + _jobId + "/shard").getString();
    } catch (std::exception const& e) {
      std::stringstream err;
      err << "Failed to find job " << _jobId << " in agency: " << e.what();
      LOG_TOPIC(ERR, Logger::AGENCY) << err.str();
      finish("Shards/" + _shard, false, err.str());
      return FAILED;
    }
  }

  if (status == PENDING) {
    std::string planPath =
        planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
    std::string curPath = curColPrefix + _database + "/" + _collection + "/" +
                          _shard + "/servers";

    Slice current = _snapshot(curPath).slice();
    Slice plan = _snapshot(planPath).slice();

    std::vector<std::string> planv, currv;
    for (auto const& srv : VPackArrayIterator(plan)) {
      planv.push_back(srv.copyString());
    }
    std::sort(planv.begin(), planv.end());
    for (auto const& srv : VPackArrayIterator(current)) {
      currv.push_back(srv.copyString());
    }
    std::sort(currv.begin(), currv.end());

    if (currv == planv) {
      if (current[0].copyString() ==
          std::string("_") + _from) {  // Retired leader

        Builder remove;  // remove
        remove.openArray();
        remove.openObject();
        // --- Plan changes
        remove.add(_agencyPrefix + planPath, VPackValue(VPackValueType::Array));
        for (size_t i = 1; i < plan.length(); ++i) {
          remove.add(plan[i]);
        }
        remove.close();
        // --- Plan version
        remove.add(_agencyPrefix + planVersion,
                   VPackValue(VPackValueType::Object));
        remove.add("op", VPackValue("increment"));
        remove.close();
        remove.close();
        remove.close();
        transact(_agent, remove);

        return PENDING;

      } else {
        bool foundFrom = false, foundTo = false;
        for (auto const& srv : VPackArrayIterator(current)) {
          std::string srv_str = srv.copyString();
          if (srv_str == _from) {
            foundFrom = true;
          }
          if (srv_str == _to) {
            foundTo = true;
          }
        }

        if (foundFrom && foundTo) {
          if (plan[0].copyString() == _from) {  // Leader

            Builder underscore;  // serverId -> _serverId
            underscore.openArray();
            underscore.openObject();
            // --- Plan changes
            underscore.add(_agencyPrefix + planPath,
                           VPackValue(VPackValueType::Array));
            underscore.add(VPackValue(std::string("_") + plan[0].copyString()));
            for (size_t i = 1; i < plan.length(); ++i) {
              underscore.add(plan[i]);
            }
            underscore.close();

            // --- Plan version
            underscore.add(_agencyPrefix + planVersion,
                           VPackValue(VPackValueType::Object));
            underscore.add("op", VPackValue("increment"));
            underscore.close();
            underscore.close();
            underscore.close();
            transact(_agent, underscore);

          } else {
            Builder remove;
            remove.openArray();
            remove.openObject();
            // --- Plan changes
            remove.add(_agencyPrefix + planPath,
                       VPackValue(VPackValueType::Array));
            for (auto const& srv : VPackArrayIterator(plan)) {
              if (srv.copyString() != _from) {
                remove.add(srv);
              }
            }
            remove.close();
            // --- Plan version
            remove.add(_agencyPrefix + planVersion,
                       VPackValue(VPackValueType::Object));
            remove.add("op", VPackValue("increment"));
            remove.close();
            remove.close();
            remove.close();
            transact(_agent, remove);
          }

          return PENDING;

        } else if (foundTo && !foundFrom) {
          if (finish("Shards/" + _shard)) {
            return FINISHED;
          }
        }
      }
    }
  }

  return status;
}
