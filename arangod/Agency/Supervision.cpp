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

std::string printTimestamp(Supervision::TimePoint const& t) {
  time_t tt = std::chrono::system_clock::to_time_t(t);
  struct tm tb;
  char buffer[21];
  TRI_gmtime(tt, &tb);
  size_t len = ::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);
  return std::string(buffer, len);
}

static std::string const syncPrefix = "/arango/Sync/ServerStates/";
static std::string const currentPrefix = "/arango/Current/DBServers/";
static std::string const supervisionPrefix = "/arango/Supervision/Health/";
std::vector<check_t> Supervision::check (std::string const& path) {

  std::vector<check_t> ret;
  Node::Children const& machinesPlanned = _snapshot(path).children();

  for (auto const& machine : machinesPlanned) {

    ServerID const& serverID = machine.first;
    auto it = _vitalSigns.find(serverID);
    std::string lastHeartbeatTime = _snapshot(syncPrefix + serverID + "/time").toJson();
    std::string lastHeartbeatStatus = _snapshot(syncPrefix + serverID + "/status").toJson();
    
    if (it != _vitalSigns.end()) {   // Existing server

      query_t report = std::make_shared<Builder>();
      report->openArray(); report->openArray(); report->openObject();
      report->add(supervisionPrefix + serverID,
                  VPackValue(VPackValueType::Object));

      report->add("LastHearbeatReceived",
                  VPackValue(printTimestamp(it->second->myTimestamp)));
      report->add("LastHearbeatSent", VPackValue(lastHeartbeatTime));
      report->add("LastHearbeatStatus", VPackValue(lastHeartbeatStatus));

      if (it->second->serverTimestamp == lastHeartbeatTime) {
        report->add("Status", VPackValue("DOWN"));
        std::chrono::seconds t{0};
        t = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now() - it->second->myTimestamp);
        if (t.count() > 60) {
          report->add("Alert", VPackValue(true));
        }

      } else {
        report->add("Status", VPackValue("UP"));
        it->second->update(lastHeartbeatStatus,lastHeartbeatTime);
      }

      report->close();        
      report->close();
      report->close();
      report->close();
      _agent->write(report);
      
    } else {                          // New server
      _vitalSigns[serverID] = 
        std::make_shared<VitalSign>(lastHeartbeatStatus,lastHeartbeatTime);
    }
  }
  
  auto itr = _vitalSigns.begin();
  while (itr != _vitalSigns.end()) {
    if (machinesPlanned.find(itr->first)==machinesPlanned.end()) {
      LOG(WARN) << itr->first << " shut down!"; 
      itr = _vitalSigns.erase(itr);
    } else {
      ++itr;
    }
  }
  
  return ret;

}

bool Supervision::moveShard (std::string const& from, std::string const& to) {
  
}

bool Supervision::doChecks (bool timedout) {

  if (_agent == nullptr) {
    return false;
  }

  _snapshot = _agent->readDB().get("/");
  
  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Sanity checks";
  std::vector<check_t> ret = check("/arango/Plan/DBServers");
  
  return true;
  
}

void Supervision::run() {

  CONDITION_LOCKER(guard, _cv);
  TRI_ASSERT(_agent!=nullptr);
  bool timedout = false;

  while (!this->isStopping()) {
    
    if (_agent->leading()) {
      timedout = _cv.wait(_frequency);//quarter second
    } else {
      _cv.wait();
    }
    
    doChecks(timedout);
    
  }
  
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
