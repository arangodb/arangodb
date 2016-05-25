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

#include <thread>
using namespace arangodb;

using namespace arangodb::consensus;

std::string printTimestamp(Supervision::TimePoint const& t) {
  time_t tt = std::chrono::system_clock::to_time_t(t);
  struct tm tb;
  size_t const len (21);
  char buffer[len];
  TRI_gmtime(tt, &tb);
  ::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);
  return std::string(buffer, len-1);
}

inline arangodb::consensus::write_ret_t transact (
  Agent* _agent, Builder const& transaction) {
  
  query_t envelope = std::make_shared<Builder>();

  try {
    envelope->openArray();
    envelope->add(transaction.slice());
    envelope->close();
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Supervision failed to build transaction.";
    LOG_TOPIC(ERR, Logger::AGENCY) << e.what();
  }
  
  LOG(INFO) << envelope->toJson();
  return _agent->write(envelope);
  
}

enum JOB_STATUS {TODO, PENDING, FINISHED, FAILED, NOTFOUND};

static std::string const pendingPrefix = "/Target/Pending/";
static std::string const finishedPrefix = "/Target/Finished/";
static std::string const failedPrefix = "/Target/Failed/";
static std::string const planColPrefix = "/Plan/Collections/";
static std::string const curColPrefix = "/Current/Collections/";
static std::string const toDoPrefix = "/Target/ToDo/";
static std::string const blockedServersPrefix = "/Supervision/DBServers/";
static std::string const blockedShardsPrefix = "/Supervision/Shards/";
static std::string const planVersion = "/Plan/Version";

Job::Job(Node const& snapshot, Agent* agent, std::string const& jobId,
         std::string const& creator, std::string const& agencyPrefix) :
  _snapshot(snapshot), _agent(agent), _jobId(jobId), _creator(creator),
  _agencyPrefix(agencyPrefix) {}

Job::~Job() {}

