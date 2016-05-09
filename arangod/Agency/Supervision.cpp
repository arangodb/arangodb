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
#include "Store.h"

#include "Basics/ConditionLocker.h"

using namespace arangodb::consensus;



Supervision::Supervision() : arangodb::Thread("Supervision"), _agent(nullptr),
                             _snapshot("Supervision"), _frequency(5000000) {
  
}

Supervision::~Supervision() {
  shutdown();
};

void Supervision::wakeUp () {

  TRI_ASSERT(_agent!=nullptr);
  _snapshot = _agent->readDB().get("/");
  _cv.signal();
  
}

/*
      "Sync": {
        "UserVersion": 2,
        "ServerStates": {
          "DBServer2": {
            "time": "2016-05-04T09:17:31Z",
            "status": "SERVINGASYNC"
          },
          "DBServer1": {
            "time": "2016-05-04T09:17:30Z",
            "status": "SERVINGASYNC"
          },
          "Coordinator1": {
            "time": "2016-05-04T09:17:31Z",
            "status": "SERVING"
          }
        },
        "Problems": null,
        "LatestID": 2000001,
        "HeartbeatIntervalMs": 1000,
        "Commands": null
      },
*/
static std::string const syncPrefix = "/arango/Sync/ServerStates/";
static std::string const supervisionPrefix = "/arango/Supervision";
std::vector<check_t> Supervision::check (std::string const& path) {

  std::vector<check_t> ret;
  Node::Children const& machines = _snapshot(path).children();

  for (auto const& machine : machines) {

    ServerID const& serverID = machine.first;
    auto it = _vital_signs.find(serverID);
    std::string lastHeartbeatTime = _snapshot(syncPrefix + serverID + "/time").toJson();
    std::string lastHeartbeatStatus = _snapshot(syncPrefix + serverID + "/status").toJson();
    
    if (it != _vital_signs.end()) {   // Existing server
      if (it->second->serverTimestamp == lastHeartbeatTime) {
        LOG(WARN) << "NO GOOD:" << serverID << it->second->serverTimestamp << ":" << it->second->serverStatus;
        continue;
      } else {
        query_t report = std::make_shared<Builder>();
        report->openArray(); report->openArray();
        report->add(std::string("/arango/Supervision/Health/" + serverID),
                    VPackValue(VPackValueType::Object));
        report->add("Status", VPackValue("GOOD"));
        report->add("LastHearbeat", VPackValue("GOOD"));
        report->close();
        report->close(); report->close();
        LOG(DEBUG) << "GOOD:" << serverID<< it->second->serverTimestamp << ":" << it->second->serverStatus;
        LOG(INFO) << report->toJson();
        it->second->update(lastHeartbeatStatus,lastHeartbeatTime);
      }
    } else {                          // New server
      _vital_signs[serverID] = 
        std::make_shared<VitalSign>(lastHeartbeatStatus,lastHeartbeatTime);
    }
  }

  return ret;

}

bool Supervision::doChecks (bool timedout) {

  if (_agent == nullptr) {
    return false;
  }

  _snapshot = _agent->readDB().get("/");
  
  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Sanity checks";
  std::vector<check_t> ret = check("/arango/Current/DBServers");
  
  return true;
  
}

void Supervision::run() {

  CONDITION_LOCKER(guard, _cv);
  TRI_ASSERT(_agent!=nullptr);
  bool timedout = false;

  /*
  while (!this->isStopping()) {
    
    if (_agent->leading()) {
      timedout = _cv.wait(_frequency);//quarter second
    } else {
      _cv.wait();
    }
    
    doChecks(timedout);
    
    }*/
  
}

// Start thread
bool Supervision::start () {
  Thread::start();
  return true;
}

// Start thread with agent
bool Supervision::start (Agent* agent) {
  _agent = agent;
  _frequency = static_cast<long>(1.0e6*_agent->config().supervisionFrequency);
  return start();
}

void Supervision::beginShutdown() {
  // Personal hygiene
  Thread::beginShutdown();
}

Store const& Supervision::store() const {
  return _agent->readDB();
}
