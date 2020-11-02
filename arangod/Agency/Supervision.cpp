////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "Agency/ActiveFailoverJob.h"
#include "Agency/AddFollower.h"
#include "Agency/Agent.h"
#include "Agency/CleanOutServer.h"
#include "Agency/FailedServer.h"
#include "Agency/Job.h"
#include "Agency/JobContext.h"
#include "Agency/RemoveFollower.h"
#include "Agency/Store.h"
#include "AgencyPaths.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Cluster/ServerState.h"
#include "Random/RandomGenerator.h"

using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::application_features;
using namespace arangodb::cluster::paths;
using namespace arangodb::cluster::paths::aliases;

struct HealthRecord {
  std::string shortName;
  std::string syncTime;
  std::string syncStatus;
  std::string status;
  std::string endpoint;
  std::string advertisedEndpoint;
  std::string lastAcked;
  std::string hostId;
  std::string serverVersion;
  std::string engine;
  size_t version;

  explicit HealthRecord() : version(0) {}

  HealthRecord(std::string const& sn, std::string const& ep, std::string const& ho,
               std::string const& en, std::string const& sv, std::string const& ae)
      : shortName(sn),
        endpoint(ep),
        advertisedEndpoint(ae),
        hostId(ho),
        serverVersion(sv),
        engine(en),
        version(0) {}

  explicit HealthRecord(Node const& node) { *this = node; }

  HealthRecord& operator=(Node const& node) {
    version = 0;
    if (shortName.empty()) {
      shortName = node.hasAsString("ShortName").first;
    }
    if (endpoint.empty()) {
      endpoint = node.hasAsString("Endpoint").first;
    }
    if (hostId.empty()) {
      hostId = node.hasAsString("Host").first;
    }
    if (node.has("Status")) {
      status = node.hasAsString("Status").first;
      if (node.has("SyncStatus")) {  // New format
        version = 2;
        syncStatus = node.hasAsString("SyncStatus").first;
        if (node.has("SyncTime")) {
          syncTime = node.hasAsString("SyncTime").first;
        }
        if (node.has("LastAckedTime")) {
          lastAcked = node.hasAsString("LastAckedTime").first;
        }
        if (node.has("AdvertisedEndpoint")) {
          version = 3;
          advertisedEndpoint = node.hasAsString("AdvertisedEndpoint").first;
        } else {
          advertisedEndpoint.clear();
        }
        if (node.has("Engine") && node.has("Version")) {
          version = 4;
          engine = node.hasAsString("Engine").first;
          serverVersion = node.hasAsString("Version").first;
        } else {
          engine.clear();
          serverVersion.clear();
        }
      } else if (node.has("LastHeartbeatStatus")) {
        version = 1;
        syncStatus = node.hasAsString("LastHeartbeatStatus").first;
        if (node.has("LastHeartbeatSent")) {
          syncTime = node.hasAsString("LastHeartbeatSent").first;
        }
        if (node.has("LastHeartbeatAcked")) {
          lastAcked = node.hasAsString("LastHeartbeatAcked").first;
        }
      }
    }
    return *this;
  }

  void toVelocyPack(VPackBuilder& obj) const {
    TRI_ASSERT(obj.isOpenObject());
    obj.add("ShortName", VPackValue(shortName));
    obj.add("Endpoint", VPackValue(endpoint));
    obj.add("Host", VPackValue(hostId));
    obj.add("SyncStatus", VPackValue(syncStatus));
    obj.add("Status", VPackValue(status));
    obj.add("Version", VPackValue(serverVersion));
    obj.add("Engine", VPackValue(engine));
    if (!advertisedEndpoint.empty()) {
      obj.add("AdvertisedEndpoint", VPackValue(advertisedEndpoint));
    }
    obj.add("Timestamp", VPackValue(timepointToString(std::chrono::system_clock::now())));
    obj.add("SyncTime", VPackValue(syncTime));
    obj.add("LastAckedTime", VPackValue(lastAcked));
  }

  bool statusDiff(HealthRecord const& other) {
    return status != other.status || syncStatus != other.syncStatus ||
           advertisedEndpoint != other.advertisedEndpoint ||
           serverVersion != other.serverVersion || engine != other.engine ||
           hostId != other.hostId || endpoint != other.endpoint;
  }

  friend std::ostream& operator<<(std::ostream& o, HealthRecord const& hr) {
    VPackBuilder builder;
    {
      VPackObjectBuilder b(&builder);
      hr.toVelocyPack(builder);
    }
    o << builder.toJson();
    return o;
  }
};

// This is initialized in AgencyFeature:
std::string Supervision::_agencyPrefix = "/arango";

Supervision::Supervision(application_features::ApplicationServer& server)
    : arangodb::CriticalThread(server, "Supervision"),
      _agent(nullptr),
      _spearhead(server, _agent),
      _snapshot(nullptr),
      _transient("Transient"),
      _frequency(1.),
      _gracePeriod(10.),
      _okThreshold(5.),
      _jobId(0),
      _jobIdMax(0),
      _lastUpdateIndex(0),
      _haveAborts(false),
      _selfShutdown(false),
      _upgraded(false),
      _nextServerCleanup(),
      _supervision_runtime_msec(server.getFeature<arangodb::MetricsFeature>().histogram(
          StaticStrings::SupervisionRuntimeMs, log_scale_t<uint64_t>(2, 50, 8000, 10),
          "Agency Supervision runtime histogram [ms]")),
      _supervision_runtime_wait_for_sync_msec(
          server.getFeature<arangodb::MetricsFeature>().histogram(
              StaticStrings::SupervisionRuntimeWaitForSyncMs,
              log_scale_t<uint64_t>(2, 10, 2000, 10),
              "Agency Supervision wait for replication time [ms]")),
      _supervision_accum_runtime_msec(
          server.getFeature<arangodb::MetricsFeature>().counter(
              StaticStrings::SupervisionAccumRuntimeMs, 0,
              "Accumulated Supervision Runtime [ms]")),
      _supervision_accum_runtime_wait_for_sync_msec(
          server.getFeature<arangodb::MetricsFeature>().counter(
              StaticStrings::SupervisionAccumRuntimeWaitForSyncMs, 0,
              "Accumulated Supervision  wait for replication time  [ms]")),
      _supervision_failed_server_counter(
          server.getFeature<arangodb::MetricsFeature>().counter(
              StaticStrings::SupervisionFailedServerCount, 0,
              "Counter for FailedServer jobs")) {}

Supervision::~Supervision() {
  if (!isStopping()) {
    shutdown();
  }
}

static std::string const syncPrefix = "/Sync/ServerStates/";
static std::string const supervisionPrefix = "/Supervision";
static std::string const supervisionMaintenance = "/Supervision/Maintenance";
static std::string const healthPrefix = "/Supervision/Health/";
static std::string const targetShortID = "/Target/MapUniqueToShortID/";
static std::string const currentServersRegisteredPrefix =
    "/Current/ServersRegistered";
static std::string const foxxmaster = "/Current/Foxxmaster";

void Supervision::upgradeOne(Builder& builder) {
  _lock.assertLockedByCurrentThread();
  // "/arango/Agency/Definition" not exists or is 0
  if (!snapshot().has("Agency/Definition")) {
    {
      VPackArrayBuilder trx(&builder);
      {
        VPackObjectBuilder oper(&builder);
        builder.add("/Agency/Definition", VPackValue(1));
        builder.add(VPackValue("/Target/ToDo"));
        { VPackObjectBuilder empty(&builder); }
        builder.add(VPackValue("/Target/Pending"));
        { VPackObjectBuilder empty(&builder); }
      }
      {
        VPackObjectBuilder o(&builder);
        builder.add(VPackValue("/Agency/Definition"));
        {
          VPackObjectBuilder prec(&builder);
          builder.add("oldEmpty", VPackValue(true));
        }
      }
    }
  }
}

void Supervision::upgradeZero(Builder& builder) {
  _lock.assertLockedByCurrentThread();
  // "/arango/Target/FailedServers" is still an array
  Slice fails = snapshot().hasAsSlice(failedServersPrefix).first;
  if (fails.isArray()) {
    {
      VPackArrayBuilder trx(&builder);
      {
        VPackObjectBuilder o(&builder);
        builder.add(VPackValue(failedServersPrefix));
        {
          VPackObjectBuilder oo(&builder);
          if (fails.length() > 0) {
            for (VPackSlice fail : VPackArrayIterator(fails)) {
              builder.add(VPackValue(fail.copyString()));
              { VPackArrayBuilder ooo(&builder); }
            }
          }
        }
      }
    }  // trx
  }
}

