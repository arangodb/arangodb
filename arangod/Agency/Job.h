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

#ifndef ARANGOD_CONSENSUS_JOB_H
#define ARANGOD_CONSENSUS_JOB_H 1

#include "Agent.h"
#include "Node.h"
#include "Supervision.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <string>

namespace arangodb {
namespace consensus {

enum JOB_STATUS {TODO, PENDING, FINISHED, FAILED, NOTFOUND};

static std::string const pendingPrefix  = "/Target/Pending/";
static std::string const failedPrefix   = "/Target/Failed/";
static std::string const finishedPrefix = "/Target/Finished/";
static std::string const toDoPrefix     = "/Target/ToDo/";
static std::string const cleanedPrefix  = "Target/CleanedServers";
static std::string const planColPrefix  = "/Plan/Collections/";
static std::string const curColPrefix   = "/Current/Collections/";
static std::string const blockedServersPrefix = "/Supervision/DBServers/";
static std::string const blockedShardsPrefix = "/Supervision/Shards/";
static std::string const serverStatePrefix = "/Sync/ServerStates/";
static std::string const planVersion    = "/Plan/Version";

struct JobResult {
  JobResult() {}
};

struct JobCallback {
  JobCallback() {}
  virtual ~JobCallback(){};
  virtual bool operator()(JobResult*) = 0;
};

struct Job {
  
  Job(Node const& snapshot, Agent* agent, std::string const& jobId,
      std::string const& creator, std::string const& agencyPrefix) :
    _snapshot(snapshot), _agent(agent), _jobId(jobId), _creator(creator),
    _agencyPrefix(agencyPrefix) {}
  
  virtual ~Job() {}
  
  virtual bool exists() const {
  
    Node const& target = _snapshot("/Target");
    unsigned res = 4;
  
    if        (target.exists(std::string("/ToDo/")     + _jobId).size() == 2) {
      res = 0;
    } else if (target.exists(std::string("/Pending/")  + _jobId).size() == 2) {
      res = 1;
    } else if (target.exists(std::string("/Finished/") + _jobId).size() == 2) {
      res = 2;
    }  else if (target.exists(std::string("/Failed/")  + _jobId).size() == 2) {
      res = 3;
    } 
  
    return (res < 4);
  
  }
  
  virtual bool finish(std::string const& type, bool success = true) const {

    Builder pending, finished;
  
    // Get todo entry
    pending.openArray();
    _snapshot(pendingPrefix + _jobId).toBuilder(pending);
    pending.close();
  
    // Prepare peding entry, block toserver
    finished.openArray();
  
    // --- Add finished
    finished.openObject();
    finished.add(_agencyPrefix + (success ? finishedPrefix : failedPrefix)
                 + _jobId, VPackValue(VPackValueType::Object));
    finished.add("timeFinished",
                 VPackValue(timepointToString(std::chrono::system_clock::now())));
    for (auto const& obj : VPackObjectIterator(pending.slice()[0])) {
      finished.add(obj.key.copyString(), obj.value);
    }
    finished.close();
  
    // --- Delete pending
    finished.add(_agencyPrefix + pendingPrefix + _jobId,
                 VPackValue(VPackValueType::Object));
    finished.add("op", VPackValue("delete"));
    finished.close();

    // --- Remove block
    finished.add(_agencyPrefix + "/Supervision/" + type,
                 VPackValue(VPackValueType::Object));
    finished.add("op", VPackValue("delete"));
    finished.close();

    // --- Need precond? 
    finished.close(); finished.close();
  
    write_ret_t res = transact(_agent, finished);
  
    if (res.accepted && res.indices.size()==1 && res.indices[0]) {
      return true;
    }

    return false;
  
  }
  
  virtual unsigned status () const = 0;
  
  virtual bool create () const = 0;
  
  virtual bool start() const = 0;
  
  Node const _snapshot;
  Agent* _agent;
  std::string _jobId;
  std::string _creator;
  std::string _agencyPrefix;
  
};

}}

#endif
