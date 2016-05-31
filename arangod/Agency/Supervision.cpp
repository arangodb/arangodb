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

#include "Supervision.h"

#include "Agent.h"
#include "CleanOutServer.h"
#include "FailedLeader.h"
#include "FailedServer.h"
#include "Job.h"
#include "Store.h"

#include "Basics/ConditionLocker.h"
#include "VocBase/server.h"

#include <thread>
using namespace arangodb;

using namespace arangodb::consensus;

std::string Supervision::_agencyPrefix = "/arango";

Supervision::Supervision()
    : arangodb::Thread("Supervision"),
      _agent(nullptr),
      _snapshot("Supervision"),
      _frequency(5),
      _gracePeriod(15),
      _jobId(0),
      _jobIdMax(0) {}

Supervision::~Supervision() { shutdown(); };

void Supervision::wakeUp() {
  TRI_ASSERT(_agent != nullptr);
  _snapshot = _agent->readDB().get(_agencyPrefix);
  _cv.signal();
}

static std::string const syncPrefix = "/Sync/ServerStates/";
static std::string const healthPrefix = "/Supervision/Health/";
static std::string const planDBServersPrefix = "/Plan/DBServers";

std::vector<check_t> Supervision::checkDBServers() {
  std::vector<check_t> ret;
  Node::Children const& machinesPlanned =
      _snapshot(planDBServersPrefix).children();

  for (auto const& machine : machinesPlanned) {

    bool good = false;
    std::string lastHeartbeatTime, lastHeartbeatStatus, lastHeartbeatAcked,
      lastStatus, heartbeatTime, heartbeatStatus, serverID;

    serverID        = machine.first;
    heartbeatTime   = _snapshot(syncPrefix + serverID + "/time").toJson();
    heartbeatStatus = _snapshot(syncPrefix + serverID + "/status").toJson();
    
    try {           // Existing
      lastHeartbeatTime =
        _snapshot(healthPrefix + serverID + "/LastHeartbeatSent").toJson();
      lastHeartbeatStatus =
        _snapshot(healthPrefix + serverID + "/LastHeartbeatStatus").toJson();
      lastHeartbeatAcked =
        _snapshot(healthPrefix + serverID + "/LastHeartbeatAcked").toJson();
      lastStatus = _snapshot(healthPrefix + serverID + "/Status").toJson();
      if (lastHeartbeatTime != heartbeatTime) { // Update
        good = true;
      }
    } catch (...) { // New server
      good = true;
    }
    
    query_t report = std::make_shared<Builder>();
    report->openArray();
    report->openArray();
    report->openObject();
    report->add(_agencyPrefix + healthPrefix + serverID,
                VPackValue(VPackValueType::Object));
    report->add("LastHeartbeatSent", VPackValue(heartbeatTime));
    report->add("LastHeartbeatStatus", VPackValue(heartbeatStatus));
    
    if (good) {
      report->add("LastHeartbeatAcked",
                  VPackValue(
                    timepointToString(std::chrono::system_clock::now())));
      report->add("Status", VPackValue("UP"));
    } else {
      std::chrono::seconds t{0};
      t = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now()-stringToTimepoint(lastHeartbeatAcked));
      if (t.count() > _gracePeriod) {  // Failure
        if (lastStatus == "DOWN") {
          report->add("Status", VPackValue("FAILED"));
          FailedServer fsj(_snapshot, _agent, std::to_string(_jobId++),
                           "supervision", _agencyPrefix, serverID);
        }
      } else {
        report->add("Status", VPackValue("DOWN"));
      }
    }

    report->close();
    report->close();
    report->close();
    report->close();
    _agent->write(report);
      
  }

  return ret;
}

bool Supervision::doChecks(bool timedout) {
  if (_agent == nullptr) {
    return false;
  }

  _snapshot = _agent->readDB().get(_agencyPrefix);

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Sanity checks";
  /*std::vector<check_t> ret = */checkDBServers();

  return true;
}

