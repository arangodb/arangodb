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

#include <thread>

#include "Agency/AddFollower.h"
#include "Agency/Agent.h"
#include "Agency/CleanOutServer.h"
#include "Agency/FailedServer.h"
#include "Agency/Job.h"
#include "Agency/JobContext.h"
#include "Agency/RemoveFollower.h"
#include "Agency/Store.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Cluster/ServerState.h"

using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::application_features;

struct HealthRecord {

  std::string shortName;
  std::string syncTime;
  std::string syncStatus;
  std::string status;
  std::string endpoint;
  std::string lastAcked;
  std::string hostId;
  size_t version;

  HealthRecord() : version(0) {}

  HealthRecord(
    std::string const& sn, std::string const& ep, std::string const& ho) :
    shortName(sn), endpoint(ep), hostId(ho), version(0) {}

  HealthRecord(Node const& node) {
    *this = node;
  }

  HealthRecord(HealthRecord const& other) {
    *this = other;
  }

  HealthRecord& operator=(Node const& node) {
    version = 0;
    if (shortName.empty()) {
      shortName = node("ShortName").getString();
    }
    if (endpoint.empty()) {
      endpoint = node("Endpoint").getString();
    }
    if (node.has("Status")) {
      status = node("Status").getString();
      if (node.has("SyncStatus")) { // New format
        version = 2;
        syncStatus = node("SyncStatus").getString();
        if (node.has("SyncTime")) {
          syncTime = node("SyncTime").getString();
        }
        if (node.has("LastAcked")) {
          lastAcked = node("LastAcked").getString();
        }
      } else if (node.has("LastHeartbeatStatus")) {
        version = 1;
        syncStatus = node("LastHeartbeatStatus").getString();
        if (node.has("LastHeartbeatSent")) {
          syncTime = node("LastHeartbeatSent").getString();
        }
        if (node.has("LastHeartbeatAcked")) {
          lastAcked = node("LastHeartbeatAcked").getString();
        }
      }
      if (node.has("Host")) {
        hostId = node("Host").getString();
      }
    }
    return *this;
  }

  HealthRecord& operator=(HealthRecord const& other) {
    shortName = other.shortName;
    syncStatus = other.syncStatus;
    status = other.status;
    endpoint = other.endpoint;
    hostId = other.hostId;
    version = other.version;
    return *this;
  }

  void toVelocyPack(VPackBuilder& obj) const {
    TRI_ASSERT(obj.isOpenObject());
    obj.add("ShortName", VPackValue(shortName));
    obj.add("Endpoint", VPackValue(endpoint));
    obj.add("Host", VPackValue(hostId));
    obj.add("SyncStatus", VPackValue(syncStatus));
    obj.add("Status", VPackValue(status));
    if (syncTime.empty()) {
      obj.add("Timestamp",
              VPackValue(timepointToString(std::chrono::system_clock::now())));
    } else {
      obj.add("SyncTime", VPackValue(syncTime));
      obj.add("LastAcked", VPackValue(lastAcked));
    }
  }

  bool statusDiff(HealthRecord const& other) {
    return (status != other.status || syncStatus != other.syncStatus);
  }

  friend std::ostream& operator<<(std::ostream& o, HealthRecord const& hr) {
    VPackBuilder builder;
    { VPackObjectBuilder b(&builder);
      hr.toVelocyPack(builder); }
    o << builder.toJson();
    return o;
  }

};


// This is initialized in AgencyFeature:
std::string Supervision::_agencyPrefix = "/arango";

Supervision::Supervision()
  : arangodb::Thread("Supervision"),
  _agent(nullptr),
  _snapshot("Supervision"),
  _transient("Transient"),
  _frequency(1.),
  _gracePeriod(5.),
  _okThreshold(1.5),
  _jobId(0),
  _jobIdMax(0),
  _selfShutdown(false),
  _upgraded(false) {}

Supervision::~Supervision() {
  if (!isStopping()) {
    shutdown();
  }
}

static std::string const syncPrefix = "/Sync/ServerStates/";
static std::string const supervisionPrefix = "/Supervision";
static std::string const healthPrefix = "/Supervision/Health/";
static std::string const targetShortID = "/Target/MapUniqueToShortID/";
static std::string const currentServersRegisteredPrefix =
  "/Current/ServersRegistered";