bool Job::exists() const {
  
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


bool Job::finish(bool success = true) const {

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
               VPackValue(printTimestamp(std::chrono::system_clock::now())));
  for (auto const& obj : VPackObjectIterator(pending.slice()[0])) {
    finished.add(obj.key.copyString(), obj.value);
  }
  finished.close();
  
  // --- Delete pending
  finished.add(_agencyPrefix + pendingPrefix + _jobId,
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


struct MoveShard : public Job {

  MoveShard(Node const& snapshot, Agent* agent, std::string const& jobId,
            std::string const& creator, std::string const& agencyPrefix,
            std::string const& database = std::string(),
            std::string const& collection = std::string(),
            std::string const& shard = std::string(),
            std::string const& from = std::string(),
            std::string const& to = std::string()) :
    Job(snapshot, agent, jobId, creator, agencyPrefix), _database(database),
    _collection(collection), _shard(shard), _from(from), _to(to) {
    
    if (exists()) {
      if (!status()) {  
        start();        
      } 
    } else {            
      create();
      start();
    }

  }

  virtual ~MoveShard() {}

  virtual bool create () const {

    LOG_TOPIC(INFO, Logger::AGENCY) << "Todo: Move shard " + _shard
      + " from " + _from + " to " + _to;

    std::string path = _agencyPrefix + toDoPrefix + _jobId;

    Builder todo;
    todo.openArray();
    todo.openObject();
    todo.add(path, VPackValue(VPackValueType::Object));
    todo.add("creator", VPackValue(_creator));
    todo.add("type", VPackValue("moveShard"));
    todo.add("database", VPackValue(_database));
    todo.add("collection", VPackValue(_collection));
    todo.add("shard", VPackValue(_shard));
    todo.add("fromServer", VPackValue(_from));
    todo.add("toServer", VPackValue(_to));
    todo.add("isLeader", VPackValue(true));    
    todo.add("jobId", VPackValue(_jobId));
    todo.add("timeCreated",
             VPackValue(printTimestamp(std::chrono::system_clock::now())));
    todo.close(); todo.close(); todo.close();
    
    write_ret_t res = transact(_agent, todo);
    
    if (res.accepted && res.indices.size()==1 && res.indices[0]) {
      return true;
    }
    
    LOG_TOPIC(INFO, Logger::AGENCY) << "Failed to insert job " + _jobId;
    return false;

  }
  
  virtual bool start() const {

    // DBservers
    std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
    //Node const& planned = _snapshot(planPath);
    
    std::string curPath =
      curColPrefix + _database + "/" + _collection + "/" + _shard + "/servers";
    Node const& current = _snapshot(curPath);
    
    if (current.slice().length() == 1) {
      LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to move shard from " + _from
        + " to " + _to + ". No in-sync followers:" + current.slice().toJson();
      return false;
    }
    
    // Copy todo to pending
    Builder todo, pending;
    
    // Get todo entry
    todo.openArray();
    _snapshot(toDoPrefix + _jobId).toBuilder(todo);
    todo.close();

    // Transaction
    pending.openArray();

    // Apply
    // --- Add pending entry
    pending.openObject();
    pending.add(_agencyPrefix + pendingPrefix + _jobId,
                VPackValue(VPackValueType::Object));
    pending.add("timeStarted",
                VPackValue(printTimestamp(std::chrono::system_clock::now())));
    for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
      pending.add(obj.key.copyString(), obj.value);
    }
    pending.close();

    // --- Remove todo entry
    pending.add(_agencyPrefix + toDoPrefix + _jobId,
                VPackValue(VPackValueType::Object));
    pending.add("op", VPackValue("delete"));
    pending.close();

    // --- Cyclic shift in sync servers
    pending.add(_agencyPrefix + planPath, VPackValue(VPackValueType::Array));
    for (size_t i = 1; i < current.slice().length(); ++i) {
      pending.add(current.slice()[i]);
    }
    pending.add(current.slice()[0]);
    pending.close();
    
    // --- Block shard
    pending.add(_agencyPrefix +  blockedShardsPrefix + _shard,
                VPackValue(VPackValueType::Object));
    pending.add("jobId", VPackValue(_jobId));
    pending.close();

    // --- Increment Plan/Version
    pending.add(_agencyPrefix +  planVersion,
                VPackValue(VPackValueType::Object));
    pending.add("op", VPackValue("increment"));
    pending.close();
    
    pending.close();

    // Precondition
    // --- Check that Current servers are as we expect
    pending.openObject();
    pending.add(_agencyPrefix + curPath, VPackValue(VPackValueType::Object));
    pending.add("old", current.slice());
    pending.close();

    // --- Check if shard is not blocked
    pending.add(_agencyPrefix + blockedShardsPrefix + _shard,
                VPackValue(VPackValueType::Object));
    pending.add("oldEmpty", VPackValue(true));
    pending.close();

    pending.close(); pending.close();

    // Transact 
    write_ret_t res = transact(_agent, pending);

    if (res.accepted && res.indices.size()==1 && res.indices[0]) {
      
      LOG_TOPIC(INFO, Logger::AGENCY) << "Pending: Move shard " + _shard
        + " from " + _from + " to " + _to;
      return true;
    }    

    LOG_TOPIC(INFO, Logger::AGENCY) <<
      "Precondition failed for starting job " + _jobId;
    return false;

  }


  virtual unsigned status () const {

    Node const& target = _snapshot("/Target");
    
    if        (target.exists(std::string("/ToDo/")     + _jobId).size() == 2) {
      
      return TODO;
      
    } else if (target.exists(std::string("/Pending/")  + _jobId).size() == 2) {
      
      Node const& job = _snapshot(pendingPrefix + _jobId);
      std::string database = job("database").toJson(),
        collection = job("collection").toJson(),
        shard = job("shard").toJson(),
        planPath = planColPrefix + database + "/" + collection + "/shards/"
          + shard,
        curPath = curColPrefix + database + "/" + collection + "/" + shard
          + "/servers";

      Node const& planned = _snapshot(planPath);
      Node const& current = _snapshot(curPath);

      if (planned.slice()[0] == current.slice()[0]) {

        if (finish()) {
          return FINISHED;
        }
        
      }

      return PENDING;
      
    } else if (target.exists(std::string("/Finished/")  + _jobId).size() == 2) {
      
      return FINISHED;
      
    } else if (target.exists(std::string("/Failed/")  + _jobId).size() == 2) {
      
      return FAILED;

    } 
    
    return NOTFOUND;
    
  }



  std::string const& _database;
  std::string const& _collection;
  std::string const& _shard;
  std::string const& _from;
  std::string const& _to;
  
};

struct FailedServer : public Job {

  FailedServer(Node const& snapshot, Agent* agent, std::string const& jobId,
               std::string const& creator, std::string const& agencyPrefix,
               std::string const& failed) :
    Job(snapshot, agent, jobId, creator, agencyPrefix), _failed(failed) {
    
    if (exists()) {
      if (status() == TODO) {  
        start();        
      } 
    } else {            
      create();
      start();
    }

  }
  
  virtual ~FailedServer () {}

  virtual bool start() const {

    // Copy todo to pending
    Builder todo, pending;

    // Get todo entry
    todo.openArray();
    _snapshot(toDoPrefix + _jobId).toBuilder(todo);
    todo.close();

    // Prepare peding entry, block toserver
    pending.openArray();

    // --- Add pending
    pending.openObject();
    pending.add(_agencyPrefix + pendingPrefix + _jobId,
                VPackValue(VPackValueType::Object));
    pending.add("timeStarted",
                VPackValue(printTimestamp(std::chrono::system_clock::now())));
    for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
      pending.add(obj.key.copyString(), obj.value);
    }
    pending.close();
    
    // --- Delete todo
    pending.add(_agencyPrefix + toDoPrefix + _jobId,
                VPackValue(VPackValueType::Object));
    pending.add("op", VPackValue("delete"));
    pending.close();

    // --- Block toServer
    pending.add(_agencyPrefix +  blockedServersPrefix + _failed,
                VPackValue(VPackValueType::Object));
    pending.add("jobId", VPackValue(_jobId));
    pending.close();
    
    pending.close();

    // Preconditions
    // --- Check that toServer not blocked
    pending.openObject();
    pending.add(_agencyPrefix +  blockedServersPrefix + _failed,
                VPackValue(VPackValueType::Object));
    pending.add("oldEmpty", VPackValue(true));
    pending.close();

    pending.close(); pending.close();

    // Transact to agency
    write_ret_t res = transact(_agent, pending);

    if (res.accepted && res.indices.size()==1 && res.indices[0]) {
      
      LOG_TOPIC(INFO, Logger::AGENCY) <<
        "Pending: DB Server " + _failed + " failed.";
      
      Node::Children const& databases =
        _snapshot("/Plan/Collections").children();

      size_t sub = 0;

      for (auto const& database : databases) {
        for (auto const& collptr : database.second->children()) {
          Node const& collection = *(collptr.second);
          Node const& replicationFactor = collection("replicationFactor");
          if (replicationFactor.slice().getUInt() > 1) {
            for (auto const& shard : collection("shards").children()) {
              VPackArrayIterator dbsit(shard.second->slice());
              
              // Only proceed if leader and create job
              if ((*dbsit.begin()).copyString() != _failed) {
                continue;
              }

              MoveShard(
                _snapshot, _agent, _jobId + "-" + std::to_string(sub++), _jobId,
                _agencyPrefix, database.first, collptr.first, shard.first,
                _failed, shard.second->slice()[1].copyString());
              
            } 
          }
        }
      }
          
      return true;
    }

    LOG_TOPIC(INFO, Logger::AGENCY) <<
      "Precondition failed for starting job " + _jobId;

    return false;
    
  }

  virtual bool create () const {

    LOG_TOPIC(INFO, Logger::AGENCY)
      << "Todo: DB Server " + _failed + " failed.";

    std::string path = _agencyPrefix + toDoPrefix + _jobId;

    Builder todo;
    todo.openArray();
    todo.openObject();
    todo.add(path, VPackValue(VPackValueType::Object));
    todo.add("type", VPackValue("failedServer"));
    todo.add("server", VPackValue(_failed));
    todo.add("jobId", VPackValue(_jobId));
    todo.add("creator", VPackValue(_creator));
    todo.add("timeCreated",
             VPackValue(printTimestamp(std::chrono::system_clock::now())));
    todo.close(); todo.close(); todo.close();

    write_ret_t res = transact(_agent, todo);

    if (res.accepted && res.indices.size()==1 && res.indices[0]) {
      return true;
    }

    LOG_TOPIC(INFO, Logger::AGENCY) << "Failed to insert job " + _jobId;
    return false;
    
  }
  
  virtual unsigned status () const {
    
    Node const& target = _snapshot("/Target");
    
    if        (target.exists(std::string("/ToDo/")     + _jobId).size() == 2) {

      return TODO;

    } else if (target.exists(std::string("/Pending/")  + _jobId).size() == 2) {

      Node::Children const& subJobs = _snapshot(pendingPrefix).children();

      size_t found = 0;
      
      for (auto const& subJob : subJobs) {
        if (!subJob.first.compare(0, _jobId.size()+1, _jobId + "-")) {
          found++;
          Node const& sj = *(subJob.second);
          std::string subJobId = sj("jobId").slice().copyString();
          std::string creator  = sj("creator").slice().copyString();
          MoveShard(_snapshot, _agent, subJobId, creator, _agencyPrefix);
        }
      }

      if (!found) {
        if (finish()) {
          return FINISHED;
        }
      }
      
      return PENDING;
      
    } else if (target.exists(std::string("/Finished/")  + _jobId).size() == 2) {
      
      return FINISHED;
      
    } else if (target.exists(std::string("/Failed/")  + _jobId).size() == 2) {
      
      return FAILED;
      
    }
    
    return NOTFOUND;
    
  }
  
  std::string const& _failed;
  
};