void Supervision::upgradeHealthRecords(Builder& builder) {
  _lock.assertLockedByCurrentThread();
  // "/arango/Supervision/health" is in old format
  Builder b;
  size_t n = 0;

  if (snapshot().has(healthPrefix)) {
    HealthRecord hr;
    {
      VPackObjectBuilder oo(&b);
      for (auto const& recPair : snapshot().hasAsChildren(healthPrefix).first) {
        if (recPair.second->has("ShortName") &&
            recPair.second->has("Endpoint")) {
          hr = *recPair.second;
          if (hr.version == 1) {
            ++n;
            b.add(VPackValue(recPair.first));
            {
              VPackObjectBuilder ooo(&b);
              hr.toVelocyPack(b);
            }
          }
        }
      }
    }
  }

  if (n > 0) {
    {
      VPackArrayBuilder trx(&builder);
      {
        VPackObjectBuilder o(&builder);
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
    upgradeMaintenance(builder);
    upgradeBackupKey(builder);
  }

  LOG_TOPIC("f7315", DEBUG, Logger::AGENCY)
      << "Upgrading the agency:" << builder.toJson();

  if (builder.slice().length() > 0) {
    generalTransaction(_agent, builder);
  }

  _upgraded = true;
}

void Supervision::upgradeMaintenance(VPackBuilder& builder) {
  _lock.assertLockedByCurrentThread();
  if (snapshot().has(supervisionMaintenance)) {
    std::string maintenanceState;
    try {
      maintenanceState = snapshot().get(supervisionMaintenance).getString();
    } catch (std::exception const& e) {
      LOG_TOPIC("cf235", ERR, Logger::SUPERVISION)
          << "Supervision maintenance key in agency is not a string. This "
             "should never happen and will prevent hot backups. "
          << e.what();
      return;
    }

    if (maintenanceState == "on") {
      VPackArrayBuilder trx(&builder);
      {
        VPackObjectBuilder o(&builder);
        builder.add(supervisionMaintenance,
                    VPackValue(timepointToString(std::chrono::system_clock::now() +
                                                 std::chrono::hours(1))));
      }
      {
        VPackObjectBuilder o(&builder);
        builder.add(supervisionMaintenance, VPackValue(maintenanceState));
      }
    }
  }
}

void Supervision::upgradeBackupKey(VPackBuilder& builder) {
  // Upgrade /arango/Target/HotBackup/Create from 0 to time out

  _lock.assertLockedByCurrentThread();
  if (snapshot().has(HOTBACKUP_KEY)) {
    Node const& tmp = snapshot()(HOTBACKUP_KEY);
    if (tmp.isNumber()) {
      if (tmp.getInt() == 0) {
        VPackArrayBuilder trx(&builder);
        {
          VPackObjectBuilder o(&builder);
          builder.add(HOTBACKUP_KEY,
                      VPackValue(timepointToString(std::chrono::system_clock::now() +
                                                   std::chrono::hours(1))));
        }
        {
          VPackObjectBuilder o(&builder);
          builder.add(HOTBACKUP_KEY, VPackValue(0));
        }
      }
    }
  }
}

void handleOnStatusDBServer(Agent* agent, Node const& snapshot,
                            HealthRecord& persisted, HealthRecord& transisted,
                            std::string const& serverID, uint64_t const& jobId,
                            std::shared_ptr<VPackBuilder>& envelope) {
  std::string failedServerPath = failedServersPrefix + "/" + serverID;
  // New condition GOOD:
  if (transisted.status == Supervision::HEALTH_STATUS_GOOD) {
    if (snapshot.has(failedServerPath)) {
      envelope = std::make_shared<VPackBuilder>();
      {
        VPackArrayBuilder a(envelope.get());
        {
          VPackObjectBuilder operations(envelope.get());
          envelope->add(VPackValue(failedServerPath));
          {
            VPackObjectBuilder ccc(envelope.get());
            envelope->add("op", VPackValue("delete"));
          }
        }
      }
    }
  } else if (  // New state: FAILED persisted: GOOD (-> BAD)
      persisted.status == Supervision::HEALTH_STATUS_GOOD &&
      transisted.status != Supervision::HEALTH_STATUS_GOOD) {
    transisted.status = Supervision::HEALTH_STATUS_BAD;
  } else if (  // New state: FAILED persisted: BAD (-> Job)
      persisted.status == Supervision::HEALTH_STATUS_BAD &&
      transisted.status == Supervision::HEALTH_STATUS_FAILED) {
    if (!snapshot.has(failedServerPath)) {
      envelope = std::make_shared<VPackBuilder>();
      agent->supervision()._supervision_failed_server_counter.operator++();
      FailedServer(snapshot, agent, std::to_string(jobId), "supervision", serverID)
          .create(envelope);
    }
  }
}

void handleOnStatusCoordinator(Agent* agent, Node const& snapshot, HealthRecord& persisted,
                               HealthRecord& transisted, std::string const& serverID) {
  if (transisted.status == Supervision::HEALTH_STATUS_FAILED) {
    VPackBuilder create;
    {
      VPackArrayBuilder tx(&create);
      {
        VPackObjectBuilder b(&create);
        // unconditionally increase reboot id and plan version
        Job::addIncreaseRebootId(create, serverID);

        // if the current foxxmaster server failed => reset the value to ""
        if (snapshot.hasAsString(foxxmaster).first == serverID) {
          create.add(foxxmaster, VPackValue(""));
        }
      }
    }
    singleWriteTransaction(agent, create, false);
  }
}

void handleOnStatusSingle(Agent* agent, Node const& snapshot, HealthRecord& persisted,
                          HealthRecord& transisted, std::string const& serverID,
                          uint64_t const& jobId, std::shared_ptr<VPackBuilder>& envelope) {
  std::string failedServerPath = failedServersPrefix + "/" + serverID;
  // New condition GOOD:
  if (transisted.status == Supervision::HEALTH_STATUS_GOOD) {
    if (snapshot.has(failedServerPath)) {
      envelope = std::make_shared<VPackBuilder>();
      {
        VPackArrayBuilder a(envelope.get());
        {
          VPackObjectBuilder operations(envelope.get());
          envelope->add(VPackValue(failedServerPath));
          {
            VPackObjectBuilder ccc(envelope.get());
            envelope->add("op", VPackValue("delete"));
          }
        }
      }
    }
  } else if (  // New state: FAILED persisted: GOOD (-> BAD)
      persisted.status == Supervision::HEALTH_STATUS_GOOD &&
      transisted.status != Supervision::HEALTH_STATUS_GOOD) {
    transisted.status = Supervision::HEALTH_STATUS_BAD;
  } else if (  // New state: FAILED persisted: BAD (-> Job)
      persisted.status == Supervision::HEALTH_STATUS_BAD &&
      transisted.status == Supervision::HEALTH_STATUS_FAILED) {
    if (!snapshot.has(failedServerPath)) {
      envelope = std::make_shared<VPackBuilder>();
      ActiveFailoverJob(snapshot, agent, std::to_string(jobId), "supervision", serverID)
          .create(envelope);
    }
  }
}

void handleOnStatus(Agent* agent, Node const& snapshot, HealthRecord& persisted,
                    HealthRecord& transisted, std::string const& serverID,
                    uint64_t const& jobId, std::shared_ptr<VPackBuilder>& envelope) {
  if (serverID.compare(0, 4, "PRMR") == 0) {
    handleOnStatusDBServer(agent, snapshot, persisted, transisted, serverID, jobId, envelope);
  } else if (serverID.compare(0, 4, "CRDN") == 0) {
    handleOnStatusCoordinator(agent, snapshot, persisted, transisted, serverID);
  } else if (serverID.compare(0, 4, "SNGL") == 0) {
    handleOnStatusSingle(agent, snapshot, persisted, transisted, serverID, jobId, envelope);
  } else {
    LOG_TOPIC("86191", ERR, Logger::SUPERVISION)
        << "Unknown server type. No supervision action taken. " << serverID;
  }
}

// Build transaction for removing unattended servers from health monitoring
query_t arangodb::consensus::removeTransactionBuilder(std::vector<std::string> const& todelete) {
  query_t del = std::make_shared<Builder>();
  {
    VPackArrayBuilder trxs(del.get());
    {
      VPackArrayBuilder trx(del.get());
      {
        VPackObjectBuilder server(del.get());
        for (auto const& srv : todelete) {
          del->add(VPackValue(Supervision::agencyPrefix() +
                              arangodb::consensus::healthPrefix + srv));
          {
            VPackObjectBuilder oper(del.get());
            del->add("op", VPackValue("delete"));
          }
        }
      }
    }
  }
  return del;
}

// Check all DB servers, guarded above doChecks
std::vector<check_t> Supervision::check(std::string const& type) {
  // Dead lock detection
  _lock.assertLockedByCurrentThread();

  // Book keeping
  std::vector<check_t> ret;
  auto const& machinesPlanned =
      snapshot().hasAsChildren(std::string("Plan/") + type).first;
  auto const& serversRegistered =
      snapshot().hasAsNode(currentServersRegisteredPrefix).first;
  std::vector<std::string> todelete;
  for (auto const& machine : snapshot().hasAsChildren(healthPrefix).first) {
    if ((type == "DBServers" && machine.first.compare(0, 4, "PRMR") == 0) ||
        (type == "Coordinators" && machine.first.compare(0, 4, "CRDN") == 0) ||
        (type == "Singles" && machine.first.compare(0, 4, "SNGL") == 0)) {
      // Put only those on list which are no longer planned:
      if (machinesPlanned.find(machine.first) == machinesPlanned.end()) {
        todelete.push_back(machine.first);
      }
    }
  }

  if (!todelete.empty()) {
    _agent->write(removeTransactionBuilder(todelete));
  }

  auto startTimeLoop = std::chrono::system_clock::now();

  // Do actual monitoring
  for (auto const& machine : machinesPlanned) {
    LOG_TOPIC("44252", TRACE, Logger::SUPERVISION)
        << "Checking health of server " << machine.first << " ...";
    std::string lastHeartbeatStatus, lastHeartbeatAcked, lastHeartbeatTime,
        lastStatus, serverID(machine.first), shortName;

    // short name arrives asynchronous to machine registering, make sure
    //  it has arrived before trying to use it
    auto tmp_shortName =
        snapshot().hasAsString(targetShortID + serverID + "/ShortName");
    if (tmp_shortName.second) {
      shortName = tmp_shortName.first;

      // "/arango/Current/ServersRegistered/<server-id>/endpoint"
      std::string endpoint;
      std::string epPath = serverID + "/endpoint";
      if (serversRegistered.has(epPath)) {
        endpoint = serversRegistered.hasAsString(epPath).first;
      }
      // "/arango/Current/ServersRegistered/<server-id>/host"
      std::string hostId;
      std::string hoPath = serverID + "/host";
      if (serversRegistered.has(hoPath)) {
        hostId = serversRegistered.hasAsString(hoPath).first;
      }
      // "/arango/Current/ServersRegistered/<server-id>/serverVersion"
      std::string serverVersion;
      std::string svPath = serverID + "/versionString";
      if (serversRegistered.has(svPath)) {
        serverVersion = serversRegistered.hasAsString(svPath).first;
      }
      // "/arango/Current/ServersRegistered/<server-id>/engine"
      std::string engine;
      std::string enPath = serverID + "/engine";
      if (serversRegistered.has(enPath)) {
        engine = serversRegistered.hasAsString(enPath).first;
      }
      //"/arango/Current/<serverId>/externalEndpoint"
      std::string externalEndpoint;
      std::string extEndPath = serverID + "/advertisedEndpoint";
      if (serversRegistered.has(extEndPath)) {
        externalEndpoint = serversRegistered.hasAsString(extEndPath).first;
      }

      // Health records from persistence, from transience and a new one
      HealthRecord transist(shortName, endpoint, hostId, engine, serverVersion, externalEndpoint);
      HealthRecord persist(shortName, endpoint, hostId, engine, serverVersion, externalEndpoint);

      // Get last health entries from transient and persistent key value stores
      bool transientHealthRecordFound = true;
      if (_transient.has(healthPrefix + serverID)) {
        transist = _transient.hasAsNode(healthPrefix + serverID).first;
      } else {
        // In this case this is the first time we look at this server during our
        // new leadership. So we do not touch the persisted health record and
        // only create a new transient health record.
        transientHealthRecordFound = false;
      }
      if (snapshot().has(healthPrefix + serverID)) {
        persist = snapshot().hasAsNode(healthPrefix + serverID).first;
      }

      // Here is an important subtlety: We will derive the health status of this
      // server a bit further down by looking at the time when we saw the last
      // heartbeat. The heartbeat is stored in transient in `Sync/ServerStates`.
      // It has a timestamp, however, this was taken on the other server, so we
      // cannot trust this time stamp because of possible clock skew. Since we
      // do not actually know when this heartbeat came in, we have to proceed as
      // follows: We make a copy of the `syncTime` and store it into the health
      // record in transient `Supervision/Health`. If we detect a difference, it
      // is the first time we have seen the new heartbeat and we can then take
      // a reading of our local system clock and use that time. However, if we
      // do not yet have a previous reading in our transient health record,
      // we must not touch the persisted health record at all. This is what the
      // flag `transientHealthRecordFound` means.

      // New health record (start with old add current information from sync)
      // Sync.time is copied to Health.syncTime
      // Sync.status is copied to Health.syncStatus
      std::string syncTime;
      std::string syncStatus;
      bool heartBeatVisible = _transient.has(syncPrefix + serverID);
      if (heartBeatVisible) {
        syncTime = _transient.hasAsString(syncPrefix + serverID + "/time").first;
        syncStatus = _transient.hasAsString(syncPrefix + serverID + "/status").first;
      } else {
        syncTime = timepointToString(std::chrono::system_clock::time_point()); // beginning of time
        syncStatus = "UNKNOWN";
      }

      // Compute the time when we last discovered a new heartbeat from that server:
      std::chrono::system_clock::time_point lastAckedTime;
      if (heartBeatVisible) {
        if (transientHealthRecordFound) {
          lastAckedTime = (syncTime != transist.syncTime)
                          ? startTimeLoop
                          : stringToTimepoint(transist.lastAcked);
        } else {
          // in this case we do no really know when this heartbeat came in,
          // however, it must have been after we became a leader, and since we
          // do not have a transient health record yet, we just assume that we
          // got it recently and set it to "now". Note that this will not make
          // a FAILED server "GOOD" again, since we do not touch the persisted
          // health record if we do not have a transient health record yet.
          lastAckedTime = startTimeLoop;
        }
      } else {
        lastAckedTime = std::chrono::system_clock::time_point();
      }
      transist.lastAcked = timepointToString(lastAckedTime);
      transist.syncTime = syncTime;
      transist.syncStatus = syncStatus;

      // update volatile values that may change
      transist.advertisedEndpoint = externalEndpoint;
      transist.serverVersion = serverVersion;
      transist.engine = engine;
      transist.hostId = hostId;
      transist.endpoint = endpoint;

      // We have now computed a new transient health record under all circumstances.

      bool changed;
      if (transientHealthRecordFound) {
        // Calculate elapsed since lastAcked
        auto elapsed = std::chrono::duration<double>(startTimeLoop - lastAckedTime);

        if (elapsed.count() <= _okThreshold) {
          transist.status = Supervision::HEALTH_STATUS_GOOD;
        } else if (elapsed.count() <= _gracePeriod) {
          transist.status = Supervision::HEALTH_STATUS_BAD;
        } else {
          transist.status = Supervision::HEALTH_STATUS_FAILED;
        }

        // Status changed?
        changed = transist.statusDiff(persist);
      } else {
        transist.status = persist.status;
        changed = false;
      }

      // Take necessary actions if any
      std::shared_ptr<VPackBuilder> envelope;
      if (changed) {
        LOG_TOPIC("bbbde", DEBUG, Logger::SUPERVISION)
            << "Status of server " << serverID << " has changed from "
            << persist.status << " to " << transist.status;
        handleOnStatus(_agent, snapshot(), persist, transist, serverID, _jobId, envelope);
        persist = transist;  // Now copy Status, SyncStatus from transient to persited
      } else {
        LOG_TOPIC("44253", TRACE, Logger::SUPERVISION)
            << "Health of server " << machine.first << " remains " << transist.status;
      }

      // Transient report
      std::shared_ptr<Builder> tReport = std::make_shared<Builder>();
      {
        VPackArrayBuilder transaction(tReport.get());  // Transist Transaction
        std::shared_ptr<VPackBuilder> envelope;
        {
          VPackObjectBuilder operation(tReport.get());        // Operation
          tReport->add(VPackValue(healthPrefix + serverID));  // Supervision/Health
          {
            VPackObjectBuilder oo(tReport.get());
            transist.toVelocyPack(*tReport);
          }
        }
      }  // Transaction

      // Persistent report
      std::shared_ptr<Builder> pReport = nullptr;
      if (changed) {
        pReport = std::make_shared<Builder>();
        {
          VPackArrayBuilder transaction(pReport.get());  // Persist Transaction
          {
            VPackObjectBuilder operation(pReport.get());        // Operation
            pReport->add(VPackValue(healthPrefix + serverID));  // Supervision/Health
            {
              VPackObjectBuilder oo(pReport.get());
              persist.toVelocyPack(*pReport);
            }
            if (envelope != nullptr) {  // Failed server
              TRI_ASSERT(envelope->slice().isArray() && envelope->slice()[0].isObject());
              for (VPackObjectIterator::ObjectPair i :
                   VPackObjectIterator(envelope->slice()[0])) {
                pReport->add(i.key.copyString(), i.value);
              }
            }
          }  // Operation
          if (envelope != nullptr && envelope->slice().length() > 1) {  // Preconditions(Job)
            TRI_ASSERT(envelope->slice().isArray() && envelope->slice()[1].isObject());
            pReport->add(envelope->slice()[1]);
          }
        }  // Transaction
      }

      if (!this->isStopping()) {
        // Replicate special event and only then transient store
        if (changed) {
          write_ret_t res = singleWriteTransaction(_agent, *pReport, false);
          if (res.accepted && res.indices.front() != 0) {
            ++_jobId;  // Job was booked
            transient(_agent, *tReport);
          }
        } else {  // Nothing special just transient store
          transient(_agent, *tReport);
        }
      }
    } else {
      LOG_TOPIC("a55cd", INFO, Logger::SUPERVISION)
          << "Short name for " << serverID
          << " not yet available.  Skipping health check.";
    }  // else

  }  // for

  return ret;
}

bool Supervision::earlyBird() const {
  std::vector<std::string> tpath{"Sync", "ServerStates"};
  std::vector<std::string> pdbpath{"Plan", "DBServers"};
  std::vector<std::string> pcpath{"Plan", "Coordinators"};

  if (!snapshot().has(pdbpath)) {
    LOG_TOPIC("3206f", DEBUG, Logger::SUPERVISION)
        << "No Plan/DBServers key in persistent store";
    return false;
  }
  VPackBuilder dbserversB = snapshot()(pdbpath).toBuilder();
  VPackSlice dbservers = dbserversB.slice();

  if (!snapshot().has(pcpath)) {
    LOG_TOPIC("b0e08", DEBUG, Logger::SUPERVISION)
        << "No Plan/Coordinators key in persistent store";
    return false;
  }
  VPackBuilder coordinatorsB = snapshot()(pcpath).toBuilder();
  VPackSlice coordinators = coordinatorsB.slice();

  if (!_transient.has(tpath)) {
    LOG_TOPIC("fe42a", DEBUG, Logger::SUPERVISION)
        << "No Sync/ServerStates key in transient store";
    return false;
  }
  VPackBuilder serverStatesB = _transient(tpath).toBuilder();
  VPackSlice serverStates = serverStatesB.slice();

  // every db server in plan accounted for in transient store?
  for (auto const& server : VPackObjectIterator(dbservers)) {
    auto serverId = server.key.copyString();
    if (!serverStates.hasKey(serverId)) {
      return false;
    }
  }
  // every db server in plan accounted for in transient store?
  for (auto const& server : VPackObjectIterator(coordinators)) {
    auto serverId = server.key.copyString();
    if (!serverStates.hasKey(serverId)) {
      return false;
    }
  }

  return true;
}

// Update local agency snapshot, guarded by callers
bool Supervision::updateSnapshot() {
  _lock.assertLockedByCurrentThread();

  if (_agent == nullptr || this->isStopping()) {
    return false;
  }

  // ********************************** WARNING ********************************
  // Only change with utmost care. This is to be the sole location for modifying
  // _snapshot. All places, which need access to the _snapshot variable must use
  // the snapshot() member to avoid accessing a null pointer!
  // Furthermore, _snapshot must never be changed without considering its
  // consequences for _lastconfirmed!

  // Update once from agency's spearhead and keep updating using RAFT log from
  // there.
  if (_lastUpdateIndex == 0) {
    _agent->executeLockedRead([&]() {
      if (_agent->spearhead().has(_agencyPrefix)) {
        _spearhead = _agent->spearhead();
        if (_spearhead.has(_agencyPrefix)) {
          _lastUpdateIndex = _agent->confirmed();
          _snapshot = _spearhead.nodePtr(_agencyPrefix);
        } else {
          _lastUpdateIndex = 0;
          _snapshot = _spearhead.nodePtr();
        }
      }
    });
  } else {
    std::vector<log_t> logs;
    _agent->executeLockedRead(
        [&]() { logs = _agent->logs(_lastUpdateIndex + 1); });
    if (!logs.empty() && !(logs.size() == 1 && _lastUpdateIndex == logs.front().index)) {
      _lastUpdateIndex = _spearhead.applyTransactions(logs);
      _snapshot = _spearhead.nodePtr(_agencyPrefix);
    }
  }
  // ***************************************************************************

  _agent->executeTransientLocked([&]() {
    if (_agent->transient().has(_agencyPrefix)) {
      _transient = _agent->transient().get(_agencyPrefix);
    }
  });

  return true;
}

// All checks, guarded by main thread
bool Supervision::doChecks() {
  _lock.assertLockedByCurrentThread();
  TRI_ASSERT(ServerState::roleToAgencyListKey(ServerState::ROLE_DBSERVER) ==
             "DBServers");
  LOG_TOPIC("aadea", DEBUG, Logger::SUPERVISION) << "Checking dbservers...";
  check(ServerState::roleToAgencyListKey(ServerState::ROLE_DBSERVER));
  TRI_ASSERT(ServerState::roleToAgencyListKey(ServerState::ROLE_COORDINATOR) ==
             "Coordinators");
  LOG_TOPIC("aadeb", DEBUG, Logger::SUPERVISION) << "Checking coordinators...";
  check(ServerState::roleToAgencyListKey(ServerState::ROLE_COORDINATOR));
  TRI_ASSERT(ServerState::roleToAgencyListKey(ServerState::ROLE_SINGLE) ==
             "Singles");
  LOG_TOPIC("aadec", DEBUG, Logger::SUPERVISION)
      << "Checking single servers (active failover)...";
  check(ServerState::roleToAgencyListKey(ServerState::ROLE_SINGLE));
  LOG_TOPIC("aaded", DEBUG, Logger::SUPERVISION) << "Server checks done.";

  return true;
}

void Supervision::reportStatus(std::string const& status) {
  bool persist = false;
  query_t report;

  {  // Do I have to report to agency under
    _lock.assertLockedByCurrentThread();
    if (snapshot().hasAsString("/Supervision/State/Mode").first != status) {
      // This includes the case that the mode is not set, since status
      // is never empty
      persist = true;
    }
  }

  report = std::make_shared<VPackBuilder>();
  {
    VPackArrayBuilder trx(report.get());
    {
      VPackObjectBuilder br(report.get());
      report->add(VPackValue("/Supervision/State"));
      {
        VPackObjectBuilder bbr(report.get());
        report->add("Mode", VPackValue(status));
        report->add("Timestamp",
                    VPackValue(timepointToString(std::chrono::system_clock::now())));
      }
    }
  }

  // Importatnt! No reporting in transient for Maintenance mode.
  if (status != "Maintenance") {
    transient(_agent, *report);
  }

  if (persist) {
    write_ret_t res = singleWriteTransaction(_agent, *report, false);
  }
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
          auto const sn = _agent->readDB().get(supervisionNode);
          if (sn.children().size() > 0) {
            done = true;
          }
        } catch (...) {
          LOG_TOPIC("4bc80", WARN, Logger::SUPERVISION)
              << "Main node in agency gone. Contact your db administrator.";
        }
      }
    });

    if (done) {
      break;
    }

    LOG_TOPIC("9a79b", DEBUG, Logger::SUPERVISION) << "Waiting for ArangoDB to "
                                                      "initialize its data.";
  }

  bool shutdown = false;
  {
    CONDITION_LOCKER(guard, _cv);
    TRI_ASSERT(_agent != nullptr);

    while (!this->isStopping()) {
      try {
        auto lapStart = std::chrono::steady_clock::now();

        {
          MUTEX_LOCKER(locker, _lock);

          if (_snapshot != nullptr && isShuttingDown()) {
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
            LOG_TOPIC("edeee", TRACE, Logger::SUPERVISION)
              << "Begin updateSnapshot";
            updateSnapshot();
            LOG_TOPIC("aaabb", TRACE, Logger::SUPERVISION)
              << "Finished updateSnapshot";

            if (!_upgraded) {
              upgradeAgency();
            }

            bool maintenanceMode = false;
            if (snapshot().has(supervisionMaintenance)) {
              try {
                if (snapshot().get(supervisionMaintenance).isString()) {
                  std::string tmp = snapshot().get(supervisionMaintenance).getString();
                  if (tmp.size() < 18) {  // legacy behaviour
                    maintenanceMode = true;
                  } else {
                    auto const maintenanceExpires = stringToTimepoint(tmp);
                    if (maintenanceExpires >= std::chrono::system_clock::now()) {
                      maintenanceMode = true;
                    }
                  }
                } else {  // legacy behaviour
                  maintenanceMode = true;
                }
              } catch (std::exception const& e) {
                LOG_TOPIC("cf236", ERR, Logger::SUPERVISION)
                  << "Supervision maintenace key in agency is not a string. "
                  "This should never happen and will prevent hot backups. "
                  << e.what();
                return;
              }
            }

            if (!maintenanceMode) {
              reportStatus("Normal");

              _haveAborts = false;

              if (_agent->leaderFor() > 55 || earlyBird()) {
                // 55 seconds is less than a minute, which fits to the
                // 60 seconds timeout in /_admin/cluster/health

                // wait 5 min or until next scheduled run
                if (_agent->leaderFor() > 300 &&
                    _nextServerCleanup < std::chrono::system_clock::now()) {
                  LOG_TOPIC("dcded", TRACE, Logger::SUPERVISION)
                    << "Begin cleanupExpiredServers";
                  cleanupExpiredServers(snapshot(), _transient);
                  LOG_TOPIC("dedcd", TRACE, Logger::SUPERVISION)
                    << "Finished cleanupExpiredServers";
                }

                try {
                  LOG_TOPIC("aa565", TRACE, Logger::SUPERVISION)
                    << "Begin doChecks";
                  doChecks();
                  LOG_TOPIC("675fc", TRACE, Logger::SUPERVISION)
                    << "Finished doChecks";
                } catch (std::exception const& e) {
                  LOG_TOPIC("e0869", ERR, Logger::SUPERVISION)
                    << e.what() << " " << __FILE__ << " " << __LINE__;
                } catch (...) {
                  LOG_TOPIC("ac4c4", ERR, Logger::SUPERVISION)
                    << "Supervision::doChecks() generated an uncaught "
                    "exception.";
                }
              } else {
                LOG_TOPIC("7928f", INFO, Logger::SUPERVISION)
                  << "Postponing supervision for now, waiting for incoming "
                  "heartbeats: "
                  << _agent->leaderFor();
              }
              try {
                LOG_TOPIC("7895a", TRACE, Logger::SUPERVISION)
                  << "Begin handleJobs";
                handleJobs();
                LOG_TOPIC("febbc", TRACE, Logger::SUPERVISION)
                  << "Finished handleJobs";
              } catch (std::exception const& e) {
                LOG_TOPIC("76123", WARN, Logger::SUPERVISION)
                  << "Caught exception in handleJobs(), error message: " << e.what();
              }
            } else {
              reportStatus("Maintenance");
            }
          } else {
            // Once we lose leadership, we need to restart building our snapshot
            if (_lastUpdateIndex > 0) {
              _lastUpdateIndex = 0;
            }
          }
        }

        // If anything was rafted, we need to wait until it is replicated,
        // otherwise it is not "committed" in the Raft sense. However, let's
        // only wait for our changes not for new ones coming in during the wait.
        if (_agent->leading()) {
          index_t leaderIndex = _agent->index();
          if (leaderIndex != 0) {
            auto wait_for_repl_start = std::chrono::steady_clock::now();

            while (!this->isStopping() && _agent->leading()) {
              auto result = _agent->waitFor(leaderIndex);
              if (result == Agent::raft_commit_t::TIMEOUT) {  // Oh snap
                // Note that we can get UNKNOWN if we have lost leadership or
                // if we are shutting down. In both cases we just leave the loop.
                LOG_TOPIC("c72b0", WARN, Logger::SUPERVISION)
                  << "Waiting for commits to be done ... ";
                continue;
              } else {  // Good we can continue
                break;
              }
            }

            auto wait_for_repl_end = std::chrono::steady_clock::now();
            auto repl_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                wait_for_repl_end - wait_for_repl_start)
              .count();
            _supervision_runtime_wait_for_sync_msec.count(repl_ms);
            _supervision_accum_runtime_wait_for_sync_msec.count(repl_ms);
          }
        }

        auto lapTime = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - lapStart)
          .count();

        _supervision_runtime_msec.count(lapTime / 1000);
        _supervision_accum_runtime_msec.count(lapTime / 1000);

        if (lapTime < 1000000) {
          _cv.wait(static_cast<uint64_t>((1000000 - lapTime) * _frequency));
        }
      } catch (std::exception const& ex) {
        LOG_TOPIC("f5af1", ERR, Logger::SUPERVISION) << "caught exception in supervision thread: " << ex.what();
        // continue
      } catch (...) {
        LOG_TOPIC("c82bb", ERR, Logger::SUPERVISION) << "caught unknown exception in supervision thread";
        // continue
      }
    }
  }

  if (shutdown) {
    _server.beginShutdown();
  }
}