static std::string const foxxmaster = "/Current/Foxxmaster";
static std::string const asyncReplLeader = "/Plan/AsyncReplication/Leader";

void Supervision::upgradeOne(Builder& builder) {
  _lock.assertLockedByCurrentThread();
  // "/arango/Agency/Definition" not exists or is 0
  if (!_snapshot.has("Agency/Definition")) {
    { VPackArrayBuilder trx(&builder);
      { VPackObjectBuilder oper(&builder);
        builder.add("/Agency/Definition", VPackValue(1));
        builder.add(VPackValue("/Target/ToDo"));
        { VPackObjectBuilder empty(&builder); }
        builder.add(VPackValue("/Target/Pending"));
        { VPackObjectBuilder empty(&builder); }
      }
      { VPackObjectBuilder o(&builder);
        builder.add(VPackValue("/Agency/Definition"));
        { VPackObjectBuilder prec(&builder);
          builder.add("oldEmpty", VPackValue(true));
        }
      }
    }
  }
}

void Supervision::upgradeZero(Builder& builder) {
  _lock.assertLockedByCurrentThread();
  // "/arango/Target/FailedServers" is still an array
  Slice fails = _snapshot(failedServersPrefix).slice();
  if (_snapshot(failedServersPrefix).slice().isArray()) {
    { VPackArrayBuilder trx(&builder);
      { VPackObjectBuilder o(&builder);
        builder.add(VPackValue(failedServersPrefix));
        { VPackObjectBuilder oo(&builder);
          if (fails.length() > 0) {
            for (VPackSlice fail : VPackArrayIterator(fails)) {
              builder.add(VPackValue(fail.copyString()));
              { VPackObjectBuilder ooo(&builder); }
            }
          }
        }}} // trx
  }
}

void Supervision::upgradeHealthRecords(Builder& builder) {
  _lock.assertLockedByCurrentThread();
  // "/arango/Supervision/health" is in old format
  Builder b;
  size_t n = 0;

  if (_snapshot.has(healthPrefix)) {
    HealthRecord hr;
    { VPackObjectBuilder oo(&b);
      for (auto recPair : _snapshot(healthPrefix).children()) {
        if (recPair.second->has("ShortName") &&
            recPair.second->has("Endpoint")) {
          hr = *recPair.second;
          if (hr.version == 1) {
            ++n;
            b.add(VPackValue(recPair.first));
            { VPackObjectBuilder ooo(&b);
              hr.toVelocyPack(b);

            }
          }
        }
      }
    }
  }

  if (n>0) {
    { VPackArrayBuilder trx(&builder);
      { VPackObjectBuilder o(&builder);
        b.add(healthPrefix, b.slice());
      }
    }
  }

}

// Upgrade agency, guarded by wakeUp
void Supervision::upgradeAgency() {
  _lock.assertLockedByCurrentThread();

  Builder builder;
  {
    VPackArrayBuilder trxs(&builder);
    upgradeZero(builder);
    fixPrototypeChain(builder);
    upgradeOne(builder);
    upgradeHealthRecords(builder);
  }

  LOG_TOPIC(DEBUG, Logger::AGENCY)
    << "Upgrading the agency:" << builder.toJson();

  if (builder.slice().length() > 0) {
    generalTransaction(_agent, builder);
  }

  _upgraded = true;

}

void handleOnStatusDBServer(
  Agent* agent, Node const& snapshot, HealthRecord& persisted,
  HealthRecord& transisted, std::string const& serverID,
  uint64_t const& jobId, std::shared_ptr<VPackBuilder>& envelope) {

  std::string failedServerPath = failedServersPrefix + "/" + serverID;

  // New condition GOOD:
  if (transisted.status == Supervision::HEALTH_STATUS_GOOD) {
    if (snapshot.has(failedServerPath)) {
      envelope = std::make_shared<VPackBuilder>();
      { VPackArrayBuilder a(envelope.get());
        { VPackObjectBuilder operations (envelope.get());
          envelope->add(VPackValue(failedServerPath));
          { VPackObjectBuilder ccc(envelope.get());
            envelope->add("op", VPackValue("delete")); }}}
    }
  } else if ( // New state: FAILED persisted: GOOD (-> BAD)
    persisted.status == Supervision::HEALTH_STATUS_GOOD &&
    transisted.status != Supervision::HEALTH_STATUS_GOOD) {
    transisted.status = Supervision::HEALTH_STATUS_BAD;
  } else if ( // New state: FAILED persisted: BAD (-> Job)
    persisted.status == Supervision::HEALTH_STATUS_BAD &&
    transisted.status == Supervision::HEALTH_STATUS_FAILED ) {
    if (!snapshot.has(failedServerPath)) {
      envelope = std::make_shared<VPackBuilder>();
      FailedServer(snapshot, agent, std::to_string(jobId),
                   "supervision", serverID).create(envelope);
    }
  }

}