struct CleanOutServer : public Job {
  
  CleanOutServer (Node const& snapshot, Agent* agent, std::string const& jobId,
                  std::string const& creator, std::string const& prefix,
                  std::string const& server) : 
    Job(snapshot, agent, jobId, creator, prefix), _server(server) {
  }

  virtual ~CleanOutServer () {}

  virtual unsigned status () const {
    return 0;
  }

  virtual bool create () const {

    LOG_TOPIC(INFO, Logger::AGENCY) << "Todo: Clean out server " + _server;
    
    std::string path = _agencyPrefix + toDoPrefix + _jobId;

    Builder todo;
    todo.openArray();
    todo.openObject();
    todo.add(path, VPackValue(VPackValueType::Object));
    todo.add("type", VPackValue("cleanOutServer"));
    todo.add("server", VPackValue(_server));
    todo.add("jobId", VPackValue(_jobId));
    todo.add("creator", VPackValue(_creator));
    todo.add("timeCreated",
             VPackValue(printTimestamp(std::chrono::system_clock::now())));
    todo.close(); todo.close(); todo.close();

    write_ret_t res = transact(_agent, todo);

    if (res.accepted && res.indices.size()==1 && res.indices[0]) {
      return true;
    }

    LOG_TOPIC(INFO, Logger::AGENCY) << "Failed to insert job " + _jobId;
    return false;

  };