// Guarded by caller
bool Supervision::isShuttingDown() {
  _lock.assertLockedByCurrentThread();
  if (_snapshot && snapshot().has("Shutdown")) {
    return snapshot().hasAsBool("Shutdown").first;
  }
  return false;
}

// Guarded by caller
std::string Supervision::serverHealth(std::string const& serverName) {
  _lock.assertLockedByCurrentThread();
  std::string const serverStatus(healthPrefix + serverName + "/Status");
  return (snapshot().has(serverStatus)) ? snapshot().hasAsString(serverStatus).first
                                        : std::string();
}

// Guarded by caller
void Supervision::handleShutdown() {
  _lock.assertLockedByCurrentThread();

  _selfShutdown = true;
  LOG_TOPIC("f1f68", DEBUG, Logger::SUPERVISION)
      << "Waiting for clients to shut down";
  auto const& serversRegistered =
      snapshot().hasAsChildren(currentServersRegisteredPrefix).first;
  bool serversCleared = true;
  for (auto const& server : serversRegistered) {
    if (server.first == "Version") {
      continue;
    }

    LOG_TOPIC("d212a", DEBUG, Logger::SUPERVISION)
        << "Waiting for " << server.first << " to shutdown";

    if (serverHealth(server.first) != HEALTH_STATUS_GOOD) {
      LOG_TOPIC("3db81", WARN, Logger::SUPERVISION)
          << "Server " << server.first << " did not shutdown properly it seems!";
      continue;
    }
    serversCleared = false;
  }

  if (serversCleared) {
    if (_agent->leading()) {
      auto del = std::make_shared<Builder>();
      {
        VPackArrayBuilder txs(del.get());
        {
          VPackArrayBuilder tx(del.get());
          {
            VPackObjectBuilder o(del.get());
            del->add(VPackValue(_agencyPrefix + "/Shutdown"));
            {
              VPackObjectBuilder oo(del.get());
              del->add("op", VPackValue("delete"));
            }
          }
        }
      }
      auto result = _agent->write(del);
      if (result.indices.size() != 1) {
        LOG_TOPIC("3bba4", ERR, Logger::SUPERVISION)
            << "Invalid resultsize of " << result.indices.size()
            << " found during shutdown";
      } else {
        if (!_agent->waitFor(result.indices.at(0))) {
          LOG_TOPIC("d98af", ERR, Logger::SUPERVISION)
              << "Result was not written to followers during shutdown";
        }
      }
    }
  }
}