void handleOnStatusCoordinator(
  Agent* agent, Node const& snapshot, HealthRecord& persisted,
  HealthRecord& transisted, std::string const& serverID) {

  if (transisted.status == Supervision::HEALTH_STATUS_FAILED) {
    // if the current foxxmaster server failed => reset the value to ""
    if (snapshot.has(foxxmaster) && snapshot(foxxmaster).getString() == serverID) {
      VPackBuilder create;
      { VPackArrayBuilder tx(&create);
        { VPackObjectBuilder d(&create);
          create.add(foxxmaster, VPackValue("")); }}
      singleWriteTransaction(agent, create);
    }

  }
}

void handleOnStatusSingle(
   Agent* agent, Node const& snapshot, HealthRecord& persisted,
   HealthRecord& transisted, std::string const& serverID) {
  // if the current leader server failed => reset the value to ""
  if (transisted.status == Supervision::HEALTH_STATUS_FAILED) {

    if (snapshot.has(asyncReplLeader) && snapshot(asyncReplLeader).getString() == serverID) {
      VPackBuilder create;
      { VPackArrayBuilder tx(&create);
        { VPackObjectBuilder d(&create);
          create.add(asyncReplLeader, VPackValue("")); }}
      singleWriteTransaction(agent, create);
    }

  }
}

void handleOnStatus(
  Agent* agent, Node const& snapshot, HealthRecord& persisted,
  HealthRecord& transisted, std::string const& serverID,
  uint64_t const& jobId, std::shared_ptr<VPackBuilder>& envelope) {

  if (serverID.compare(0,4,"PRMR") == 0) {
    handleOnStatusDBServer(
      agent, snapshot, persisted, transisted, serverID, jobId, envelope);
  } else if (serverID.compare(0,4,"CRDN") == 0) {
    handleOnStatusCoordinator(
      agent, snapshot, persisted, transisted, serverID);
  } else if (serverID.compare(0,4,"SNGL") == 0) {
    handleOnStatusSingle(agent, snapshot, persisted, transisted, serverID);
  } else {
    LOG_TOPIC(ERR, Logger::SUPERVISION)
      << "Unknown server type. No supervision action taken. " << serverID;
  }

}

// Build transaction for removing unattended servers from health monitoring
query_t arangodb::consensus::removeTransactionBuilder(
  std::vector<std::string> const& todelete) {
  
  query_t del = std::make_shared<Builder>();
  { VPackArrayBuilder trxs(del.get());
    { VPackArrayBuilder trx(del.get());
      { VPackObjectBuilder server(del.get());
        for (auto const& srv : todelete) {
          del->add(
            VPackValue(Supervision::agencyPrefix() +
                       arangodb::consensus::healthPrefix + srv));
          { VPackObjectBuilder oper(del.get());
            del->add("op", VPackValue("delete")); }}}}}
  return del;
  
}