  virtual bool start() const {

    // Copy todo to pending
    Builder todo, pending;
    
    // Get todo entry
    todo.openArray();
    _snapshot(toDoPrefix + _jobId).toBuilder(todo);
    todo.close();

    // Prepare peding entry, block toserver
    pending.openArray();
    
    // --- Add pending
    pending.openObject();
    pending.add(_agencyPrefix + pendingPrefix + _jobId,
                VPackValue(VPackValueType::Object));
    pending.add("timeStarted",
                VPackValue(printTimestamp(std::chrono::system_clock::now())));
    for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
      pending.add(obj.key.copyString(), obj.value);
    }
    pending.close();
    
    // --- Delete todo
    pending.add(_agencyPrefix + toDoPrefix + _jobId,
                VPackValue(VPackValueType::Object));
    pending.add("op", VPackValue("delete"));
    pending.close();
    
    // --- Block toServer
    pending.add(_agencyPrefix +  blockedServersPrefix + _server,
                VPackValue(VPackValueType::Object));
    pending.add("jobId", VPackValue(_jobId));
    pending.close();
    
    pending.close();
    
    // Preconditions
    // --- Check that toServer not blocked
    pending.openObject();
    pending.add(_agencyPrefix +  blockedServersPrefix + _server,
                VPackValue(VPackValueType::Object));
    pending.add("oldEmpty", VPackValue(true));
    pending.close();