struct ServerLists {
  std::unordered_map<ServerID, uint64_t> dbservers;
  std::unordered_map<ServerID, uint64_t> coordinators;
};

//  Get all planned servers
//  If heartbeat in transient too old or missing
//    If heartbeat in snapshot older than 1 day
//      Remove coordinator everywhere
//      Remove DB server everywhere, if not leader of a shard

std::unordered_map<ServerID, std::string> deletionCandidates(Node const& snapshot,
                                                             Node const& transient,
                                                             std::string const& type) {
  using namespace std::chrono;
  std::unordered_map<ServerID, std::string> serverList;
  std::string const planPath = "/Plan/" + type;

  if (snapshot.has(planPath) && !snapshot(planPath).children().empty()) {
    std::string persistedTimeStamp;

    for (auto const& serverId : snapshot(planPath).children()) {
      auto const& transientHeartbeat =
          transient.hasAsNode("/Supervision/Health/" + serverId.first);
      try {
        // Do we have a transient heartbeat younger than a day?
        if (transientHeartbeat.second) {
          auto const t =
              stringToTimepoint(transientHeartbeat.first("Timestamp").getString());
          if (t > system_clock::now() - hours(24)) {
            continue;
          }
        }
        // Else do we have a persistent heartbeat younger than a day?
        auto const& persistentHeartbeat =
            snapshot.hasAsNode("/Supervision/Health/" + serverId.first);
        if (persistentHeartbeat.second) {
          persistedTimeStamp = persistentHeartbeat.first("Timestamp").getString();
          auto const t = stringToTimepoint(persistedTimeStamp);
          if (t > system_clock::now() - hours(24)) {
            continue;
          }
        } else {
          if (!persistedTimeStamp.empty()) {
            persistedTimeStamp.clear();
          }
        }
      } catch (std::exception const& e) {
        LOG_TOPIC("21a9e", DEBUG, Logger::SUPERVISION)
            << "Failing to analyse " << serverId << " as deletion candidate "
            << e.what();
      }

      // We are still here?
      serverList.emplace(serverId.first, persistedTimeStamp);
    }
  }

  // Clear shard DB servers from the deletion candidates
  if (type == "DBServers") {
    if (!serverList.empty()) {  // we need to go through all shard leaders :(
      for (auto const& database : snapshot("Plan/Collections").children()) {
        for (auto const& collection : database.second->children()) {
          for (auto const& shard : (*collection.second)("shards").children()) {
            Slice const servers = (*shard.second).getArray();
            if (servers.length() > 0) {
              try {
                for (auto const& server : VPackArrayIterator(servers)) {
                  if (serverList.find(server.copyString()) != serverList.end()) {
                    serverList.erase(server.copyString());
                  }
                }
              } catch (std::exception const& e) {
                // TODO: this needs a little attention
                LOG_TOPIC("720a5", DEBUG, Logger::SUPERVISION) << e.what();
              }
            } else {
              return serverList;
            }
          }
        }
      }
    }
  }
  return serverList;
}