// Check all DB servers, guarded above doChecks
std::vector<check_t> Supervision::check(std::string const& type) {

  // Dead lock detection
  _lock.assertLockedByCurrentThread();

  // Book keeping
  std::vector<check_t> ret;
  auto const& machinesPlanned = _snapshot(std::string("Plan/")+type).children();
  auto const& serversRegistered = _snapshot(currentServersRegisteredPrefix);
  std::vector<std::string> todelete;
  for (auto const& machine : _snapshot(healthPrefix).children()) {
    if ((type == "DBServers" && machine.first.compare(0,4,"PRMR") == 0) ||
        (type == "Coordinators" && machine.first.compare(0,4,"CRDN") == 0) ||
        (type == "Singles" && machine.first.compare(0,4,"SNGL") == 0)) {
      todelete.push_back(machine.first);
    }
  }

  // Remove all machines, which are no longer planned
  for (auto const& machine : machinesPlanned) {
    todelete.erase(
      std::remove(
        todelete.begin(), todelete.end(), machine.first), todelete.end());
  }
  if (!todelete.empty()) {
    _agent->write(removeTransactionBuilder(todelete));
  }

  // Do actual monitoring
  for (auto const& machine : machinesPlanned) {
    std::string lastHeartbeatStatus, lastHeartbeatAcked, lastHeartbeatTime,
      lastStatus, serverID(machine.first), shortName;

    // short name arrives asynchronous to machine registering, make sure
    //  it has arrived before trying to use it
    if (LEAF == _snapshot(targetShortID + serverID + "/ShortName").type()) {

      shortName = _snapshot(targetShortID + serverID + "/ShortName").getString();

      // Endpoint
      std::string endpoint;
      std::string epPath = serverID + "/endpoint";
      if (serversRegistered.has(epPath)) {
        endpoint = serversRegistered(epPath).getString();
      }
      std::string hostId;
      std::string hoPath = serverID + "/host";
      if (serversRegistered.has(hoPath)) {
        hostId = serversRegistered(hoPath).getString();
      }

      // Health records from persistence, from transience and a new one
      HealthRecord transist(shortName, endpoint, hostId);
      HealthRecord persist(shortName, endpoint, hostId);

      // Get last health entries from transient and persistent key value stores
      if (_transient.has(healthPrefix + serverID)) {
        transist = _transient(healthPrefix + serverID);
      }
      if (_snapshot.has(healthPrefix + serverID)) {
        persist = _snapshot(healthPrefix + serverID);
      }

      // New health record (start with old add current information from sync)
      // Sync.time is copied to Health.syncTime
      // Sync.status is copied to Health.syncStatus
      std::string syncTime = _transient.has(syncPrefix + serverID) ?
        _transient(syncPrefix + serverID + "/time").getString() :
        timepointToString(std::chrono::system_clock::time_point());
      std::string syncStatus = _transient.has(syncPrefix + serverID) ?
        _transient(syncPrefix + serverID + "/status").getString() : "UNKNOWN";

      // Last change registered in sync (transient != sync)
      // Either now or value in transient
      auto lastAckedTime = (syncTime != transist.syncTime) ?
        std::chrono::system_clock::now() : stringToTimepoint(transist.lastAcked);
      transist.lastAcked = timepointToString(lastAckedTime);
      transist.syncTime = syncTime;
      transist.syncStatus = syncStatus;

      // Calculate elapsed since lastAcked
      auto elapsed = std::chrono::duration<double>(
        std::chrono::system_clock::now() - lastAckedTime);

      if (elapsed.count() <= _okThreshold) {
        transist.status = Supervision::HEALTH_STATUS_GOOD;
      } else if (elapsed.count() <= _gracePeriod) {
        transist.status = Supervision::HEALTH_STATUS_BAD;
      } else {
        transist.status = Supervision::HEALTH_STATUS_FAILED;
      }

      // Status changed?
      bool changed = transist.statusDiff(persist);

      // Take necessary actions if any
      std::shared_ptr<VPackBuilder> envelope;
      if (changed) {
        handleOnStatus(_agent, _snapshot, persist, transist, serverID, _jobId,
                       envelope);
      }

      persist = transist; // Now copy Status, SyncStatus from transient to persited

      // Transient report
      std::shared_ptr<Builder> tReport = std::make_shared<Builder>();
      { VPackArrayBuilder transaction(tReport.get());        // Transist Transaction
        std::shared_ptr<VPackBuilder> envelope;
        { VPackObjectBuilder operation(tReport.get());       // Operation
          tReport->add(VPackValue(healthPrefix + serverID)); // Supervision/Health
          { VPackObjectBuilder oo(tReport.get());
            transist.toVelocyPack(*tReport); }}} // Transaction

      // Persistent report
      std::shared_ptr<Builder> pReport = nullptr;
      if (changed) {
        pReport = std::make_shared<Builder>();
        { VPackArrayBuilder transaction(pReport.get());      // Persist Transaction
          { VPackObjectBuilder operation(pReport.get());     // Operation
            pReport->add(VPackValue(healthPrefix + serverID)); // Supervision/Health
            { VPackObjectBuilder oo(pReport.get());
              persist.toVelocyPack(*pReport); }
            if (envelope != nullptr) {                       // Failed server
              TRI_ASSERT(
                envelope->slice().isArray() && envelope->slice()[0].isObject());
              for (VPackObjectIterator::ObjectPair i : VPackObjectIterator(envelope->slice()[0])) {
                pReport->add(i.key.copyString(), i.value);
              }
            }} // Operation
          if (envelope != nullptr && envelope->slice().length()>1) {  // Preconditions(Job)
            TRI_ASSERT(
              envelope->slice().isArray() && envelope->slice()[1].isObject());
            pReport->add(envelope->slice()[1]);
          }} // Transaction
      }

      if (!this->isStopping()) {

        // Replicate special event and only then transient store
        if (changed) {
          write_ret_t res = singleWriteTransaction(_agent, *pReport);
          if (res.accepted && res.indices.front() != 0) {
            ++_jobId; // Job was booked
            transient(_agent, *tReport);
          }
        } else { // Nothing special just transient store
          transient(_agent, *tReport);
        }
      }
    } else {
      LOG_TOPIC(INFO, Logger::SUPERVISION) <<
        "Short name for << " << serverID << " not yet available.  Skipping health check.";
    } // else

  } // for

  return ret;
}