    pending.close(); pending.close();
    
    // Transact to agency
    write_ret_t res = transact(_agent, pending);
    
    if (res.accepted && res.indices.size()==1 && res.indices[0]) {
      
      LOG_TOPIC(INFO, Logger::AGENCY) << "Pending: Clean out server " + _server;
      
      Node::Children const& databases =
        _snapshot("/Plan/Collections").children();
      
      size_t sub = 0;
    
      for (auto const& database : databases) {
        for (auto const& collptr : database.second->children()) {
          Node const& collection = *(collptr.second);
          Node const& replicationFactor = collection("replicationFactor");
          if (replicationFactor.slice().getUInt() > 1) {
            for (auto const& shard : collection("shards").children()) {
              VPackArrayIterator dbsit(shard.second->slice());
              
              // Only proceed if leader and create job
              if ((*dbsit.begin()).copyString() != _server) {
                continue;
              }

              MoveShard(
                _snapshot, _agent, _jobId + "-" + std::to_string(sub++), _jobId,
                _agencyPrefix, database.first, collptr.first, shard.first,
                _server, shard.second->slice()[1].copyString());
              
            } 
          }
        }
      }
          
      return true;
    }

    LOG_TOPIC(INFO, Logger::AGENCY) <<
      "Precondition failed for starting job " + _jobId;

    return false;
    
    
  };

  std::string const& _server;
  
};


std::string Supervision::_agencyPrefix = "/arango";

Supervision::Supervision()
    : arangodb::Thread("Supervision"),
      _agent(nullptr),
      _snapshot("Supervision"),
      _frequency(5),
      _gracePeriod(10),
      _jobId(0),
      _jobIdMax(0) {}

Supervision::~Supervision() { shutdown(); };

void Supervision::wakeUp() {
  TRI_ASSERT(_agent != nullptr);
  _snapshot = _agent->readDB().get(_agencyPrefix);
  _cv.signal();
}

static std::string const syncPrefix = "/Sync/ServerStates/";
static std::string const supervisionPrefix = "/Supervision/Health/";
static std::string const planDBServersPrefix = "/Plan/DBServers";

std::vector<check_t> Supervision::checkDBServers() {
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

    if (it != _vitalSigns.end()) {  // Existing server

      query_t report = std::make_shared<Builder>();
      report->openArray();
      report->openArray();
      report->openObject();
      report->add(_agencyPrefix + supervisionPrefix + serverID,
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
        if (t.count() > _gracePeriod) {  // Failure
          if (it->second->maintenance() == "0") {
            it->second->maintenance(std::to_string(_jobId++));
          }
          FailedServer fsj(_snapshot, _agent, it->second->maintenance(),
                           "supervision", _agencyPrefix, serverID);
        }
        
      } else {
        report->add("Status", VPackValue("UP"));
        it->second->update(lastHeartbeatStatus, lastHeartbeatTime);
      }
      
      report->close();
      report->close();
      report->close();
      report->close();
      _agent->write(report);
      
    } else {  // New server
      _vitalSigns[serverID] =
          std::make_shared<VitalSign>(lastHeartbeatStatus, lastHeartbeatTime);
    }
  }

  auto itr = _vitalSigns.begin();
  while (itr != _vitalSigns.end()) {
    if (machinesPlanned.find(itr->first) == machinesPlanned.end()) {
      itr = _vitalSigns.erase(itr);
    } else {
      ++itr;
    }
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