void Supervision::cleanupExpiredServers(Node const& snapshot, Node const& transient) {
  auto servers = deletionCandidates(snapshot, transient, "DBServers");
  auto const& currentDatabases = snapshot("Current/Databases");

  VPackBuilder del;
  {
    VPackObjectBuilder d(&del);
    del.add("op", VPackValue("delete"));
  }

  auto envelope = std::make_shared<VPackBuilder>();
  VPackBuilder& trxs = *envelope;  // Transactions
  {
    VPackArrayBuilder t(&trxs);
    for (auto const& server : servers) {
      {
        VPackArrayBuilder ta(&trxs);
        auto const serverName = server.first;
        LOG_TOPIC("fa76d", DEBUG, Logger::SUPERVISION)
            << "Removing long overdue db server " << serverName
            << "last seen: " << server.second;
        {
          VPackObjectBuilder oper(&trxs);  // Operation for one server
          trxs.add("/arango/Supervision/Health/" + serverName, del.slice());
          trxs.add("/arango/Plan/DBServers/" + serverName, del.slice());
          trxs.add("/arango/Current/DBServers/" + serverName, del.slice());
          trxs.add("/arango/Target/MapUniqueToShortID/" + serverName, del.slice());
          trxs.add("/arango/Current/ServersKnown/" + serverName, del.slice());
          trxs.add("/arango/Current/ServersRegistered/" + serverName, del.slice());
          for (auto const& j : currentDatabases.children()) {
            trxs.add("/arango/Current/Databases/" + j.first + "/" + serverName,
                     del.slice());
          }
        }
        if (!server.second.empty()) {  // Timestamp unchanged only, if persistent entry
          VPackObjectBuilder prec(&trxs);
          trxs.add("/arango/Supervision/Health/" + serverName + "/Timestamp",
                   VPackValue(server.second));
        }
      }
    }
  }
  if (servers.size() > 0) {
    _nextServerCleanup = std::chrono::system_clock::now() + std::chrono::seconds(3600);
    _agent->write(envelope);
  }

  trxs.clear();
  servers = deletionCandidates(snapshot, transient, "Coordinators");
  {
    VPackArrayBuilder t(&trxs);
    for (auto const& server : servers) {
      {
        VPackArrayBuilder ta(&trxs);
        auto const serverName = server.first;
        LOG_TOPIC("f6a7d", DEBUG, Logger::SUPERVISION)
            << "Removing long overdue coordinator " << serverName
            << "last seen: " << server.second;
        {
          VPackObjectBuilder ts(&trxs);
          trxs.add("/arango/Supervision/Health/" + serverName, del.slice());
          trxs.add("/arango/Plan/Coordinators/" + serverName, del.slice());
          trxs.add("/arango/Current/Coordinators/" + serverName, del.slice());
          trxs.add("/arango/Target/MapUniqueToShortID/" + serverName, del.slice());
          trxs.add("/arango/Current/ServersKnown/" + serverName, del.slice());
          trxs.add("/arango/Current/ServersRegistered/" + serverName, del.slice());
        }
        if (!server.second.empty()) {  // Timestamp unchanged only, if persistent entry
          VPackObjectBuilder prec(&trxs);
          trxs.add("/arango/Supervision/Health/" + serverName + "/Timestamp",
                   VPackValue(server.second));
        }
      }
    }
  }
  if (servers.size() > 0) {
    _agent->write(envelope);
  }
  _nextServerCleanup = std::chrono::system_clock::now() + std::chrono::seconds(3600);
}