// Update local agency snapshot, guarded by callers
bool Supervision::updateSnapshot() {
  _lock.assertLockedByCurrentThread();

  if (_agent == nullptr || this->isStopping()) {
    return false;
  }

  _agent->executeLockedRead([&]() {
      if (_agent->readDB().has(_agencyPrefix)) {
        _snapshot = _agent->readDB().get(_agencyPrefix);
      }
      if (_agent->transient().has(_agencyPrefix)) {
        _transient = _agent->transient().get(_agencyPrefix);
      }
    });

  return true;

}

// All checks, guarded by main thread
bool Supervision::doChecks() {
  _lock.assertLockedByCurrentThread();
  TRI_ASSERT(ServerState::roleToAgencyListKey(ServerState::ROLE_PRIMARY) == "DBServers");
  check(ServerState::roleToAgencyListKey(ServerState::ROLE_PRIMARY));
  TRI_ASSERT(ServerState::roleToAgencyListKey(ServerState::ROLE_COORDINATOR) == "Coordinators");
  check(ServerState::roleToAgencyListKey(ServerState::ROLE_COORDINATOR));
  TRI_ASSERT(ServerState::roleToAgencyListKey(ServerState::ROLE_SINGLE) == "Singles");
  check(ServerState::roleToAgencyListKey(ServerState::ROLE_SINGLE));
  return true;
}

