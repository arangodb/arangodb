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
#include "VocBase/server.h"


using namespace arangodb;

namespace arangodb {
namespace consensus {

inline void makeReport (query_t& envelope, Builder const& report) {
  envelope->openArray();
  envelope->openArray();
  envelope->add(report.slice());
  envelope->close();
  envelope->close();
}

template<> struct Job<arangodb::consensus::FAILED_DBSERVER> {

  Job (Node const& snapshot, Agent* agent, uint64_t jobId,
       std::string const& failed) {
    // 1. find all shards in plan, where failed was leader.
    // 2. swap positions in plan between failed and a random in sync follower
    
    Node::Children const& databases =
      snapshot("/arango/Plan/Collections").children();
    
    for (auto const& database : databases) {
      for (auto const& collptr : database.second->children()) {
        Node const& collection = *(collptr.second);
        Node const& replicationFactor = collection("replicationFactor");
        if (replicationFactor.slice().getUInt() > 1) {
          for (auto const& shard : collection("shards").children()) {

            std::vector<std::string> bad, reordered;
            VPackArrayIterator dbsit (shard.second->slice());

            if ((*dbsit.begin()).copyString() != failed) { // Cannot do much
              continue;
            }
            
            for (auto const& dbserver : dbsit) {
              std::string serverID = dbserver.copyString();
              if (dbserver.copyString() == failed) {
                bad.push_back(serverID);
              } else {
                reordered.push_back(serverID);
              }
            }

            // Put into supervision
            std::string path ("/arango/Supervision/Jobs/Pending/");
            path += arangodb::basics::StringUtils::itoa(jobId);
            LOG(WARN) << path;
            query_t envelope = std::make_shared<Builder>();
            Builder report;
            report.openObject();
            report.add(path, VPackValue(VPackValueType::Object));
            report.add("shard", VPackValue(shard.first));
            report.add("dbservers", VPackValue(failed));
            report.add("old", shard.second->slice());
            report.add("new", VPackValue(VPackValueType::Array));
            for (auto const& server : reordered) {
              report.add(VPackValue(server));
            }
            for (auto const& server : bad) {
              report.add(VPackValue(server));
            }
            report.close();
            report.close();
            report.close();
            LOG(WARN) << report.toJson();
            makeReport(envelope, report);

            envelope->clear();
            report.clear();

            // Put into plan
            path = std::string("/arango/Plan/Collections/")
              + database.first + "/" + collptr.first + "/shards/" + shard.first;

            LOG(WARN) << path;
              
            /*report.openObject();
              path = std::string("/arango/Plan/Collections")
              + database.first + "/" + collptr.first + "/" + shard.first;
              report.close();
              LOG(WARN) << report.toJson();
              makeReport(envelope, report);
              LOG(WARN) << envelope->toJson();*/
            //agent->write(envelope);
            // }
            //}
          }
        } 
      }
    }
    
  }
  
};

}}


using namespace arangodb::consensus;

Supervision::Supervision() : arangodb::Thread("Supervision"), _agent(nullptr),
                             _snapshot("Supervision"), _frequency(5),
                             _gracePeriod(10) {
  
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
static std::string const supervisionPrefix = "/arango/Supervision/Health/";
static std::string const planDBServersPrefix = "/arango/Plan/DBServers";
std::vector<check_t> Supervision::checkDBServers () {

  std::vector<check_t> ret;
  Node::Children const& machinesPlanned =
    _snapshot(planDBServersPrefix).children();

  for (auto const& machine : machinesPlanned) {

    ServerID const& serverID = machine.first;
    auto it = _vitalSigns.find(serverID);
    std::string lastHeartbeatTime =
      _snapshot(syncPrefix + serverID + "/time").toJson();
    std::string lastHeartbeatStatus =
      _snapshot(syncPrefix + serverID + "/status").toJson();
    
    if (it != _vitalSigns.end()) {   // Existing server

      query_t report = std::make_shared<Builder>();
      report->openArray(); report->openArray(); report->openObject();
      report->add(supervisionPrefix + serverID,
                  VPackValue(VPackValueType::Object));
      report->add("LastHearbeatReceived",
                  VPackValue(printTimestamp(it->second->myTimestamp)));
      report->add("LastHearbeatSent", VPackValue(it->second->serverTimestamp));
      report->add("LastHearbeatStatus", VPackValue(lastHeartbeatStatus));

      if (it->second->serverTimestamp == lastHeartbeatTime) {
        report->add("Status", VPackValue("DOWN"));
        std::chrono::seconds t{0};
        t = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now() - it->second->myTimestamp);
        if (t.count() > _gracePeriod) {               // Failure
          if (it->second->maintenance() == 0) {
            it->second->maintenance(TRI_NewTickServer());
            Job<FAILED_DBSERVER> jfl(_snapshot, _agent, it->second->maintenance(),
                                   serverID);
          }
        }

      } else {
        report->add("Status", VPackValue("UP"));
        it->second->update(lastHeartbeatStatus,lastHeartbeatTime);
      }

      report->close(); report->close(); report->close(); report->close();
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
  return true;
}

bool Supervision::doChecks (bool timedout) {

  if (_agent == nullptr) {
    return false;
  }

  _snapshot = _agent->readDB().get("/");
  
  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Sanity checks";
  std::vector<check_t> ret = checkDBServers();
  
  return true;
  
}

void Supervision::run() {

  CONDITION_LOCKER(guard, _cv);
  TRI_ASSERT(_agent!=nullptr);
  bool timedout = false;

  while (!this->isStopping()) {
    
    doChecks(timedout);

    if (_agent->leading()) {
      timedout = _cv.wait(_frequency*1000000);//quarter second
    } else {
      _cv.wait();
    }
    
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
  _frequency = static_cast<long>(_agent->config().supervisionFrequency);
  return start();
}

void Supervision::beginShutdown() {
  // Personal hygiene
  Thread::beginShutdown();
}

Store const& Supervision::store() const {
  return _agent->readDB();
}
