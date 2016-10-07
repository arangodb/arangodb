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

#include "Job.h"

using namespace arangodb::consensus;

Job::Job(Node const& snapshot, Agent* agent, std::string const& jobId,
         std::string const& creator, std::string const& agencyPrefix) :
  _snapshot(snapshot),
  _agent(agent),
  _jobId(jobId),
  _creator(creator),
  _agencyPrefix(agencyPrefix),
  _jb(nullptr) {}

Job::~Job() {}

JOB_STATUS Job::exists() const {

  Node const& target = _snapshot("/Target");
  
  if (target.exists(std::string("/ToDo/") + _jobId).size() == 2) {
    return TODO;
  } else if (target.exists(std::string("/Pending/") + _jobId).size() == 2) {
    return PENDING;
  } else if (target.exists(std::string("/Finished/") + _jobId).size() == 2) {
    return FINISHED;
  } else if (target.exists(std::string("/Failed/") + _jobId).size() == 2) {
    return FAILED;
  }
  
  return NOTFOUND;
  
}


bool Job::finish(std::string const& type, bool success,
                 std::string const& reason) const {
  
  Builder pending, finished;
  
  // Get todo entry
  pending.openArray();
  if (_snapshot.exists(pendingPrefix + _jobId).size() == 3) {
    _snapshot(pendingPrefix + _jobId).toBuilder(pending);
  } else if (_snapshot.exists(toDoPrefix + _jobId).size() == 3) {
    _snapshot(toDoPrefix + _jobId).toBuilder(pending);
  } else {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Nothing in pending to finish up for job " << _jobId;
    return false;
  }
  pending.close();

  std::string jobType;
  try {
    jobType = pending.slice()[0].get("type").copyString();
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "Failed to obtain type of job " << _jobId;
  }
  
  // Prepare peding entry, block toserver
  finished.openArray();
  
  // --- Add finished
  finished.openObject();
  finished.add(
    _agencyPrefix + (success ? finishedPrefix : failedPrefix) + _jobId,
    VPackValue(VPackValueType::Object));
  finished.add(
    "timeFinished",
    VPackValue(timepointToString(std::chrono::system_clock::now())));
  for (auto const& obj : VPackObjectIterator(pending.slice()[0])) {
    finished.add(obj.key.copyString(), obj.value);
  }
  if (!reason.empty()) {
    finished.add("reason", VPackValue(reason));
  }
  finished.close();

  // --- Delete pending
  finished.add(_agencyPrefix + pendingPrefix + _jobId,
               VPackValue(VPackValueType::Object));
  finished.add("op", VPackValue("delete"));
  finished.close();

  // --- Delete todo
  finished.add(_agencyPrefix + toDoPrefix + _jobId,
               VPackValue(VPackValueType::Object));
  finished.add("op", VPackValue("delete"));
  finished.close();

  // --- Remove block if specified
  if (jobType == "moveShard") {
    for (auto const& shard :
           VPackArrayIterator(pending.slice()[0].get("shards"))) {
      finished.add(_agencyPrefix + "/Supervision/Shards/" + shard.copyString(),
                   VPackValue(VPackValueType::Object));
      finished.add("op", VPackValue("delete"));
      finished.close();
    }
  } else if (type != "") {
    finished.add(_agencyPrefix + "/Supervision/" + type,
                 VPackValue(VPackValueType::Object));
    finished.add("op", VPackValue("delete"));
    finished.close();
  }

  // --- Need precond?
  finished.close();
  finished.close();

  write_ret_t res = transact(_agent, finished);
  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC(INFO, Logger::AGENCY)
      << "Successfully finished job " << type << "(" << _jobId << ")";
    return true;
  }

  return false;
}


std::vector<std::string> Job::availableServers() const {

  std::vector<std::string> ret;

  // Get servers from plan
  Node::Children const& dbservers = _snapshot(plannedServers).children();
  for (auto const& srv : dbservers) {
    ret.push_back(srv.first);
  }
  
  // Remove cleaned servers from ist
  try {
    for (auto const& srv :
           VPackArrayIterator(_snapshot(cleanedPrefix).slice())) {
      ret.erase(
        std::remove(ret.begin(), ret.end(), srv.copyString()),
        ret.end());
    }
  } catch (...) {}

  
  // Remove failed servers from list
  try {
    for (auto const& srv :
           VPackArrayIterator(_snapshot(failedServersPrefix).slice())) {
      ret.erase(
        std::remove(ret.begin(), ret.end(), srv.copyString()),
        ret.end());
    }
  } catch (...) {}
  
  return ret;
  
}