void Supervision::run() {
  // First wait until somebody has initialized the ArangoDB data, before
  // that running the supervision does not make sense and will indeed
  // lead to horrible errors:

  std::string const supervisionNode = _agencyPrefix + supervisionPrefix;

  while (!this->isStopping()) {
    {
      CONDITION_LOCKER(guard, _cv);
      _cv.wait(static_cast<uint64_t>(1000000 * _frequency));
    }

    bool done = false;
    MUTEX_LOCKER(locker, _lock);
    _agent->executeLockedRead([&]() {
      if (_agent->readDB().has(supervisionNode)) {
        try {
          _snapshot = _agent->readDB().get(supervisionNode);
          if (_snapshot.children().size() > 0) {
            done = true;
          }
        } catch (...) {
          LOG_TOPIC(WARN, Logger::SUPERVISION) <<
            "Main node in agency gone. Contact your db administrator.";
        }
      }
    });

    if (done) {
      break;
    }

    LOG_TOPIC(DEBUG, Logger::SUPERVISION) << "Waiting for ArangoDB to "
      "initialize its data.";
  }

  bool shutdown = false;
  {
    CONDITION_LOCKER(guard, _cv);
    TRI_ASSERT(_agent != nullptr);

    while (!this->isStopping()) {
      
      {
        MUTEX_LOCKER(locker, _lock);

        if (isShuttingDown()) {
          handleShutdown();
        } else if (_selfShutdown) {
          shutdown = true;
          break;
        }

        // Only modifiy this condition with extreme care:
        // Supervision needs to wait until the agent has finished leadership
        // preparation or else the local agency snapshot might be behind its
        // last state. 
        if (_agent->leading() && _agent->getPrepareLeadership() == 0) {

          if (_jobId == 0 || _jobId == _jobIdMax) {
            getUniqueIds();  // cannot fail but only hang
          }

          updateSnapshot();

          if (!_upgraded) {
            upgradeAgency();
          }

          if (_agent->leaderFor() > 10) {
            try {
              doChecks();
            } catch (std::exception const& e) {
              LOG_TOPIC(ERR, Logger::SUPERVISION)
                << e.what() << " " << __FILE__ << " " << __LINE__;
            } catch (...) {
              LOG_TOPIC(ERR, Logger::SUPERVISION) <<
                "Supervision::doChecks() generated an uncaught exception.";
            }
          }

          handleJobs();
        }
      }
      _cv.wait(static_cast<uint64_t>(1000000 * _frequency));
    }
  }
  
  if (shutdown) {
    ApplicationServer::server->beginShutdown();
  }
}

// Guarded by caller
bool Supervision::isShuttingDown() {
  _lock.assertLockedByCurrentThread();
  return (_snapshot.has("Shutdown") && _snapshot("Shutdown").isBool()) ?
    _snapshot("/Shutdown").getBool() : false;
}

// Guarded by caller
std::string Supervision::serverHealth(std::string const& serverName) {
  _lock.assertLockedByCurrentThread();
  std::string const serverStatus(healthPrefix + serverName + "/Status");
  return (_snapshot.has(serverStatus)) ?
    _snapshot(serverStatus).getString() : std::string();
}

// Guarded by caller
void Supervision::handleShutdown() {
  _lock.assertLockedByCurrentThread();

  _selfShutdown = true;
  LOG_TOPIC(DEBUG, Logger::SUPERVISION) << "Waiting for clients to shut down";
  auto const& serversRegistered =
      _snapshot(currentServersRegisteredPrefix).children();
  bool serversCleared = true;
  for (auto const& server : serversRegistered) {
    if (server.first == "Version") {
      continue;
    }

    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "Waiting for " << server.first << " to shutdown";

    if (serverHealth(server.first) != HEALTH_STATUS_GOOD) {
      LOG_TOPIC(WARN, Logger::SUPERVISION)
        << "Server " << server.first << " did not shutdown properly it seems!";
      continue;
    }
    serversCleared = false;
  }

  if (serversCleared) {
    if (_agent->leading()) {
      auto del = std::make_shared<Builder>();
      { VPackArrayBuilder txs(del.get());
        { VPackArrayBuilder tx(del.get());
          { VPackObjectBuilder o(del.get());
            del->add(VPackValue(_agencyPrefix + "/Shutdown"));
            { VPackObjectBuilder oo(del.get());
              del->add("op", VPackValue("delete")); }}}}
      auto result = _agent->write(del);
      if (result.indices.size() != 1) {
        LOG_TOPIC(ERR, Logger::SUPERVISION)
          << "Invalid resultsize of " << result.indices.size()
          << " found during shutdown";
      } else {
        if (!_agent->waitFor(result.indices.at(0))) {
          LOG_TOPIC(ERR, Logger::SUPERVISION)
            << "Result was not written to followers during shutdown";
        }
      }
    }
  }
}

// Guarded by caller
bool Supervision::handleJobs() {
  _lock.assertLockedByCurrentThread();
  // Do supervision

  shrinkCluster();
  enforceReplication();
  workJobs();

  return true;
}

// Guarded by caller
void Supervision::workJobs() {
  _lock.assertLockedByCurrentThread();

  for (auto const& todoEnt : _snapshot(toDoPrefix).children()) {
    JobContext(
      TODO, (*todoEnt.second)("jobId").getString(), _snapshot, _agent).run();
  }

  for (auto const& pendEnt : _snapshot(pendingPrefix).children()) {
    JobContext(
      PENDING, (*pendEnt.second)("jobId").getString(), _snapshot, _agent).run();
  }

}


