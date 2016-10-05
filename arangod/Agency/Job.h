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

enum JOB_STATUS { TODO, PENDING, FINISHED, FAILED, NOTFOUND };
const std::vector<std::string> pos({"/Target/ToDo/", "/Target/Pending/",
                                    "/Target/Finished/", "/Target/Failed/"});

static std::string const pendingPrefix = "/Target/Pending/";
static std::string const failedPrefix = "/Target/Failed/";
static std::string const finishedPrefix = "/Target/Finished/";
static std::string const toDoPrefix = "/Target/ToDo/";
static std::string const cleanedPrefix = "/Target/CleanedServers";
static std::string const failedServersPrefix = "/Target/FailedServers";
static std::string const planColPrefix = "/Plan/Collections/";
static std::string const curColPrefix = "/Current/Collections/";
static std::string const blockedServersPrefix = "/Supervision/DBServers/";
static std::string const blockedShardsPrefix = "/Supervision/Shards/";
static std::string const serverStatePrefix = "/Sync/ServerStates/";
static std::string const planVersion = "/Plan/Version";

inline arangodb::consensus::write_ret_t transact(Agent* _agent,
                                                 Builder const& transaction,
                                                 bool waitForCommit = true) {
  query_t envelope = std::make_shared<Builder>();

  try {
    envelope->openArray();
    envelope->add(transaction.slice());
    envelope->close();
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY)
        << "Supervision failed to build transaction.";
    LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
  }

  LOG_TOPIC(DEBUG, Logger::AGENCY) << envelope->toJson();
  auto ret = _agent->write(envelope);
  if (waitForCommit && !ret.indices.empty()) {
    auto maximum = *std::max_element(ret.indices.begin(), ret.indices.end());
    if (maximum > 0) {  // some baby has worked
      _agent->waitFor(maximum);
    }
  }
  return ret;
}

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
      std::string const& creator, std::string const& agencyPrefix);

  virtual ~Job()

  virtual JOB_STATUS exists() const;

  virtual bool finish(std::string const& type, bool success = true,
                      std::string const& reason = std::string()) const;
  virtual JOB_STATUS status() = 0;

  virtual bool create() = 0;

  virtual bool start() = 0;

  Node const _snapshot;
  Agent* _agent;
  std::string _jobId;
  std::string _creator;
  std::string _agencyPrefix;

  std::shared_ptr<Builder> _jb;
  
};

}
}

#endif