void Supervision::run() {

  CONDITION_LOCKER(guard, _cv);
  TRI_ASSERT(_agent != nullptr);
  bool timedout = false;

  while (!this->isStopping()) {

    // Get agency prefix after cluster init
    if (_jobId == 0) {
      if (!updateAgencyPrefix(10)) {
        LOG_TOPIC(ERR, Logger::AGENCY)
          << "Cannot get prefix from Agency. Stopping supervision for good.";
        break;
      }
    }

    // Get bunch of job IDs from agency for future jobs
    if (_jobId == 0 || _jobId == _jobIdMax) {
      if (!getUniqueIds()) {
        LOG_TOPIC(ERR, Logger::AGENCY)
          << "Cannot get unique IDs from Agency. Stopping supervision for good.";
        break;
      }

    }

    // Do nothing unless leader 
    if (_agent->leading()) {
      timedout = _cv.wait(_frequency * 1000000);  // quarter second
    } else {
      _cv.wait();
    }

    // Do supervision
    doChecks(timedout);
    workJobs();
    

  }
  
}

void Supervision::workJobs() {

  for (auto const& todoEnt : _snapshot(toDoPrefix).children()) {
    Node const& todo = *todoEnt.second;
    if (todo("type").toJson() == "failedServer") {
      FailedServer fs (
        _snapshot, _agent, todo("jobId").toJson(), todo("creator").toJson(),
        _agencyPrefix, todo("server").toJson());
    }
  }

  for (auto const& todoEnt : _snapshot(pendingPrefix).children()) {
    Node const& todo = *todoEnt.second;
    if (todo("type").toJson() == "failedServer") {
      FailedServer fs (
        _snapshot, _agent, todo("jobId").toJson(), todo("creator").toJson(),
        _agencyPrefix, todo("server").toJson());
    }
  }

}

// Start thread
bool Supervision::start() {
  Thread::start();
  return true;
}

// Start thread with agent
bool Supervision::start(Agent* agent) {
  _agent = agent;
  _frequency = static_cast<long>(_agent->config().supervisionFrequency);

  return start();
}

// Get agency prefix fron agency
bool Supervision::updateAgencyPrefix (size_t nTries, int intervalSec) {

  // Try nTries to get agency's prefix in intervals 
  for (size_t i = 0; i < nTries; i++) {
    _snapshot = _agent->readDB().get("/");
    if (_snapshot.children().size() > 0) {
      _agencyPrefix = std::string("/") + _snapshot.children().begin()->first;
      LOG_TOPIC(DEBUG, Logger::AGENCY) << "Agency prefix is " << _agencyPrefix;
      return true;
    }
    std::this_thread::sleep_for (std::chrono::seconds(intervalSec));
  }

  // Stand-alone agency
  return false;
  
}

static std::string const syncLatest = "/Sync/LatestID";
// Get bunch of cluster's unique ids from agency 
bool Supervision::getUniqueIds() {
  uint64_t latestId;

  try {
    latestId = std::stoul(
        _agent->readDB().get(_agencyPrefix + "/Sync/LatestID").slice().toJson());
  } catch (std::exception const& e) {
    LOG(WARN) << e.what();
    return false;
  }

  bool success = false;
  while (!success) {
    Builder uniq;
    uniq.openArray();
    uniq.openObject();
    uniq.add(_agencyPrefix + syncLatest, VPackValue(latestId + 100000)); // new 
    uniq.close();
    uniq.openObject();
    uniq.add(_agencyPrefix + syncLatest, VPackValue(latestId));      // precond
    uniq.close();
    uniq.close();

    auto result = transact(_agent, uniq);
    if (result.indices[0]) {
      _agent->waitFor(result.indices[0]);
      success = true;
      _jobId = latestId;
      _jobIdMax = latestId + 100000;
    }

    latestId = std::stoul(
        _agent->readDB().get(_agencyPrefix + "/Sync/LatestID").slice().toJson());
  }

  return success;
}

void Supervision::updateFromAgency() {
  auto const& jobsPending =
      _snapshot("/Supervision/Jobs/Pending").children();

  for (auto const& jobent : jobsPending) {
    auto const& job = *(jobent.second);

    LOG_TOPIC(WARN, Logger::AGENCY)
      << job.name() << " " << job("failed").toJson() << job("");
  }
}

void Supervision::beginShutdown() {
  // Personal hygiene
  Thread::beginShutdown();
}

Store const& Supervision::store() const {
  return _agent->readDB();
}