void Supervision::enforceReplication() {
  _lock.assertLockedByCurrentThread();
  auto const& plannedDBs = _snapshot(planColPrefix).children();

  for (const auto& db_ : plannedDBs) { // Planned databases
    auto const& db = *(db_.second);
    for (const auto& col_ : db.children()) { // Planned collections
      auto const& col = *(col_.second);

      size_t replicationFactor;
      if (col.has("replicationFactor") && col("replicationFactor").isUInt()) {
        replicationFactor = col("replicationFactor").getUInt();
      } else {
        LOG_TOPIC(DEBUG, Logger::SUPERVISION)
          << "no replicationFactor entry in " << col.toJson();
        continue;
      }

      // mop: satellites => distribute to every server
      if (replicationFactor == 0) {
        auto available = Job::availableServers(_snapshot);
        replicationFactor = available.size();
      }

      bool clone = col.has("distributeShardsLike");

      if (!clone) {
        for (auto const& shard_ : col("shards").children()) { // Pl shards
          auto const& shard = *(shard_.second);

          size_t actualReplicationFactor = shard.slice().length();
          if (actualReplicationFactor != replicationFactor) {
            // Check that there is not yet an addFollower or removeFollower
            // or moveShard job in ToDo for this shard:
            auto const& todo = _snapshot(toDoPrefix).children();
            bool found = false;
            for (auto const& pair : todo) {
              auto const& job = pair.second;
              if (job->has("type") &&
                  ((*job)("type").getString() == "addFollower" ||
                   (*job)("type").getString() == "removeFollower" ||
                   (*job)("type").getString() == "moveShard") &&
                  job->has("shard") &&
                  (*job)("shard").getString() == shard_.first) {
                found = true;
                LOG_TOPIC(DEBUG, Logger::SUPERVISION) << "already found "
                  "addFollower or removeFollower job in ToDo, not scheduling "
                  "again for shard " << shard_.first;
                break;
              }
            }
            // Check that shard is not locked:
            if (_snapshot.has(blockedShardsPrefix + shard_.first)) {
              found = true;
            }
            if (!found) {
              if (actualReplicationFactor < replicationFactor) {
                AddFollower(
                  _snapshot, _agent, std::to_string(_jobId++), "supervision",
                  db_.first, col_.first, shard_.first).run();
              } else {
                RemoveFollower(
                  _snapshot, _agent, std::to_string(_jobId++), "supervision",
                  db_.first, col_.first, shard_.first).run();
              }
            }
          }
        }
      }
    }
  }

}

void Supervision::fixPrototypeChain(Builder& migrate) {
  _lock.assertLockedByCurrentThread();

  auto const& snap = _snapshot;

  std::function<std::string (std::string const&, std::string const&)> resolve;
  resolve = [&] (std::string const& db, std::string const& col) {
    Node const& n = snap(planColPrefix + db + "/" + col);
    std::string s;
    if (n.has("distributeShardsLike")) {
      s = n("distributeShardsLike").getString();
    }
    return (s.empty()) ? col : resolve(db, s);
  };

  for (auto const& database : _snapshot(planColPrefix).children()) {
    for (auto const& collection : database.second->children()) {
      if (collection.second->has("distributeShardsLike")) {
        auto prototype = (*collection.second)("distributeShardsLike").getString();
        if (!prototype.empty()) {
          std::string u;
          try {
            u = resolve(database.first, prototype);
          } catch (...) {}
          if (u != prototype) {
            { VPackArrayBuilder trx(&migrate);
              { VPackObjectBuilder oper(&migrate);
                migrate.add(
                  planColPrefix + database.first + "/" + collection.first + "/" +
                  "distributeShardsLike", VPackValue(u)); }
              { VPackObjectBuilder prec(&migrate);
                migrate.add(
                  planColPrefix + database.first + "/" + collection.first + "/" +
                  "distributeShardsLike", VPackValue(prototype)); }
            }
          }
        }
      }
    }
  }
}

