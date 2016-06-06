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
  if (!this->isStopping()) {
    _snapshot = _agent->readDB().get(_agencyPrefix);
  }
  _cv.signal();
}

static std::string const syncPrefix = "/Sync/ServerStates/";
static std::string const healthPrefix = "/Supervision/Health/";
static std::string const planDBServersPrefix = "/Plan/DBServers";
static std::string const planCoordinatorsPrefix = "/Plan/Coordinators";

std::vector<check_t> Supervision::checkDBServers() {
  std::vector<check_t> ret;
  Node::Children const machinesPlanned =
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
      report->add("Status", VPackValue("GOOD"));
    } else {
      std::chrono::seconds t{0};
      t = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now()-stringToTimepoint(lastHeartbeatAcked));
      if (t.count() > _gracePeriod) {  // Failure
        if (lastStatus == "BAD") {
          report->add("Status", VPackValue("FAILED"));
          FailedServer fsj(_snapshot, _agent, std::to_string(_jobId++),
                           "supervision", _agencyPrefix, serverID);
        }
      } else {
        report->add("Status", VPackValue("BAD"));
      }
    }

    report->close();
    report->close();
    report->close();
    report->close();
    if (!this->isStopping()) {
      _agent->write(report);
    }
      
  }

  return ret;
}

std::vector<check_t> Supervision::checkCoordinators() {
  std::vector<check_t> ret;
  Node::Children const machinesPlanned =
      _snapshot(planCoordinatorsPrefix).children();

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
      report->add("Status", VPackValue("GOOD"));
    } else {
      std::chrono::seconds t{0};
      t = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now()-stringToTimepoint(lastHeartbeatAcked));
      if (t.count() > _gracePeriod) {  // Failure
        if (lastStatus == "BAD") {
          report->add("Status", VPackValue("FAILED"));
        }
      } else {
        report->add("Status", VPackValue("BAD"));
      }
    }

    report->close();
    report->close();
    report->close();
    report->close();
    if (!this->isStopping()) {
      _agent->write(report);
    }
      
  }

  return ret;
}

bool Supervision::updateSnapshot() {

  if (_agent == nullptr || this->isStopping()) {
    return false;
  }
  _snapshot = _agent->readDB().get(_agencyPrefix);
  return true;
}

bool Supervision::doChecks(bool timedout) {

  checkDBServers();
  checkCoordinators();
  return true;
  
}

void Supervision::run() {

  CONDITION_LOCKER(guard, _cv);
  TRI_ASSERT(_agent != nullptr);
  bool timedout = false;
  
  while (!this->isStopping()) {
    
    // Get agency prefix after cluster init
    if (_jobId == 0) {
      // We need the agency prefix to work, but it is only initialized by
      // some other server in the cluster. Since the supervision does not
      // make sense at all without other ArangoDB servers, we wait pretty
      // long here before giving up:
      if (!updateAgencyPrefix(1000, 1)) {
        LOG_TOPIC(ERR, Logger::AGENCY)
          << "Cannot get prefix from Agency. Stopping supervision for good.";
        break;
      }
    }
    
    // Get bunch of job IDs from agency for future jobs
    if (_jobId == 0 || _jobId == _jobIdMax) {
      getUniqueIds();  // cannot fail but only hang
    }
    
    // Do nothing unless leader 
    if (_agent->leading()) {
      timedout = _cv.wait(_frequency * 1000000);  // quarter second
    } else {
      _cv.wait();
    }
    
    // Do supervision
    updateSnapshot();
    doChecks(timedout);
    workJobs();
    
  }
  
}

void Supervision::workJobs() {

  Node::Children const todos = _snapshot(toDoPrefix).children();
  Node::Children const pends = _snapshot(pendingPrefix).children();

  for (auto const& todoEnt : todos) {
    Node const& job = *todoEnt.second;
    
    try {
      std::string jobType = job("type").getString(),
        jobId = job("jobId").getString(),
        creator = job("creator").getString();
      if (jobType == "failedServer") {
        FailedServer fs(_snapshot, _agent, jobId, creator, _agencyPrefix);
      } else if (jobType == "cleanOutServer") {
        CleanOutServer cos(_snapshot, _agent, jobId, creator, _agencyPrefix);
      }
    } catch (std::exception const&) {}
  }

  for (auto const& pendEnt : pends) {
    Node const& job = *pendEnt.second;
    
    try {
      std::string jobType = job("type").getString(),
        jobId = job("jobId").getString(),
        creator = job("creator").getString();
      if (jobType == "failedServer") {
        FailedServer fs(_snapshot, _agent, jobId, creator, _agencyPrefix);
      } else if (jobType == "cleanOutServer") {
        CleanOutServer cos(_snapshot, _agent, jobId, creator, _agencyPrefix);
      }
    } catch (std::exception const&) {}
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
  while (!this->isStopping()) {
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
void Supervision::getUniqueIds() {
  uint64_t latestId;
  // Run forever, supervision does not make sense before the agency data
  // is initialized by some other server...
  while (!this->isStopping()) {
    try {
      latestId = std::stoul(
          _agent->readDB().get(_agencyPrefix + "/Sync/LatestID").slice().toJson());
    } catch (...) {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      continue;
    }

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
      _jobId = latestId;
      _jobIdMax = latestId + 100000;
      return;
    }
  }
}

void Supervision::updateFromAgency() {
  auto const jobsPending =
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