void Supervision::cleanupLostCollections(Node const& snapshot,
                                         AgentInterface* agent, uint64_t& jobId) {
  std::unordered_set<std::string> failedServers;

  // Search for failed server
  //  Could also use `Target/FailedServers`
  auto const& health = snapshot.hasAsChildren(healthPrefix);
  if (!health.second) {
    return;
  }

  for (auto const& server : health.first) {
    HealthRecord record(*server.second.get());

    if (record.status == Supervision::HEALTH_STATUS_FAILED) {
      failedServers.insert(server.first);
    }
  }

  if (failedServers.size() == 0) {
    return;
  }

  // Now iterate over all shards and look for failed leaders.
  auto const& collections = snapshot.hasAsChildren("/Current/Collections");
  if (!collections.second) {
    return;
  }

  auto builder = std::make_shared<VPackBuilder>();
  {
    VPackArrayBuilder trxs(builder.get());

    for (auto const& database : collections.first) {
      auto const& dbname = database.first;

      auto const& collections = database.second->children();

      for (auto const& collection : collections) {
        auto const& colname = collection.first;

        for (auto const& shard : collection.second->children()) {
          auto const& servers = shard.second->hasAsArray("servers").first;

          TRI_ASSERT(servers.isArray());

          if (servers.length() >= 1) {
            TRI_ASSERT(servers[0].isString());
            auto const& servername = servers[0].copyString();

            if (failedServers.find(servername) != failedServers.end()) {
              // potentially lost shard
              auto const& shardname = shard.first;

              auto const& planurlinsnapshot =
                  "/Plan/Collections/" + dbname + "/" + colname + "/shards/" + shardname;

              auto const& planurl = "/arango" + planurlinsnapshot;
              auto const& currenturl = "/arango/Current/Collections/" + dbname +
                                       "/" + colname + "/" + shardname;
              auto const& healthurl =
                  "/arango/Supervision/Health/" + servername + "/Status";
              // check if it exists in Plan
              if (snapshot.has(planurlinsnapshot)) {
                continue;
              }
              LOG_TOPIC("89987", TRACE, Logger::SUPERVISION)
                  << "Found a lost shard: " << shard.first;
              // Now remove that shard
              {
                VPackArrayBuilder trx(builder.get());
                {
                  VPackObjectBuilder update(builder.get());
                  // remove the shard in current
                  builder->add(VPackValue(currenturl));
                  {
                    VPackObjectBuilder op(builder.get());
                    builder->add("op", VPackValue("delete"));
                  }
                  // add a job done entry to "Target/Finished"
                  std::string jobIdStr = std::to_string(jobId++);
                  builder->add(VPackValue("/arango/Target/Finished/" + jobIdStr));
                  {
                    VPackObjectBuilder op(builder.get());
                    builder->add("op", VPackValue("set"));
                    builder->add(VPackValue("new"));
                    {
                      VPackObjectBuilder job(builder.get());
                      builder->add("type", VPackValue("cleanUpLostCollection"));
                      builder->add("server", VPackValue(shardname));
                      builder->add("jobId", VPackValue(jobIdStr));
                      builder->add("creator", VPackValue("supervision"));
                      builder->add("timeCreated", VPackValue(timepointToString(
                                                      std::chrono::system_clock::now())));
                    }
                  }
                  // increase current version
                  builder->add(VPackValue("/arango/Current/Version"));
                  {
                    VPackObjectBuilder op(builder.get());
                    builder->add("op", VPackValue("increment"));
                  }
                }
                {
                  VPackObjectBuilder precon(builder.get());
                  // pre condition:
                  //  still in current
                  //  not in plan
                  //  still failed
                  builder->add(VPackValue(planurl));
                  {
                    VPackObjectBuilder cond(builder.get());
                    builder->add("oldEmpty", VPackValue(true));
                  }
                  builder->add(VPackValue(currenturl));
                  {
                    VPackObjectBuilder cond(builder.get());
                    builder->add("oldEmpty", VPackValue(false));
                  }
                  builder->add(VPackValue(healthurl));
                  {
                    VPackObjectBuilder cond(builder.get());
                    builder->add("old", VPackValue(Supervision::HEALTH_STATUS_FAILED));
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  auto const& trx = builder->slice();

  if (trx.length() > 0) {
    // do it! fire and forget!
    agent->write(builder);
  }
}

// Remove expired hot backup lock if exists
void Supervision::unlockHotBackup() {
  _lock.assertLockedByCurrentThread();
  if (snapshot().has(HOTBACKUP_KEY)) {
    Node tmp = snapshot()(HOTBACKUP_KEY);
    if (tmp.isString()) {
      if (std::chrono::system_clock::now() > stringToTimepoint(tmp.getString())) {
        VPackBuilder unlock;
        {
          VPackArrayBuilder trxs(&unlock);
          {
            VPackObjectBuilder u(&unlock);
            unlock.add(VPackValue(HOTBACKUP_KEY));
            {
              VPackObjectBuilder o(&unlock);
              unlock.add("op", VPackValue("delete"));
            }
          }
        }
        write_ret_t res = singleWriteTransaction(_agent, unlock, false);
      }
    }
  }
}

// Guarded by caller
bool Supervision::handleJobs() {
  _lock.assertLockedByCurrentThread();
  // Do supervision
  LOG_TOPIC("67eef", TRACE, Logger::SUPERVISION) << "Begin unlockHotBackup";
  unlockHotBackup();

  LOG_TOPIC("76ffe", TRACE, Logger::SUPERVISION) << "Begin shrinkCluster";
  shrinkCluster();

  LOG_TOPIC("43256", TRACE, Logger::SUPERVISION) << "Begin enforceReplication";
  enforceReplication();

  LOG_TOPIC("76190", TRACE, Logger::SUPERVISION)
      << "Begin cleanupLostCollections";
  cleanupLostCollections(snapshot(), _agent, _jobId);
  // Note that this function consumes job IDs, potentially many, so the member
  // is incremented inside the function. Furthermore, `cleanupLostCollections`
  // is static for catch testing purposes.

  LOG_TOPIC("00789", TRACE, Logger::SUPERVISION)
      << "Begin readyOrphanedIndexCreations";
  readyOrphanedIndexCreations();

  LOG_TOPIC("00790", TRACE, Logger::SUPERVISION)
      << "Begin checkBrokenCreatedDatabases";
  checkBrokenCreatedDatabases();

  LOG_TOPIC("69480", TRACE, Logger::SUPERVISION)
      << "Begin checkBrokenCollections";
  checkBrokenCollections();

  LOG_TOPIC("83402", TRACE, Logger::SUPERVISION)
      << "Begin checkBrokenAnalyzers";
  checkBrokenAnalyzers();

  LOG_TOPIC("00aab", TRACE, Logger::SUPERVISION) << "Begin workJobs";
  workJobs();

  LOG_TOPIC("0892b", TRACE, Logger::SUPERVISION)
      << "Begin cleanupFinishedAndFailedJobs";
  cleanupFinishedAndFailedJobs();

  return true;
}

// Guarded by caller
void Supervision::cleanupFinishedAndFailedJobs() {
  // This deletes old Supervision jobs in /Target/Finished and
  // /Target/Failed. We can be rather generous here since old
  // snapshots and log entries are kept for much longer.
  // We only keep up to 500 finished jobs and 1000 failed jobs.
  _lock.assertLockedByCurrentThread();

  constexpr size_t maximalFinishedJobs = 500;
  constexpr size_t maximalFailedJobs = 1000;

  auto cleanup = [&](std::string const& prefix, size_t limit) {
    auto const& jobs = snapshot().hasAsChildren(prefix).first;
    if (jobs.size() <= 2 * limit) {
      return;
    }
    typedef std::pair<std::string, std::string> keyDate;
    std::vector<keyDate> v;
    v.reserve(jobs.size());
    for (auto const& p : jobs) {
      auto created = p.second->hasAsString("timeCreated");
      if (created.second) {
        v.emplace_back(p.first, created.first);
      } else {
        v.emplace_back(p.first, "1970");  // will be sorted very early
      }
    }
    std::sort(v.begin(), v.end(), [](keyDate const& a, keyDate const& b) -> bool {
      return a.second < b.second;
    });
    size_t toBeDeleted = v.size() - limit;  // known to be positive
    LOG_TOPIC("98451", INFO, Logger::AGENCY) << "Deleting " << toBeDeleted
                                             << " old jobs"
                                                " in "
                                             << prefix;
    VPackBuilder trx;  // We build a transaction here
    {                  // Pair for operation, no precondition here
      VPackArrayBuilder guard1(&trx);
      {
        VPackObjectBuilder guard2(&trx);
        for (auto it = v.begin(); toBeDeleted-- > 0 && it != v.end(); ++it) {
          trx.add(VPackValue(prefix + it->first));
          {
            VPackObjectBuilder guard2(&trx);
            trx.add("op", VPackValue("delete"));
          }
        }
      }
    }
    singleWriteTransaction(_agent, trx, false);  // do not care about the result
  };

  cleanup(finishedPrefix, maximalFinishedJobs);
  cleanup(failedPrefix, maximalFailedJobs);
}

// Guarded by caller
void Supervision::workJobs() {
  _lock.assertLockedByCurrentThread();

  bool dummy = false;
  // ATTENTION: It is necessary to copy the todos here, since we modify
  // below!
  auto todos = snapshot().hasAsChildren(toDoPrefix).first;
  auto it = todos.begin();
  static std::string const FAILED = "failed";

  // In the case that there are a lot of jobs in ToDo or in Pending we cannot
  // afford to run through all of them before we do another Supervision round.
  // This is because only in a new round we discover things like a server
  // being good again. Currently, we manage to work through approx. 200 jobs
  // per second. Therefore, we have - for now - chosen to limit the number of
  // jobs actually worked on to 1000 in ToDo and 1000 in Pending. However,
  // since some jobs are just waiting, we cannot work on the same 1000
  // jobs in each round. This is where the randomization comes in. We work
  // on up to 1000 *random* jobs. This will eventually cover everything with
  // very high probability. Note that the snapshot does not change, so
  // `todos.size()` is constant for the loop, even though we do agency
  // transactions to remove ToDo jobs.
  size_t const maximalJobsPerRound = 1000;
  bool selectRandom = todos.size() > maximalJobsPerRound;

  LOG_TOPIC("00567", TRACE, Logger::SUPERVISION)
      << "Begin ToDos of type Failed*";
  bool doneFailedJob = false;
  while (it != todos.end()) {
    auto const& jobNode = *(it->second);
    if (jobNode.hasAsString("type").first.compare(0, FAILED.length(), FAILED) == 0) {
      if (selectRandom && RandomGenerator::interval(static_cast<uint64_t>(todos.size())) >
                              maximalJobsPerRound) {
        LOG_TOPIC("675fe", TRACE, Logger::SUPERVISION) << "Skipped ToDo Job";
        ++it;
        continue;
      }

      LOG_TOPIC("87812", TRACE, Logger::SUPERVISION)
          << "Begin JobContext::run()";
      JobContext(TODO, jobNode.hasAsString("jobId").first, snapshot(), _agent).run(_haveAborts);
      LOG_TOPIC("98115", TRACE, Logger::SUPERVISION)
          << "Finish JobContext::run()";
      it = todos.erase(it);
      doneFailedJob = true;
    } else {
      ++it;
    }
  }

  // Do not start other jobs, if above resilience jobs aborted stuff
  if (!_haveAborts && !doneFailedJob) {
    LOG_TOPIC("00654", TRACE, Logger::SUPERVISION) << "Begin ToDos";
    for (auto const& todoEnt : todos) {
      if (selectRandom && RandomGenerator::interval(static_cast<uint64_t>(todos.size())) >
                              maximalJobsPerRound) {
        LOG_TOPIC("77889", TRACE, Logger::SUPERVISION) << "Skipped ToDo Job";
        continue;
      }
      auto const& jobNode = *todoEnt.second;
      if (jobNode.hasAsString("type").first.compare(0, FAILED.length(), FAILED) != 0) {
        LOG_TOPIC("aa667", TRACE, Logger::SUPERVISION)
            << "Begin JobContext::run()";
        JobContext(TODO, jobNode.hasAsString("jobId").first, snapshot(), _agent).run(dummy);
        LOG_TOPIC("65bcd", TRACE, Logger::SUPERVISION)
            << "Finish JobContext::run()";
      }
    }
  }
  LOG_TOPIC("a55ce", TRACE, Logger::SUPERVISION)
      << "Updating snapshot after ToDo";
  updateSnapshot();

  LOG_TOPIC("08641", TRACE, Logger::SUPERVISION) << "Begin Pendings";
  auto const& pends = snapshot().hasAsChildren(pendingPrefix).first;
  selectRandom = pends.size() > maximalJobsPerRound;

  for (auto const& pendEnt : pends) {
    if (selectRandom && RandomGenerator::interval(static_cast<uint64_t>(pends.size())) >
                            maximalJobsPerRound) {
      LOG_TOPIC("54310", TRACE, Logger::SUPERVISION) << "Skipped Pending Job";
      continue;
    }
    auto const& jobNode = *(pendEnt.second);
    LOG_TOPIC("009ba", TRACE, Logger::SUPERVISION) << "Begin JobContext::run()";
    JobContext(PENDING, jobNode.hasAsString("jobId").first, snapshot(), _agent).run(dummy);
    LOG_TOPIC("99006", TRACE, Logger::SUPERVISION)
        << "Finish JobContext::run()";
  }
}

bool Supervision::verifyCoordinatorRebootID(std::string const& coordinatorID,
                                            uint64_t wantedRebootID, bool& coordinatorFound) {
  // check if the coordinator exists in health
  std::string const& health = serverHealth(coordinatorID);
  LOG_TOPIC("44432", DEBUG, Logger::SUPERVISION)
      << "verifyCoordinatorRebootID: coordinatorID=" << coordinatorID
      << " health=" << health;

  // if the server is not found, health is an empty string
  coordinatorFound = !health.empty();
  if (health != "GOOD" && health != "BAD") {
    return false;
  }

  // now lookup reboot id
  std::pair<uint64_t, bool> rebootID =
      snapshot().hasAsUInt(curServersKnown + coordinatorID + "/" + StaticStrings::RebootId);
  LOG_TOPIC("54326", DEBUG, Logger::SUPERVISION)
      << "verifyCoordinatorRebootID: rebootId=" << rebootID.first
      << " bool=" << rebootID.second;
  return rebootID.second && rebootID.first == wantedRebootID;
}

void Supervision::deleteBrokenDatabase(std::string const& database,
                                       std::string const& coordinatorID,
                                       uint64_t rebootID, bool coordinatorFound) {
  auto envelope = std::make_shared<Builder>();
  {
    VPackArrayBuilder trxs(envelope.get());
    {
      VPackArrayBuilder trx(envelope.get());
      {
        VPackObjectBuilder operation(envelope.get());

        // increment Plan Version
        {
          VPackObjectBuilder o(envelope.get(), _agencyPrefix + "/" + PLAN_VERSION);
          envelope->add("op", VPackValue("increment"));
        }

        // delete the database from Plan/Databases
        {
          VPackObjectBuilder o(envelope.get(), _agencyPrefix + planDBPrefix + database);
          envelope->add("op", VPackValue("delete"));
        }

        // delete the database from Plan/Collections
        {
          VPackObjectBuilder o(envelope.get(), _agencyPrefix + planColPrefix + database);
          envelope->add("op", VPackValue("delete"));
        }

        // delete the database from Plan/Analyzers
        {
          VPackObjectBuilder o(envelope.get(), _agencyPrefix + planAnalyzersPrefix + database);
          envelope->add("op", VPackValue("delete"));
        }
      }
      {
        // precondition that this database is still in Plan and is building
        VPackObjectBuilder preconditions(envelope.get());
        auto const databasesPath = plan()->databases()->database(database)->str();
        envelope->add(databasesPath + "/" + StaticStrings::AttrIsBuilding,
                      VPackValue(true));
        envelope->add(databasesPath + "/" + StaticStrings::AttrCoordinatorRebootId,
                      VPackValue(rebootID));
        envelope->add(databasesPath + "/" + StaticStrings::AttrCoordinator,
                      VPackValue(coordinatorID));

        {
          VPackObjectBuilder precondition(envelope.get(),
                                          _agencyPrefix + healthPrefix + coordinatorID);
          envelope->add("oldEmpty", VPackValue(!coordinatorFound));
        }
      }
    }
  }

  write_ret_t res = _agent->write(envelope);
  if (!res.successful()) {
    LOG_TOPIC("38482", DEBUG, Logger::SUPERVISION)
        << "failed to delete broken database in agency. Will retry "
        << envelope->toJson();
  }
}

void Supervision::deleteBrokenCollection(std::string const& database,
                                         std::string const& collection,
                                         std::string const& coordinatorID,
                                         uint64_t rebootID, bool coordinatorFound) {
  auto envelope = std::make_shared<Builder>();
  {
    VPackArrayBuilder trxs(envelope.get());
    {
      std::string collection_path =
          plan()->collections()->database(database)->collection(collection)->str();

      VPackArrayBuilder trx(envelope.get());
      {
        VPackObjectBuilder operation(envelope.get());
        // increment Plan Version
        {
          VPackObjectBuilder o(envelope.get(), _agencyPrefix + "/" + PLAN_VERSION);
          envelope->add("op", VPackValue("increment"));
        }
        // delete the collection from Plan/Collections/<db>
        {
          VPackObjectBuilder o(envelope.get(), collection_path);
          envelope->add("op", VPackValue("delete"));
        }
      }
      {
        // precondition that this collection is still in Plan and is building
        VPackObjectBuilder preconditions(envelope.get());
        envelope->add(collection_path + "/" + StaticStrings::AttrIsBuilding,
                      VPackValue(true));
        envelope->add(collection_path + "/" + StaticStrings::AttrCoordinatorRebootId,
                      VPackValue(rebootID));
        envelope->add(collection_path + "/" + StaticStrings::AttrCoordinator,
                      VPackValue(coordinatorID));

        {
          VPackObjectBuilder precondition(envelope.get(), _agencyPrefix + healthPrefix +
                                                              "/" + coordinatorID);
          envelope->add("oldEmpty", VPackValue(!coordinatorFound));
        }
      }
    }
  }

  write_ret_t res = _agent->write(envelope);
  if (!res.successful()) {
    LOG_TOPIC("38485", DEBUG, Logger::SUPERVISION)
        << "failed to delete broken collection in agency. Will retry. "
        << envelope->toJson();
  }
}

void Supervision::restoreBrokenAnalyzersRevision(std::string const& database,
                                                 AnalyzersRevision::Revision revision,
                                                 AnalyzersRevision::Revision buildingRevision,
                                                 std::string const& coordinatorID,
                                                 uint64_t rebootID,
                                                 bool coordinatorFound) {
  auto envelope = std::make_shared<Builder>();
  {
    VPackArrayBuilder trxs(envelope.get());
    {
      std::string anPath = _agencyPrefix + planAnalyzersPrefix + database + "/";

      VPackArrayBuilder trx(envelope.get());
      {
        VPackObjectBuilder operation(envelope.get());
        // increment Plan Version
        {
          VPackObjectBuilder o(envelope.get(), _agencyPrefix + "/" + PLAN_VERSION);
          envelope->add("op", VPackValue("increment"));
        }
        // restore the analyzers revision from Plan/Analyzers/<db>/
        {
          VPackObjectBuilder o(envelope.get(), anPath + StaticStrings::AnalyzersBuildingRevision);
          envelope->add("op", VPackValue("decrement"));
        }
        {
          VPackObjectBuilder o(envelope.get(), anPath + StaticStrings::AttrCoordinatorRebootId);
          envelope->add("op", VPackValue("delete"));
        }
        {
          VPackObjectBuilder o(envelope.get(), anPath + StaticStrings::AttrCoordinator);
          envelope->add("op", VPackValue("delete"));
        }
      }
      {
        // precondition that this analyzers revision is still in Plan and is building
        VPackObjectBuilder preconditions(envelope.get());
        envelope->add(anPath + StaticStrings::AnalyzersRevision,
                      VPackValue(revision));
        envelope->add(anPath + StaticStrings::AnalyzersBuildingRevision,
                      VPackValue(buildingRevision));
        envelope->add(anPath + StaticStrings::AttrCoordinatorRebootId,
                      VPackValue(rebootID));
        envelope->add(anPath + StaticStrings::AttrCoordinator,
                      VPackValue(coordinatorID));
        {
          VPackObjectBuilder precondition(envelope.get(), _agencyPrefix + healthPrefix +
                                          "/" + coordinatorID);
          envelope->add("oldEmpty", VPackValue(!coordinatorFound));
        }
      }
    }
  }

  write_ret_t res = _agent->write(envelope);
  if (!res.successful()) {
    LOG_TOPIC("e43cb", DEBUG, Logger::SUPERVISION)
        << "failed to resore broken analyzers revision in agency. Will retry. "
        << envelope->toJson();
  }
}

void Supervision::resourceCreatorLost(
    std::shared_ptr<Node> const& resource,
    std::function<void(const ResourceCreatorLostEvent&)> const& action) {
  //  check if the coordinator exists and its reboot is the same as specified
  auto [rebootID, rebootIDExists] =
      resource->hasAsUInt(StaticStrings::AttrCoordinatorRebootId);
  auto [coordinatorID, coordinatorIDExists] =
      resource->hasAsString(StaticStrings::AttrCoordinator);

  bool keepResource = true;
  bool coordinatorFound = false;

  if (rebootIDExists && coordinatorIDExists) {
    keepResource = Supervision::verifyCoordinatorRebootID(coordinatorID,
                                                          rebootID, coordinatorFound);
    // incomplete data, should not happen
  } else {
    //          v---- Please note this awesome log-id
    LOG_TOPIC("dbbad", WARN, Logger::SUPERVISION)
        << "resource has set `isBuilding` but is missing coordinatorID and "
           "rebootID";
  }

  if (!keepResource) {
    action(ResourceCreatorLostEvent{resource, coordinatorID,
                                    rebootID, coordinatorFound});
  }
}

void Supervision::ifResourceCreatorLost(
    std::shared_ptr<Node> const& resource,
    std::function<void(const ResourceCreatorLostEvent&)> const& action) {
  // check if isBuilding is set and it is true
  auto isBuilding = resource->hasAsBool(StaticStrings::AttrIsBuilding);
  if (isBuilding.first && isBuilding.second) {
    // this database or collection is currently being built
    resourceCreatorLost(resource, action);
  }
}

void Supervision::checkBrokenCreatedDatabases() {
  _lock.assertLockedByCurrentThread();

  // check if snapshot has databases
  std::pair<Node const&, bool> databases = snapshot().hasAsNode(planDBPrefix);
  if (!databases.second) {
    return;
  }

  // dbpair is <std::string, std::shared_ptr<Node>>
  for (auto const& dbpair : databases.first.children()) {
    std::shared_ptr<Node> const& db = dbpair.second;

    LOG_TOPIC("24152", DEBUG, Logger::SUPERVISION) << "checkBrokenDbs: " << *db;

    ifResourceCreatorLost(db, [&](ResourceCreatorLostEvent const& ev) {
      LOG_TOPIC("fe522", INFO, Logger::SUPERVISION)
          << "checkBrokenCreatedDatabases: removing skeleton database with "
             "name "
          << dbpair.first;
      // delete this database and all of its collections
      deleteBrokenDatabase(dbpair.first, ev.coordinatorId,
                           ev.coordinatorRebootId, ev.coordinatorFound);
    });
  }
}

void Supervision::checkBrokenCollections() {
  _lock.assertLockedByCurrentThread();

  // check if snapshot has databases
  std::pair<Node const&, bool> collections = snapshot().hasAsNode(planColPrefix);
  if (!collections.second) {
    return;
  }

  // dbpair is <std::string, std::shared_ptr<Node>>
  for (auto const& dbpair : collections.first.children()) {
    std::shared_ptr<Node> const& db = dbpair.second;

    for (auto const& collectionPair : db->children()) {
      // collectionPair.first is collection id
      std::pair<std::string, bool> collectionNamePair =
          collectionPair.second->hasAsString(StaticStrings::DataSourceName);
      std::string const& collectionName = collectionNamePair.first;
      if (!collectionNamePair.second || collectionName.empty() ||
          collectionName.front() == '_') {
        continue;
      }

      ifResourceCreatorLost(collectionPair.second, [&](ResourceCreatorLostEvent const& ev) {
        LOG_TOPIC("fe523", INFO, Logger::SUPERVISION)
            << "checkBrokenCollections: removing broken collection with name "
            << dbpair.first;
        // delete this database and all of its collections
        deleteBrokenCollection(dbpair.first, collectionPair.first, ev.coordinatorId,
                               ev.coordinatorRebootId, ev.coordinatorFound);
      });
    }
  }
}

void Supervision::checkBrokenAnalyzers() {
  _lock.assertLockedByCurrentThread();

  // check if snapshot has analyzers
  auto [node, exists] = snapshot().hasAsNode(planAnalyzersPrefix);
  if (!exists) {
    return;
  }

  for (auto const& dbData : node.children()) {
    auto const& revisions = dbData.second;
    auto const revision = revisions->hasAsUInt(StaticStrings::AnalyzersRevision);
    auto const buildingRevision = revisions->hasAsUInt(StaticStrings::AnalyzersBuildingRevision);
    if (revision.second && buildingRevision.second && revision.first != buildingRevision.first) {
      resourceCreatorLost(revisions, [this, &dbData, &revision, &buildingRevision](ResourceCreatorLostEvent const& ev) {
        LOG_TOPIC("ae5a3", INFO, Logger::SUPERVISION)
            << "checkBrokenAnalyzers: fixing broken analyzers revision with database name "
            << dbData.first;
        restoreBrokenAnalyzersRevision(dbData.first, revision.first, buildingRevision.first,
                                       ev.coordinatorId, ev.coordinatorRebootId, ev.coordinatorFound);
      });
    }
  }
}

void Supervision::readyOrphanedIndexCreations() {
  _lock.assertLockedByCurrentThread();

  if (snapshot().has(planColPrefix) && snapshot().has(curColPrefix)) {
    auto const& plannedDBs = snapshot()(planColPrefix).children();
    auto const& currentDBs = snapshot()(curColPrefix);

    for (auto const& db : plannedDBs) {
      std::string const& dbname = db.first;
      auto const& database = *(db.second);
      auto const& plannedCols = database.children();
      for (auto const& col : plannedCols) {
        auto const& colname = col.first;
        std::string const& colPath = dbname + "/" + colname + "/";
        auto const& collection = *(col.second);
        std::unordered_set<std::string> built;
        Slice indexes;
        if (collection.has("indexes")) {
          indexes = collection("indexes").getArray();
          if (indexes.length() > 0) {
            for (auto const& planIndex : VPackArrayIterator(indexes)) {
              if (planIndex.hasKey(StaticStrings::IndexIsBuilding) &&
                  collection.has("shards")) {
                auto const& planId = planIndex.get("id");
                auto const& shards = collection("shards");
                if (collection.has("numberOfShards") &&
                    collection("numberOfShards").isUInt()) {
                  auto const& nshards = collection("numberOfShards").getUInt();
                  if (nshards == 0) {
                    continue;
                  }
                  size_t nIndexes = 0;
                  for (auto const& sh : shards.children()) {
                    auto const& shname = sh.first;

                    if (currentDBs.has(colPath + shname + "/indexes")) {
                      auto const& curIndexes =
                          currentDBs(colPath + shname + "/indexes").slice();
                      for (auto const& curIndex : VPackArrayIterator(curIndexes)) {
                        auto const& curId = curIndex.get("id");
                        if (basics::VelocyPackHelper::equal(planId, curId, false)) {
                          ++nIndexes;
                        }
                      }
                    }
                  }
                  if (nIndexes == nshards) {
                    built.emplace(planId.copyString());
                  }
                }
              }
            }
          }
        }

        // We have some indexes, that have been built and are isBuilding still
        if (!built.empty()) {
          auto envelope = std::make_shared<Builder>();
          {
            VPackArrayBuilder trxs(envelope.get());
            {
              VPackArrayBuilder trx(envelope.get());
              {
                VPackObjectBuilder operation(envelope.get());
                envelope->add(VPackValue(_agencyPrefix + "/" + PLAN_VERSION));
                {
                  VPackObjectBuilder o(envelope.get());
                  envelope->add("op", VPackValue("increment"));
                }
                envelope->add(VPackValue(_agencyPrefix + planColPrefix + colPath + "indexes"));
                VPackArrayBuilder value(envelope.get());
                for (auto const& planIndex : VPackArrayIterator(indexes)) {
                  if (built.find(planIndex.get("id").copyString()) != built.end()) {
                    {
                      VPackObjectBuilder props(envelope.get());
                      for (auto const& prop : VPackObjectIterator(planIndex)) {
                        auto const& key = prop.key.copyString();
                        if (key != StaticStrings::IndexIsBuilding) {
                          envelope->add(key, prop.value);
                        }
                      }
                    }
                  } else {
                    envelope->add(planIndex);
                  }
                }
              }
              {
                VPackObjectBuilder precondition(envelope.get());
                envelope->add(VPackValue(_agencyPrefix + planColPrefix + colPath + "indexes"));
                envelope->add(indexes);
              }
            }
          }

          write_ret_t res = _agent->write(envelope);
          if (!res.successful()) {
            LOG_TOPIC("3848f", DEBUG, Logger::SUPERVISION)
                << "failed to report ready index to agency. Will retry.";
          }
        }
      }
    }
  }
}

void Supervision::enforceReplication() {
  _lock.assertLockedByCurrentThread();

  // First check the number of AddFollower and RemoveFollower jobs in ToDo:
  // We always maintain that we have at most maxNrAddRemoveJobsInTodo
  // AddFollower or RemoveFollower jobs in ToDo. These are all long-term
  // cleanup jobs, so they can be done over time. This is to ensure that
  // there is no overload on the Agency job system. Therefore, if this
  // number is at least maxNrAddRemoveJobsInTodo, we skip the rest of
  // the function:
  int const maxNrAddRemoveJobsInTodo = 50;

  auto const& todos = snapshot().hasAsChildren(toDoPrefix).first;
  int nrAddRemoveJobsInTodo = 0;
  for (auto it = todos.begin(); it != todos.end(); ++it) {
    auto jobNode = *(it->second);
    auto t = jobNode.hasAsString("type");
    if (t.second && (t.first == "addFollower" || t.first == "removeFollower")) {
      if (++nrAddRemoveJobsInTodo >= maxNrAddRemoveJobsInTodo) {
        return;
      }
    }
  }

  // We will loop over plannedDBs, so we use hasAsChildren
  auto const& plannedDBs = snapshot().hasAsChildren(planColPrefix).first;
  // We will lookup in currentDBs, so we use hasAsNode
  auto const& currentDBs = snapshot().hasAsNode(curColPrefix).first;

  for (const auto& db_ : plannedDBs) {  // Planned databases
    auto const& db = *(db_.second);
    for (const auto& col_ : db.children()) {  // Planned collections
      auto const& col = *(col_.second);

      size_t replicationFactor;
      auto replFact = col.hasAsUInt(StaticStrings::ReplicationFactor);
      if (replFact.second) {
        replicationFactor = replFact.first;
      } else {
        auto replFact2 = col.hasAsString(StaticStrings::ReplicationFactor);
        if (replFact2.second && replFact2.first == StaticStrings::Satellite) {
          // satellites => distribute to every server
          auto available = Job::availableServers(snapshot());
          replicationFactor = Job::countGoodOrBadServersInList(snapshot(), available);
        } else {
          LOG_TOPIC("d3b54", DEBUG, Logger::SUPERVISION)
              << "no replicationFactor entry in " << col.toJson();
          continue;
        }
      }

      bool const clone = col.has(StaticStrings::DistributeShardsLike);
      bool const isBuilding = std::invoke([&col] {
        auto pair = col.hasAsBool(StaticStrings::AttrIsBuilding);
        // Return true if the attribute exists, is a bool, and that bool is
        // true. Return false otherwise.
        return pair.first && pair.second;
      });

      if (!clone && !isBuilding) {
        for (auto const& shard_ : col.hasAsChildren("shards").first) {  // Pl shards
          auto const& shard = *(shard_.second);
          VPackBuilder onlyFollowers;
          {
            VPackArrayBuilder guard(&onlyFollowers);
            bool first = true;
            for (auto const& pp : VPackArrayIterator(shard.slice())) {
              if (!first) {
                onlyFollowers.add(pp);
              }
              first = false;
            }
          }
          size_t actualReplicationFactor =
              1 + Job::countGoodOrBadServersInList(snapshot(), onlyFollowers.slice());
          // leader plus GOOD followers
          size_t apparentReplicationFactor = shard.slice().length();

          if (actualReplicationFactor != replicationFactor ||
              apparentReplicationFactor != replicationFactor) {
            // First check the case that not all are in sync:
            std::string curPath =
                db_.first + "/" + col_.first + "/" + shard_.first + "/servers";
            auto const& currentServers = currentDBs.hasAsArray(curPath);
            size_t inSyncReplicationFactor = actualReplicationFactor;
            if (currentServers.second) {
              if (currentServers.first.length() < actualReplicationFactor) {
                inSyncReplicationFactor = currentServers.first.length();
              }
            }

            // Check that there is not yet an addFollower or removeFollower
            // or moveShard job in ToDo for this shard:
            auto const& todo = snapshot().hasAsChildren(toDoPrefix).first;
            bool found = false;
            for (auto const& pair : todo) {
              auto const& job = pair.second;
              auto tmp_type = job->hasAsString("type");
              auto tmp_shard = job->hasAsString("shard");
              if ((tmp_type.first == "addFollower" || tmp_type.first == "removeFollower" ||
                   tmp_type.first == "moveShard") &&
                  tmp_shard.first == shard_.first) {
                found = true;
                LOG_TOPIC("441b6", DEBUG, Logger::SUPERVISION)
                    << "already found "
                       "addFollower or removeFollower job in ToDo, not "
                       "scheduling "
                       "again for shard "
                    << shard_.first;
                break;
              }
            }
            // Check that shard is not locked:
            if (snapshot().has(blockedShardsPrefix + shard_.first)) {
              found = true;
            }
            if (!found) {
              if (actualReplicationFactor < replicationFactor) {
                AddFollower(snapshot(), _agent, std::to_string(_jobId++),
                            "supervision", db_.first, col_.first, shard_.first)
                    .create();
                if (++nrAddRemoveJobsInTodo >= maxNrAddRemoveJobsInTodo) {
                  return;
                }
              } else if (apparentReplicationFactor > replicationFactor &&
                         inSyncReplicationFactor >= replicationFactor) {
                RemoveFollower(snapshot(), _agent, std::to_string(_jobId++),
                               "supervision", db_.first, col_.first, shard_.first)
                    .create();
                if (++nrAddRemoveJobsInTodo >= maxNrAddRemoveJobsInTodo) {
                  return;
                }
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

  auto const& snap = snapshot();

  std::function<std::string(std::string const&, std::string const&)> resolve;
  resolve = [&](std::string const& db, std::string const& col) {
    std::string s;
    auto const& tmp_n = snap.hasAsNode(planColPrefix + db + "/" + col);
    if (tmp_n.second) {
      Node const& n = tmp_n.first;
      s = n.hasAsString("distributeShardsLike").first;
    }
    return (s.empty()) ? col : resolve(db, s);
  };

  for (auto const& database : snapshot().hasAsChildren(planColPrefix).first) {
    for (auto const& collection : database.second->children()) {
      if (collection.second->has("distributeShardsLike")) {
        auto prototype =
            (*collection.second).hasAsString("distributeShardsLike").first;
        if (!prototype.empty()) {
          std::string u;
          try {
            u = resolve(database.first, prototype);
          } catch (...) {
          }
          if (u != prototype) {
            {
              VPackArrayBuilder trx(&migrate);
              {
                VPackObjectBuilder oper(&migrate);
                migrate.add(planColPrefix + database.first + "/" +
                                collection.first + "/" + "distributeShardsLike",
                            VPackValue(u));
              }
              {
                VPackObjectBuilder prec(&migrate);
                migrate.add(planColPrefix + database.first + "/" +
                                collection.first + "/" + "distributeShardsLike",
                            VPackValue(prototype));
              }
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

  auto const& todo = snapshot().hasAsChildren(toDoPrefix).first;
  auto const& pending = snapshot().hasAsChildren(pendingPrefix).first;

  if (!todo.empty() || !pending.empty()) {  // This is low priority
    return;
  }

  // Get servers from plan
  auto availServers = Job::availableServers(snapshot());

  // set by external service like Kubernetes / Starter / DCOS
  size_t targetNumDBServers;
  std::string const NDBServers("/Target/NumberOfDBServers");

  if (snapshot().hasAsUInt(NDBServers).second) {
    targetNumDBServers = snapshot().hasAsUInt(NDBServers).first;
  } else {
    LOG_TOPIC("7aa3b", TRACE, Logger::SUPERVISION)
        << "Targeted number of DB servers not set yet";
    return;
  }

  // Only if number of servers in target is smaller than the available
  if (targetNumDBServers < availServers.size()) {
    // Minimum 1 DB server must remain
    if (availServers.size() == 1) {
      LOG_TOPIC("4ced8", DEBUG, Logger::SUPERVISION)
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
    auto const& databases = snapshot().hasAsChildren(planColPrefix).first;
    for (auto const& database : databases) {
      for (auto const& collptr : database.second->children()) {
        auto const& node = *collptr.second;
        if (node.hasAsUInt("replicationFactor").second) {
          auto replFact = node.hasAsUInt("replicationFactor").first;
          if (replFact > maxReplFact) {
            maxReplFact = replFact;
          }
        }
        // Note that this could be a SatelliteCollection, in any case, ignore:
      }
    }

    // mop: do not account any failedservers in this calculation..the ones
    // having
    // a state of failed still have data of interest to us! We wait indefinitely
    // for them to recover or for the user to remove them
    if (maxReplFact < availServers.size()) {
      // Clean out as long as number of available servers is bigger
      // than maxReplFactor and bigger than targeted number of db servers
      if (availServers.size() > maxReplFact && availServers.size() > targetNumDBServers) {
        // Sort servers by name
        std::sort(availServers.begin(), availServers.end());

        // Schedule last server for cleanout
        bool dummy;
        CleanOutServer(snapshot(), _agent, std::to_string(_jobId++),
                       "supervision", availServers.back())
            .run(dummy);
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
  _okThreshold = _agent->config().supervisionOkThreshold();
  _gracePeriod = _agent->config().supervisionGracePeriod();
  return start();
}

static std::string const syncLatest = "/Sync/LatestID";

void Supervision::getUniqueIds() {
  _lock.assertLockedByCurrentThread();

  int64_t n = 10000;

  std::string path = _agencyPrefix + "/Sync/LatestID";
  auto builder = std::make_shared<Builder>();
  {
    VPackArrayBuilder a(builder.get());
    {
      VPackArrayBuilder b(builder.get());
      {
        VPackObjectBuilder c(builder.get());
        {
          builder->add(VPackValue(path));
          VPackObjectBuilder b(builder.get());
          builder->add("op", VPackValue("increment"));
          builder->add("step", VPackValue(n));
        }
      }
    }
    {
      VPackArrayBuilder a(builder.get());
      builder->add(VPackValue(path));
    }
  }  // [[{path:{"op":"increment","step":n}}],[path]]

  auto ret = _agent->transact(builder);
  if (ret.accepted) {
    try {
      _jobIdMax =
          ret.result->slice()[1]
              .get(std::vector<std::string>({"arango", "Sync", "LatestID"}))
              .getUInt();
      _jobId = _jobIdMax - n;
    } catch (std::exception const& e) {
      LOG_TOPIC("4da4b", ERR, Logger::SUPERVISION)
          << "Failed to acquire job IDs from agency: " << e.what() << __FILE__
          << " " << __LINE__;
    }
  }
}

void Supervision::beginShutdown() {
  // Personal hygiene
  Thread::beginShutdown();

  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}

Node const& Supervision::snapshot() const {
  if (_snapshot == nullptr) {
    _snapshot = (_spearhead.has(_agencyPrefix)) ? _spearhead.nodePtr(_agencyPrefix)
                                                : _spearhead.nodePtr();
  }
  return *_snapshot;
}