// Shrink cluster if applicable, guarded by caller
void Supervision::shrinkCluster() {
  _lock.assertLockedByCurrentThread();

  auto const& todo = _snapshot(toDoPrefix).children();
  auto const& pending = _snapshot(pendingPrefix).children();

  if (!todo.empty() || !pending.empty()) { // This is low priority
    return;
  }

  // Get servers from plan
  auto availServers = Job::availableServers(_snapshot);

  size_t targetNumDBServers;
  std::string const NDBServers ("/Target/NumberOfDBServers");

  if (_snapshot.has(NDBServers) && _snapshot(NDBServers).isUInt()) {
    targetNumDBServers = _snapshot(NDBServers).getUInt();
  } else {
    LOG_TOPIC(TRACE, Logger::SUPERVISION)
      << "Targeted number of DB servers not set yet";
    return;
  }

  // Only if number of servers in target is smaller than the available
  if (targetNumDBServers < availServers.size()) {
    // Minimum 1 DB server must remain
    if (availServers.size() == 1) {
      LOG_TOPIC(DEBUG, Logger::SUPERVISION)
        << "Only one db server left for operation";
      return;
    }

    /**
     * mop: TODO instead of using Plan/Collections we should watch out for
     * Plan/ReplicationFactor and Current...when the replicationFactor is not
     * fullfilled we should add a follower to the plan
     * When seeing more servers in Current than replicationFactor we should
     * remove a server.
     * RemoveServer then should be changed so that it really just kills a server
     * after a while...
     * this way we would have implemented changing the replicationFactor and
     * have an awesome new feature
     **/
    // Find greatest replication factor among all collections
    uint64_t maxReplFact = 1;
    auto const& databases = _snapshot(planColPrefix).children();
    for (auto const& database : databases) {
      for (auto const& collptr : database.second->children()) {
        auto const& node = *collptr.second;
        if (node.has("replicationFactor") &&
            node("replicationFactor").isUInt()) {
          auto replFact = node("replicationFactor").getUInt();
          if (replFact > maxReplFact) {
            maxReplFact = replFact;
          }
        } else {
          LOG_TOPIC(WARN, Logger::SUPERVISION)
            << "Cannot retrieve replication factor for collection "
            << collptr.first;
          return;
        }
      }
    }

    // mop: do not account any failedservers in this calculation..the ones
    // having
    // a state of failed still have data of interest to us! We wait indefinately
    // for them to recover or for the user to remove them
    if (maxReplFact < availServers.size()) {
      // Clean out as long as number of available servers is bigger
      // than maxReplFactor and bigger than targeted number of db servers
      if (availServers.size() > maxReplFact &&
          availServers.size() > targetNumDBServers) {
        // Sort servers by name
        std::sort(availServers.begin(), availServers.end());

        // Schedule last server for cleanout
        CleanOutServer(_snapshot, _agent, std::to_string(_jobId++),
                       "supervision", availServers.back()).run();
      }
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
  _frequency = _agent->config().supervisionFrequency();
  _gracePeriod = _agent->config().supervisionGracePeriod();
  return start();
}

static std::string const syncLatest = "/Sync/LatestID";

void Supervision::getUniqueIds() {
  _lock.assertLockedByCurrentThread();

  size_t n = 10000;

  std::string path = _agencyPrefix + "/Sync/LatestID";
  auto builder = std::make_shared<Builder>();
  { VPackArrayBuilder a(builder.get());
    { VPackArrayBuilder b(builder.get());
      { VPackObjectBuilder c(builder.get());
        {
          builder->add(VPackValue(path));
          VPackObjectBuilder b(builder.get());
          builder->add("op",VPackValue("increment"));
          builder->add("step",VPackValue(n));
        }
      }
    }
    { VPackArrayBuilder a(builder.get());
      builder->add(VPackValue(path)); }
  } // [[{path:{"op":"increment","step":n}}],[path]]

  auto ret = _agent->transact(builder);
  if (ret.accepted) {
    try {
      _jobIdMax = ret.result->slice()[1].get(
        std::vector<std::string>(
          {"arango", "Sync", "LatestID"})).getUInt();
      _jobId = _jobIdMax - n;
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::SUPERVISION)
        << "Failed to acquire job IDs from agency: "
        << e.what() << __FILE__ << " " << __LINE__;
    }
  }

}


void Supervision::beginShutdown() {
  // Personal hygiene
  Thread::beginShutdown();

  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}
