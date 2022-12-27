////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include <Basics/StringUtils.h>
#include <Basics/overload.h>
#include <thread>

#include "Agency/ActiveFailoverJob.h"
#include "Agency/AddFollower.h"
#include "Agency/Agent.h"
#include "Agency/CleanOutServer.h"
#include "Agency/FailedServer.h"
#include "Agency/Helpers.h"
#include "Agency/Job.h"
#include "Agency/JobContext.h"
#include "Agency/RemoveFollower.h"
#include "Agency/Store.h"
#include "AgencyPaths.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterHelpers.h"
#include "Cluster/ServerState.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"
#include "Metrics/MetricsFeature.h"
#include "Random/RandomGenerator.h"
#include "Replication2/AgencyMethods.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/Algorithms.h"
#include "Replication2/ReplicatedLog/ParticipantsHealth.h"
#include "Replication2/ReplicatedLog/Supervision.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/Supervision.h"
#include "StorageEngine/HealthData.h"
#include "Basics/ScopeGuard.h"
#include "MoveShard.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::application_features;
using namespace arangodb::cluster::paths;
using namespace arangodb::cluster::paths::aliases;

struct RuntimeScale {
  static metrics::LogScale<uint64_t> scale() { return {2, 50, 8000, 10}; }
};
struct WaitForReplicationScale {
  static metrics::LogScale<uint64_t> scale() { return {2, 10, 2000, 10}; }
};

DECLARE_COUNTER(arangodb_agency_supervision_failed_server_total,
                "Counter for FailedServer jobs");
DECLARE_HISTOGRAM(arangodb_agency_supervision_runtime_msec, RuntimeScale,
                  "Agency Supervision runtime histogram [ms]");
DECLARE_HISTOGRAM(arangodb_agency_supervision_runtime_wait_for_replication_msec,
                  WaitForReplicationScale,
                  "Agency Supervision wait for replication time [ms]");

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

  HealthRecord(std::string const& sn, std::string const& ep,
               std::string const& ho, std::string const& en,
               std::string const& sv, std::string const& ae)
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
    if (auto newShortName = node.hasAsString("ShortName");
        shortName.empty() && newShortName) {
      shortName = newShortName.value();
    }
    if (auto newEndpoint = node.hasAsString("Endpoint");
        endpoint.empty() && newEndpoint) {
      endpoint = newEndpoint.value();
    }
    if (auto newHostId = node.hasAsString("Host");
        hostId.empty() && newHostId) {
      hostId = newHostId.value();
    }
    if (node.has("Status")) {
      status = node.hasAsString("Status").value();
      if (node.has("SyncStatus")) {  // New format
        version = 2;
        syncStatus = node.hasAsString("SyncStatus").value();
        if (node.has("SyncTime")) {
          syncTime = node.hasAsString("SyncTime").value();
        }
        if (node.has("LastAckedTime")) {
          lastAcked = node.hasAsString("LastAckedTime").value();
        }
        if (node.has("AdvertisedEndpoint")) {
          version = 3;
          advertisedEndpoint = node.hasAsString("AdvertisedEndpoint").value();
        } else {
          advertisedEndpoint.clear();
        }
        if (node.has("Engine") && node.has("Version")) {
          version = 4;
          engine = node.hasAsString("Engine").value();
          serverVersion = node.hasAsString("Version").value();
        } else {
          engine.clear();
          serverVersion.clear();
        }
      } else if (node.has("LastHeartbeatStatus")) {
        version = 1;
        syncStatus = node.hasAsString("LastHeartbeatStatus").value();
        if (node.has("LastHeartbeatSent")) {
          syncTime = node.hasAsString("LastHeartbeatSent").value();
        }
        if (node.has("LastHeartbeatAcked")) {
          lastAcked = node.hasAsString("LastHeartbeatAcked").value();
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
    obj.add("Timestamp",
            VPackValue(timepointToString(std::chrono::system_clock::now())));
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

Supervision::Supervision(ArangodServer& server)
    : arangodb::Thread(server, "Supervision"),
      _agent(nullptr),
      _spearhead(server, _agent),
      _snapshot(nullptr),
      _transient("Transient"),
      _frequency(1.),
      _gracePeriod(10.),
      _okThreshold(5.),
      _delayAddFollower(0),
      _delayFailedFollower(0),
      _jobId(0),
      _jobIdMax(0),
      _lastUpdateIndex(0),
      _haveAborts(false),
      _upgraded(false),
      _nextServerCleanup(),
      _supervision_runtime_msec(
          server.getFeature<metrics::MetricsFeature>().add(
              arangodb_agency_supervision_runtime_msec{})),
      _supervision_runtime_wait_for_sync_msec(
          server.getFeature<metrics::MetricsFeature>().add(
              arangodb_agency_supervision_runtime_wait_for_replication_msec{})),
      _supervision_failed_server_counter(
          server.getFeature<metrics::MetricsFeature>().add(
              arangodb_agency_supervision_failed_server_total{})) {}

Supervision::~Supervision() { shutdown(); }

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
  Slice fails = snapshot().hasAsSlice(failedServersPrefix).value();
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
      for (auto const& recPair :
           snapshot().hasAsChildren(healthPrefix).value().get()) {
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
      maintenanceState = snapshot()
                             .get(supervisionMaintenance)
                             .value()
                             .get()
                             .getString()
                             .value();
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
        builder.add(
            supervisionMaintenance,
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
    Node const& tmp = snapshot().get(HOTBACKUP_KEY).value().get();
    if (tmp.isNumber()) {
      if (tmp.getInt() == 0) {
        VPackArrayBuilder trx(&builder);
        {
          VPackObjectBuilder o(&builder);
          builder.add(
              HOTBACKUP_KEY,
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

void handleOnStatusDBServer(
    Agent* agent, Node const& snapshot, HealthRecord& persisted,
    HealthRecord& transisted, std::string const& serverID,
    uint64_t const& jobId, std::shared_ptr<VPackBuilder>& envelope,
    std::unordered_set<std::string> const& dbServersInMaintenance,
    uint64_t delayFailedFollower, bool failedLeaderAddsFollower) {
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
      if (!dbServersInMaintenance.contains(serverID)) {
        envelope = std::make_shared<VPackBuilder>();
        agent->supervision()._supervision_failed_server_counter.operator++();
        std::string notBefore;
        if (delayFailedFollower > 0) {
          auto now = std::chrono::system_clock::now();
          notBefore = timepointToString(
              now + std::chrono::seconds(delayFailedFollower));
        }
        FailedServer(snapshot, agent, std::to_string(jobId), "supervision",
                     serverID, notBefore, failedLeaderAddsFollower)
            .create(envelope);
      }
    }
  }
}

void handleOnStatusCoordinator(Agent* agent, Node const& snapshot,
                               HealthRecord& persisted,
                               HealthRecord& transisted,
                               std::string const& serverID) {
  if (transisted.status == Supervision::HEALTH_STATUS_FAILED) {
    VPackBuilder create;
    {
      VPackArrayBuilder tx(&create);
      {
        VPackObjectBuilder b(&create);
        // unconditionally increase reboot id and plan version
        Job::addIncreaseRebootId(create, serverID);

        // if the current foxxmaster server failed => reset the value to ""
        if (snapshot.hasAsString(foxxmaster).value() == serverID) {
          create.add(foxxmaster, VPackValue(""));
        }
      }
    }
    singleWriteTransaction(agent, create, false);
  }
}

void handleOnStatusSingle(Agent* agent, Node const& snapshot,
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
      ActiveFailoverJob(snapshot, agent, std::to_string(jobId), "supervision",
                        serverID)
          .create(envelope);
    }
  }
}

void handleOnStatus(
    Agent* agent, Node const& snapshot, HealthRecord& persisted,
    HealthRecord& transisted, std::string const& serverID,
    uint64_t const& jobId, std::shared_ptr<VPackBuilder>& envelope,
    std::unordered_set<std::string> const& dbServersInMaintenance,
    uint64_t delayFailedFollower, bool failedLeaderAddsFollower) {
  if (ClusterHelpers::isDBServerName(serverID)) {
    handleOnStatusDBServer(agent, snapshot, persisted, transisted, serverID,
                           jobId, envelope, dbServersInMaintenance,
                           delayFailedFollower, failedLeaderAddsFollower);
  } else if (ClusterHelpers::isCoordinatorName(serverID)) {
    handleOnStatusCoordinator(agent, snapshot, persisted, transisted, serverID);
  } else if (serverID.compare(0, 4, "SNGL") == 0) {
    handleOnStatusSingle(agent, snapshot, persisted, transisted, serverID,
                         jobId, envelope);
  } else {
    LOG_TOPIC("86191", ERR, Logger::SUPERVISION)
        << "Unknown server type. No supervision action taken. " << serverID;
  }
}

// Check all DB servers, guarded above doChecks
std::vector<check_t> Supervision::check(std::string const& type) {
  // Dead lock detection
  _lock.assertLockedByCurrentThread();

  // Book keeping
  std::vector<check_t> ret;
  auto const& machinesPlanned =
      snapshot().hasAsChildren(std::string("Plan/") + type).value().get();
  auto const& serversRegistered =
      snapshot().hasAsNode(currentServersRegisteredPrefix).value().get();
  std::vector<std::string> todelete;
  for (auto const& machine :
       snapshot().hasAsChildren(healthPrefix).value().get()) {
    if ((type == "DBServers" &&
         ClusterHelpers::isDBServerName(machine.first)) ||
        (type == "Coordinators" &&
         ClusterHelpers::isCoordinatorName(machine.first)) ||
        (type == "Singles" && machine.first.compare(0, 4, "SNGL") == 0)) {
      // Put only those on list which are no longer planned:
      if (machinesPlanned.find(machine.first) == machinesPlanned.end()) {
        todelete.push_back(machine.first);
      }
    }
  }

  if (!todelete.empty()) {
    velocypack::Builder builder;
    removeTransactionBuilder(builder, todelete);
    _agent->write(builder.slice());
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
    if (tmp_shortName) {
      shortName = *tmp_shortName;

      // "/arango/Current/ServersRegistered/<server-id>/endpoint"
      std::string endpoint;
      std::string epPath = serverID + "/endpoint";
      if (serversRegistered.has(epPath)) {
        endpoint = serversRegistered.hasAsString(epPath).value();
      }
      // "/arango/Current/ServersRegistered/<server-id>/host"
      std::string hostId;
      std::string hoPath = serverID + "/host";
      if (serversRegistered.has(hoPath)) {
        hostId = serversRegistered.hasAsString(hoPath).value();
      }
      // "/arango/Current/ServersRegistered/<server-id>/serverVersion"
      std::string serverVersion;
      std::string svPath = serverID + "/versionString";
      if (serversRegistered.has(svPath)) {
        serverVersion = serversRegistered.hasAsString(svPath).value();
      }
      // "/arango/Current/ServersRegistered/<server-id>/engine"
      std::string engine;
      std::string enPath = serverID + "/engine";
      if (serversRegistered.has(enPath)) {
        engine = serversRegistered.hasAsString(enPath).value();
      }
      //"/arango/Current/<serverId>/externalEndpoint"
      std::string externalEndpoint;
      std::string extEndPath = serverID + "/advertisedEndpoint";
      if (serversRegistered.has(extEndPath)) {
        externalEndpoint = serversRegistered.hasAsString(extEndPath).value();
      }

      // Health records from persistence, from transience and a new one
      HealthRecord transist(shortName, endpoint, hostId, engine, serverVersion,
                            externalEndpoint);
      HealthRecord persist(shortName, endpoint, hostId, engine, serverVersion,
                           externalEndpoint);

      // Get last health entries from transient and persistent key value stores
      bool transientHealthRecordFound = true;
      if (_transient.has(healthPrefix + serverID)) {
        transist = _transient.hasAsNode(healthPrefix + serverID).value();
      } else {
        // In this case this is the first time we look at this server during our
        // new leadership. So we do not touch the persisted health record and
        // only create a new transient health record.
        transientHealthRecordFound = false;
      }
      if (snapshot().has(healthPrefix + serverID)) {
        persist = snapshot().hasAsNode(healthPrefix + serverID).value();
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

      // in recent versions of ArangoDB, servers can also report back
      // whether they are healthy or not, by sending in the "health"
      // struct with details. currently only DB servers do this, and
      // versions older than 3.8 don't send this at all. so it is an
      // optional attribute, and we cannot rely on it being present.
      // the assumption is thus that servers that do not send back any
      // health data should be considered healthy.
      // TODO: decide on how to exactly handle the "health" info here,
      // and then make use of it. it remains unused for now and is thus
      // specially marked.
      [[maybe_unused]] bool isHealthy = true;

      bool heartbeatVisible = _transient.has(syncPrefix + serverID);
      if (heartbeatVisible) {
        syncTime =
            _transient.hasAsString(syncPrefix + serverID + "/time").value();
        syncStatus =
            _transient.hasAsString(syncPrefix + serverID + "/status").value();
        // it is optional for servers to send health data, so we need to be
        // prepared for not receiving any.
        auto healthData =
            _transient.hasAsBuilder(syncPrefix + serverID + "/health");
        if (healthData) {
          VPackSlice healthSlice = healthData.value().slice();
          if (healthSlice.isObject()) {
            // check health status reported by server
            HealthData hd = HealthData::fromVelocyPack(healthSlice);

            LOG_TOPIC("c77f5", TRACE, Logger::SUPERVISION)
                << "server " << serverID
                << " sent health data: " << healthSlice.toJson()
                << ", ok: " << hd.res.ok()
                << ", message: " << hd.res.errorMessage()
                << ", bg error: " << hd.backgroundError
                << ", free disk bytes: " << hd.freeDiskSpaceBytes
                << ", free disk percent: " << hd.freeDiskSpacePercent;

            if (hd.res.fail()) {
              // server reported itself as unhealthy.
              // TODO: do something about this!
              isHealthy = false;
            }
          }
        }
      } else {
        syncTime = timepointToString(
            std::chrono::system_clock::time_point());  // beginning of time
        syncStatus = "UNKNOWN";
      }

      // Compute the time when we last discovered a new heartbeat from that
      // server:
      std::chrono::system_clock::time_point lastAckedTime;
      if (heartbeatVisible) {
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

      // We have now computed a new transient health record under all
      // circumstances.

      bool changed;
      if (transientHealthRecordFound) {
        // Calculate elapsed since lastAcked
        auto elapsed =
            std::chrono::duration<double>(startTimeLoop - lastAckedTime);

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
        handleOnStatus(_agent, snapshot(), persist, transist, serverID, _jobId,
                       envelope, _DBServersInMaintenance, _delayFailedFollower,
                       _failedLeaderAddsFollower);
        persist =
            transist;  // Now copy Status, SyncStatus from transient to persited
      } else {
        LOG_TOPIC("44253", TRACE, Logger::SUPERVISION)
            << "Health of server " << machine.first << " remains "
            << transist.status;
      }

      // Transient report
      Builder tReport;
      {
        VPackArrayBuilder transaction(&tReport);  // Transist Transaction
        std::shared_ptr<VPackBuilder> envelope;
        {
          VPackObjectBuilder operation(&tReport);  // Operation
          tReport.add(
              VPackValue(healthPrefix + serverID));  // Supervision/Health
          {
            VPackObjectBuilder oo(&tReport);
            transist.toVelocyPack(tReport);
          }
        }
      }  // Transaction

      // Persistent report
      Builder pReport;
      if (changed) {
        {
          VPackArrayBuilder transaction(&pReport);  // Persist Transaction
          {
            VPackObjectBuilder operation(&pReport);  // Operation
            pReport.add(
                VPackValue(healthPrefix + serverID));  // Supervision/Health
            {
              VPackObjectBuilder oo(&pReport);
              persist.toVelocyPack(pReport);
            }
            if (envelope != nullptr) {  // Failed server
              TRI_ASSERT(envelope->slice().isArray() &&
                         envelope->slice()[0].isObject());
              for (auto i :
                   VPackObjectIterator(envelope->slice()[0],
                                       /*useSequentialIteration*/ false)) {
                pReport.add(i.key.stringView(), i.value);
              }
            }
          }  // Operation
          if (envelope != nullptr &&
              envelope->slice().length() > 1) {  // Preconditions(Job)
            TRI_ASSERT(envelope->slice().isArray() &&
                       envelope->slice()[1].isObject());
            pReport.add(envelope->slice()[1]);
          }
        }  // Transaction
      }

      if (!this->isStopping()) {
        // Replicate special event and only then transient store
        if (changed) {
          write_ret_t res = singleWriteTransaction(_agent, pReport, false);
          if (res.accepted && res.indices.front() != 0) {
            ++_jobId;  // Job was booked
            transient(_agent, tReport);
          }
        } else {  // Nothing special just transient store
          transient(_agent, tReport);
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
  VPackBuilder dbserversB = snapshot().get(pdbpath).value().get().toBuilder();
  VPackSlice dbservers = dbserversB.slice();

  if (!snapshot().has(pcpath)) {
    LOG_TOPIC("b0e08", DEBUG, Logger::SUPERVISION)
        << "No Plan/Coordinators key in persistent store";
    return false;
  }
  VPackBuilder coordinatorsB = snapshot().get(pcpath).value().get().toBuilder();
  VPackSlice coordinators = coordinatorsB.slice();

  if (!_transient.has(tpath)) {
    LOG_TOPIC("fe42a", DEBUG, Logger::SUPERVISION)
        << "No Sync/ServerStates key in transient store";
    return false;
  }
  VPackBuilder serverStatesB = _transient.get(tpath).value().get().toBuilder();
  VPackSlice serverStates = serverStatesB.slice();

  // every db server in plan accounted for in transient store?
  for (auto server :
       VPackObjectIterator(dbservers, /*useSequentialIteration*/ true)) {
    auto serverId = server.key.stringView();
    if (!serverStates.hasKey(serverId)) {
      return false;
    }
  }
  // every db server in plan accounted for in transient store?
  for (auto server :
       VPackObjectIterator(coordinators, /*useSequentialIteration*/ true)) {
    auto serverId = server.key.stringView();
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
    if (!logs.empty() &&
        !(logs.size() == 1 && _lastUpdateIndex == logs.front().index)) {
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

  {  // Do I have to report to agency under
    _lock.assertLockedByCurrentThread();
    if (auto modeString = snapshot().hasAsString("/Supervision/State/Mode");
        !modeString || modeString.value() != status) {
      // This includes the case that the mode is not set, since status
      // is never empty
      persist = true;
    }
  }

  VPackBuilder report;
  {
    VPackArrayBuilder trx(&report);
    {
      VPackObjectBuilder br(&report);
      report.add(VPackValue("/Supervision/State"));
      {
        VPackObjectBuilder bbr(&report);
        report.add("Mode", VPackValue(status));
        report.add(
            "Timestamp",
            VPackValue(timepointToString(std::chrono::system_clock::now())));
      }
    }
  }

  // Important! No reporting in transient for Maintenance mode.
  if (status != "Maintenance") {
    transient(_agent, report);
  }

  if (persist) {
    write_ret_t res = singleWriteTransaction(_agent, report, false);
  }
}

void Supervision::updateDBServerMaintenance() {
  // This method checks all entries in /arango/Target/MaintenanceDBServers
  // and makes sure that /arango/Current/MaintenanceDBServers reflects
  // the state of the Target entries (including potentially run out
  // timeouts. Furthermore, it updates the set _DBServersInMaintenance,
  // which will be used for the rest of the supervision run.

  // Algorithm for each entry in Target:
  //   If Target says maintenance:
  //     if timeout reached:
  //       remove entry from Target
  //       stop
  //     else:
  //       if Current entry differs: copy over
  //       put server in _DBServersInMaintenance
  // Algorithm for each entry in Current:
  //   If Current says maintenance:
  //     if server not in _DBServersInMaintenance:
  //       remove entry

  velocypack::Builder builder;
  builder.openArray();

  auto ensureDBServerInCurrent = [&](std::string const& serverId, bool yes) {
    // This closure will copy the entry
    // /Target/MaintenanceDBServers/<serverId>
    // to /Current/MaintenanceDBServers/<serverId> if they differ and `yes`
    // is `true`. If `yes` is `false`, the entry will be removed.
    std::string targetPath =
        std::string(TARGET_MAINTENANCE_DBSERVERS) + "/" + serverId;
    std::string currentPath =
        std::string(CURRENT_MAINTENANCE_DBSERVERS) + "/" + serverId;
    auto current = snapshot().has(currentPath);
    if (yes == false) {
      if (current) {
        {
          VPackArrayBuilder ab(&builder);
          {
            VPackObjectBuilder ob(&builder);
            builder.add(VPackValue("/arango/" + currentPath));
            {
              VPackObjectBuilder ob2(&builder);
              builder.add("op", "delete");
            }
          }
        }
      }
      return;
    }
    auto target = snapshot().get(targetPath);
    if (target.has_value()) {
      if (!current || target.has_value() != current) {
        {
          VPackArrayBuilder ab(&builder);
          {
            VPackObjectBuilder ob(&builder);
            builder.add(VPackValue("/arango/" + currentPath));
            {
              VPackObjectBuilder ob2(&builder);
              builder.add("op", "set");
              builder.add(VPackValue("new"));
              target->get().toBuilder(builder);
            }
          }
        }
      }
    }
  };

  std::unordered_set<std::string> newDBServersInMaintenance;
  auto target = snapshot().hasAsChildren(TARGET_MAINTENANCE_DBSERVERS);
  if (target) {
    // If the key is not there or there is no object there, nobody is in
    // maintenance.
    Node::Children const& targetServers = target.value().get();
    for (auto const& p : targetServers) {
      std::string const& serverId = p.first;
      std::shared_ptr<Node> const& entry = p.second;
      auto mode = entry->hasAsString("Mode");
      if (mode) {
        std::string const& modeSt = mode.value();
        if (modeSt == "maintenance") {
          // Yes, it says maintenance, now check the timeout:
          auto timeout = entry->hasAsString("Until");
          if (timeout) {
            auto const maintenanceExpires = stringToTimepoint(timeout.value());
            if (maintenanceExpires < std::chrono::system_clock::now()) {
              // Need to switch off maintenance mode
              ensureDBServerInCurrent(serverId, false);
            } else {
              // Server is in maintenance mode:
              newDBServersInMaintenance.insert(serverId);
              ensureDBServerInCurrent(serverId, true);
            }
          }
        }
      }
    }
  }
  auto current = snapshot().hasAsChildren(CURRENT_MAINTENANCE_DBSERVERS);
  if (current) {
    Node::Children const& currentServers = current.value().get();
    for (auto const& p : currentServers) {
      std::string const& serverId = p.first;
      if (newDBServersInMaintenance.find(serverId) ==
          newDBServersInMaintenance.end()) {
        ensureDBServerInCurrent(serverId, false);
      }
    }
  }
  _DBServersInMaintenance.swap(newDBServersInMaintenance);

  builder.close();
  if (builder.slice().length() > 0) {
    write_ret_t res = _agent->write(builder.slice());
    if (!res.successful()) {
      LOG_TOPIC("2d6fa", WARN, Logger::SUPERVISION)
          << "failed to update maintenance servers in agency. Will retry. "
          << builder.toJson();
    }
  }
}

void Supervision::step() {
  _lock.assertLockedByCurrentThread();
  if (_jobId == 0 || _jobId == _jobIdMax) {
    getUniqueIds();  // cannot fail but only hang
  }
  LOG_TOPIC("edeee", TRACE, Logger::SUPERVISION) << "Begin updateSnapshot";
  updateSnapshot();
  LOG_TOPIC("aaabb", TRACE, Logger::SUPERVISION) << "Finished updateSnapshot";

  if (!_upgraded) {
    upgradeAgency();
  }

  bool maintenanceMode = false;
  if (snapshot().has(supervisionMaintenance)) {
    try {
      if (snapshot().get(supervisionMaintenance).value().get().isString()) {
        std::string tmp = snapshot()
                              .get(supervisionMaintenance)
                              .value()
                              .get()
                              .getString()
                              .value();
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
          << "Supervision maintenance key in agency is not a string. "
             "This should never happen and will prevent hot backups. "
          << e.what();
      return;
    }
  }

  if (!maintenanceMode) {
    reportStatus("Normal");

    _haveAborts = false;

    // We now need to check for any changes in DBServer maintenance modes.
    // Note that if we confirm a switch to maintenance mode from
    // /arango/Target/MaintenanceDBServers in
    // /arango/Current/MaintenanceDBServers, then this maintenance mode
    // must already count for **this** run of the supervision. Therefore,
    // the following function not only updates the actual place in Current,
    // but also computes the list of DBServers in maintenance mode in
    // _DBServersInMaintenance, which can then be used in the rest of the
    // checks.
    updateDBServerMaintenance();

    if (_agent->leaderFor() > 55 || earlyBird()) {
      // 55 seconds is less than a minute, which fits to the
      // 60 seconds timeout in /_admin/cluster/health

      try {
        LOG_TOPIC("aa565", TRACE, Logger::SUPERVISION) << "Begin doChecks";
        doChecks();
        LOG_TOPIC("675fc", TRACE, Logger::SUPERVISION) << "Finished doChecks";
      } catch (std::exception const& e) {
        LOG_TOPIC("e0869", ERR, Logger::SUPERVISION) << e.what();
      } catch (...) {
        LOG_TOPIC("ac4c4", ERR, Logger::SUPERVISION)
            << "Supervision::doChecks() generated an uncaught "
               "exception.";
      }

      // wait 5 min or until next scheduled run
      if (_agent->leaderFor() > 300 &&
          _nextServerCleanup < std::chrono::system_clock::now()) {
        // Make sure that we have the latest and greatest information
        // about heartbeats in _transient. Note that after a long
        // Maintenance mode of the supervision, the `doChecks` above
        // might have updated /arango/Supervision/Health in the
        // transient store *just now above*. We need to reflect these
        // changes in _transient.
        _agent->executeTransientLocked([&]() {
          if (_agent->transient().has(_agencyPrefix)) {
            _transient = _agent->transient().get(_agencyPrefix);
          }
        });

        LOG_TOPIC("dcded", TRACE, Logger::SUPERVISION)
            << "Begin cleanupExpiredServers";
        cleanupExpiredServers(snapshot(), _transient);
        LOG_TOPIC("dedcd", TRACE, Logger::SUPERVISION)
            << "Finished cleanupExpiredServers";
      }

    } else {
      LOG_TOPIC("7928f", INFO, Logger::SUPERVISION)
          << "Postponing supervision for now, waiting for incoming "
             "heartbeats: "
          << _agent->leaderFor();
    }
    try {
      LOG_TOPIC("7895a", TRACE, Logger::SUPERVISION) << "Begin handleJobs";
      handleJobs();
      LOG_TOPIC("febbc", TRACE, Logger::SUPERVISION) << "Finished handleJobs";
    } catch (std::exception const& e) {
      LOG_TOPIC("76123", WARN, Logger::SUPERVISION)
          << "Caught exception in handleJobs(), error message: " << e.what();
    }
  } else {
    reportStatus("Maintenance");
  }
}

void Supervision::waitForIndexCommitted(index_t index) {
  if (index != 0) {
    auto wait_for_repl_start = std::chrono::steady_clock::now();

    while (!this->isStopping() && _agent->leading()) {
      auto result = _agent->waitFor(index);
      if (result == Agent::raft_commit_t::TIMEOUT) {  // Oh snap
        // Note that we can get UNKNOWN if we have lost leadership or
        // if we are shutting down. In both cases we just leave the
        // loop.
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
  }
}

void Supervision::notify() noexcept {
  {
    CONDITION_LOCKER(guard, _cv);
    _shouldRunAgain = true;
  }
  _cv.signal();
}

void Supervision::waitForSupervisionNode() {
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
}

void Supervision::run() {
  // wait for cluster bootstrap
  waitForSupervisionNode();

  bool shutdown = false;
  {
    CONDITION_LOCKER(guard, _cv);
    TRI_ASSERT(_agent != nullptr);

    while (!this->isStopping()) {
      _shouldRunAgain =
          false;  // we start running, no reason to run again, yet.
      try {
        auto lapStart = std::chrono::steady_clock::now();
        {
          guard.unlock();
          ScopeGuard scopeGuard([&]() noexcept { guard.lock(); });

          {
            MUTEX_LOCKER(locker, _lock);

            // Only modifiy this condition with extreme care:
            // Supervision needs to wait until the agent has finished leadership
            // preparation or else the local agency snapshot might be behind its
            // last state.
            if (_agent->leading() && _agent->getPrepareLeadership() == 0) {
              step();
            } else {
              // Once we lose leadership, we need to restart building our
              // snapshot
              if (_lastUpdateIndex > 0) {
                _lastUpdateIndex = 0;
              }
            }
          }

          // If anything was rafted, we need to wait until it is replicated,
          // otherwise it is not "committed" in the Raft sense. However, let's
          // only wait for our changes not for new ones coming in during the
          // wait.
          if (_agent->leading()) {
            waitForIndexCommitted(_agent->index());
          }
        }
        auto lapTime = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now() - lapStart)
                           .count();

        _supervision_runtime_msec.count(lapTime / 1000);

        if (!_shouldRunAgain && lapTime < 1000000) {
          // wait returns false if timeout was reached
          _cv.wait(static_cast<uint64_t>((1000000 - lapTime) * _frequency));
        }
      } catch (std::exception const& ex) {
        LOG_TOPIC("f5af1", ERR, Logger::SUPERVISION)
            << "caught exception in supervision thread: " << ex.what();
        // continue without throwing
      } catch (...) {
        LOG_TOPIC("c82bb", ERR, Logger::SUPERVISION)
            << "caught unknown exception in supervision thread";
        // continue without throwing
      }
    }
  }

  if (shutdown) {
    _server.beginShutdown();
  }
}

std::string Supervision::serverHealthFunctional(Node const& snapshot,
                                                std::string const& serverName) {
  std::string const serverStatus(healthPrefix + serverName + "/Status");
  return (snapshot.has(serverStatus))
             ? snapshot.hasAsString(serverStatus).value()
             : std::string();
}

// Guarded by caller
std::string Supervision::serverHealth(std::string const& serverName) {
  _lock.assertLockedByCurrentThread();
  return Supervision::serverHealthFunctional(snapshot(), serverName);
}

//  Get all planned servers
//  If heartbeat in transient too old or missing
//    If heartbeat in snapshot older than 1 day
//      Remove coordinator everywhere
//      Remove DB server everywhere, if not leader of a shard

std::unordered_map<ServerID, std::string> deletionCandidates(
    Node const& snapshot, Node const& transient, std::string const& type) {
  using namespace std::chrono;
  std::unordered_map<ServerID, std::string> serverList;
  std::string const planPath = "/Plan/" + type;

  if (snapshot.has(planPath) &&
      !snapshot.get(planPath).value().get().children().empty()) {
    std::string persistedTimeStamp;

    for (auto const& serverId :
         snapshot.get(planPath).value().get().children()) {
      auto const& transientHeartbeat =
          transient.hasAsNode("/Supervision/Health/" + serverId.first);
      try {
        // Do we have a transient heartbeat younger than a day?
        if (transientHeartbeat) {
          auto const t = stringToTimepoint(transientHeartbeat->get()
                                               .get("Timestamp")
                                               .value()
                                               .get()
                                               .getString()
                                               .value());
          if (t > system_clock::now() - hours(24)) {
            continue;
          }
        }
        // Else do we have a persistent heartbeat younger than a day?
        auto const& persistentHeartbeat =
            snapshot.hasAsNode("/Supervision/Health/" + serverId.first);
        if (persistentHeartbeat) {
          persistedTimeStamp = persistentHeartbeat->get()
                                   .get("Timestamp")
                                   .value()
                                   .get()
                                   .getString()
                                   .value();
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
      for (auto const& database :
           snapshot.get("Plan/Collections").value().get().children()) {
        for (auto const& collection : database.second->children()) {
          for (auto const& shard :
               (*collection.second).get("shards").value().get().children()) {
            Slice const servers = (*shard.second).getArray().value();
            if (servers.length() > 0) {
              try {
                for (auto const& server : VPackArrayIterator(servers)) {
                  if (serverList.find(server.copyString()) !=
                      serverList.end()) {
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

void Supervision::cleanupExpiredServers(Node const& snapshot,
                                        Node const& transient) {
  auto servers = deletionCandidates(snapshot, transient, "DBServers");
  auto const& currentDatabases =
      snapshot.get("Current/Databases").value().get();

  VPackBuilder del;
  {
    VPackObjectBuilder d(&del);
    del.add("op", VPackValue("delete"));
  }

  velocypack::Builder trxs;
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
          trxs.add("/arango/Target/MapUniqueToShortID/" + serverName,
                   del.slice());
          trxs.add("/arango/Current/ServersKnown/" + serverName, del.slice());
          trxs.add("/arango/Current/ServersRegistered/" + serverName,
                   del.slice());
          for (auto const& j : currentDatabases.children()) {
            trxs.add("/arango/Current/Databases/" + j.first + "/" + serverName,
                     del.slice());
          }
        }
        if (!server.second
                 .empty()) {  // Timestamp unchanged only, if persistent entry
          VPackObjectBuilder prec(&trxs);
          trxs.add("/arango/Supervision/Health/" + serverName + "/Timestamp",
                   VPackValue(server.second));
        }
      }
    }
  }
  if (servers.size() > 0) {
    _nextServerCleanup =
        std::chrono::system_clock::now() + std::chrono::seconds(3600);
    _agent->write(trxs.slice());
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
          trxs.add("/arango/Target/MapUniqueToShortID/" + serverName,
                   del.slice());
          trxs.add("/arango/Current/ServersKnown/" + serverName, del.slice());
          trxs.add("/arango/Current/ServersRegistered/" + serverName,
                   del.slice());
        }
        if (!server.second
                 .empty()) {  // Timestamp unchanged only, if persistent entry
          VPackObjectBuilder prec(&trxs);
          trxs.add("/arango/Supervision/Health/" + serverName + "/Timestamp",
                   VPackValue(server.second));
        }
      }
    }
  }
  if (servers.size() > 0) {
    _agent->write(trxs.slice());
  }
  _nextServerCleanup =
      std::chrono::system_clock::now() + std::chrono::seconds(3600);
}

void Supervision::cleanupLostCollections(Node const& snapshot,
                                         AgentInterface* agent,
                                         uint64_t& jobId) {
  // Search for failed server
  //  Could also use `Target/FailedServers`
  auto const& health = snapshot.hasAsChildren(healthPrefix);
  if (!health) {
    return;
  }

  std::unordered_set<std::string> failedServers;
  for (auto const& server : health->get()) {
    HealthRecord record(*server.second.get());

    if (record.status == Supervision::HEALTH_STATUS_FAILED) {
      failedServers.insert(server.first);
    }
  }

  if (failedServers.empty()) {
    return;
  }

  // Now iterate over all shards and look for failed leaders.
  auto const& collections = snapshot.hasAsChildren("/Current/Collections");
  if (!collections) {
    return;
  }

  velocypack::Builder builder;
  {
    VPackArrayBuilder trxs(&builder);

    for (auto const& database : collections->get()) {
      auto const& dbname = database.first;

      auto const& collections = database.second->children();

      for (auto const& collection : collections) {
        auto const& colname = collection.first;

        for (auto const& shard : collection.second->children()) {
          auto servers = shard.second->hasAsArray("servers").value();

          TRI_ASSERT(servers.isArray());

          if (servers.length() >= 1) {
            TRI_ASSERT(servers[0].isString());
            auto const& servername = servers[0].copyString();

            if (failedServers.find(servername) != failedServers.end()) {
              // potentially lost shard
              auto const& shardname = shard.first;

              auto const& planurlinsnapshot = "/Plan/Collections/" + dbname +
                                              "/" + colname + "/shards/" +
                                              shardname;

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
                VPackArrayBuilder trx(&builder);
                {
                  VPackObjectBuilder update(&builder);
                  // remove the shard in current
                  builder.add(VPackValue(currenturl));
                  {
                    VPackObjectBuilder op(&builder);
                    builder.add("op", VPackValue("delete"));
                  }
                  // add a job done entry to "Target/Finished"
                  std::string jobIdStr = std::to_string(jobId++);
                  builder.add(
                      VPackValue("/arango/Target/Finished/" + jobIdStr));
                  {
                    VPackObjectBuilder op(&builder);
                    builder.add("op", VPackValue("set"));
                    builder.add(VPackValue("new"));
                    {
                      VPackObjectBuilder job(&builder);
                      builder.add("type", VPackValue("cleanUpLostCollection"));
                      builder.add("server", VPackValue(shardname));
                      builder.add("jobId", VPackValue(jobIdStr));
                      builder.add("creator", VPackValue("supervision"));
                      builder.add("timeCreated",
                                  VPackValue(timepointToString(
                                      std::chrono::system_clock::now())));
                    }
                  }
                  // increase current version
                  builder.add(VPackValue("/arango/Current/Version"));
                  {
                    VPackObjectBuilder op(&builder);
                    builder.add("op", VPackValue("increment"));
                  }
                }
                {
                  VPackObjectBuilder precon(&builder);
                  // pre condition:
                  //  still in current
                  //  not in plan
                  //  still failed
                  builder.add(VPackValue(planurl));
                  {
                    VPackObjectBuilder cond(&builder);
                    builder.add("oldEmpty", VPackValue(true));
                  }
                  builder.add(VPackValue(currenturl));
                  {
                    VPackObjectBuilder cond(&builder);
                    builder.add("oldEmpty", VPackValue(false));
                  }
                  builder.add(VPackValue(healthurl));
                  {
                    VPackObjectBuilder cond(&builder);
                    builder.add("old",
                                VPackValue(Supervision::HEALTH_STATUS_FAILED));
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  if (builder.slice().length() > 0) {
    // do it! fire and forget!
    agent->write(builder.slice());
  }
}

// Remove expired hot backup lock if exists
void Supervision::unlockHotBackup() {
  _lock.assertLockedByCurrentThread();
  if (snapshot().has(HOTBACKUP_KEY)) {
    Node const& tmp = snapshot().get(HOTBACKUP_KEY).value().get();
    if (tmp.isString()) {
      if (std::chrono::system_clock::now() >
          stringToTimepoint(tmp.getString().value())) {
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
void Supervision::handleJobs() {
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
  // is static for unit testing purposes.

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

  LOG_TOPIC("2cd7b", TRACE, Logger::SUPERVISION)
      << "Begin checkUndoLeaderChangeActions";
  checkUndoLeaderChangeActions();

  LOG_TOPIC("f7aa5", TRACE, Logger::SUPERVISION)
      << "Begin checkReplicatedStates";
  checkReplicatedStates();
  updateSnapshot();  // make updates from replicated state visible

  LOG_TOPIC("f7d05", TRACE, Logger::SUPERVISION) << "Begin checkReplicatedLogs";
  checkReplicatedLogs();

  LOG_TOPIC("83676", TRACE, Logger::SUPERVISION)
      << "Begin cleanupReplicatedLogs";
  cleanupReplicatedLogs();  // TODO do this only every x seconds?
  cleanupReplicatedStates();

  LOG_TOPIC("00aab", TRACE, Logger::SUPERVISION) << "Begin workJobs";
  workJobs();

  LOG_TOPIC("0892b", TRACE, Logger::SUPERVISION)
      << "Begin cleanupFinishedAndFailedJobs";
  cleanupFinishedAndFailedJobs();

  LOG_TOPIC("0892c", TRACE, Logger::SUPERVISION)
      << "Begin cleanupHotbackupTransferJobs";
  cleanupHotbackupTransferJobs();

  LOG_TOPIC("0892d", TRACE, Logger::SUPERVISION)
      << "Begin failBrokenHotbackupTransferJobs";
  failBrokenHotbackupTransferJobs();
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
    auto const& jobs = snapshot().hasAsChildren(prefix).value().get();
    if (jobs.size() <= 2 * limit) {
      return;
    }
    typedef std::pair<std::string, std::string> keyDate;
    std::vector<keyDate> v;
    v.reserve(jobs.size());
    for (auto const& p : jobs) {
      auto created = p.second->hasAsString("timeCreated");
      if (created) {
        v.emplace_back(p.first, *created);
      } else {
        v.emplace_back(p.first, "1970");  // will be sorted very early
      }
    }
    std::sort(v.begin(), v.end(),
              [](keyDate const& a, keyDate const& b) -> bool {
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
void arangodb::consensus::cleanupHotbackupTransferJobsFunctional(
    Node const& snapshot, std::shared_ptr<VPackBuilder> envelope) {
  // This deletes old Hotbackup transfer jobs in
  // /Target/HotBackup/TransferJobs according to their time stamp.
  // We keep at most 100 transfer jobs which are completed.
  // We also delete old hotbackup transfer job locks if needed.
  constexpr uint64_t maximalNumberTransferJobs = 100;
  constexpr char const* prefix = HOTBACKUP_TRANSFER_JOBS;

  auto static const noJobs = Node::Children{};
  auto const jobs = snapshot.hasAsChildren(prefix).value_or(noJobs).get();
  auto locks = snapshot.hasAsChildren(HOTBACKUP_TRANSFER_LOCKS);
  bool locksFound = locks && locks.value().get().size() > 0;

  if (jobs.size() <= maximalNumberTransferJobs + 6 && !locksFound) {
    // We tolerate some more jobs before we take action. This
    // is to avoid that we go through all jobs every second. Oasis takes
    // a hotbackup every 2h, so this number 6 would lead to the list
    // being traversed approximately every 12h.
    // The locks are cleaned up if no jobs is ongoing and no cleanup is needed
    // for the jobs. To get to this code, we enter it every 60th time, even
    // if there is no need for it from the perspective of the transfer jobs.
    return;
  }
  typedef std::pair<std::string, std::string> keyDate;
  std::vector<keyDate> v;
  v.reserve(jobs.size());
  bool foundOngoing = false;
  for (auto const& p : jobs) {
    auto const& dbservers = p.second->hasAsChildren("DBServers");
    if (!dbservers) {
      continue;
    }
    // Now check if everything is completed or failed, or if the job
    // has no status information whatsoever:
    bool completed = true;
    for (auto const& pp : dbservers->get()) {
      auto const& status = pp.second->hasAsString("Status");
      if (status) {
        if (status->compare("COMPLETED") != 0 &&
            status->compare("FAILED") != 0) {
          // If we get here, we might be looking at an old leftover which
          // appears to be ongoing, or at a new style, properly ongoing
          // We ignore non-completedness of old crud and only consider
          // new jobs with a rebootId as incomplete:
          auto const& rebootId = pp.second->hasAsUInt(StaticStrings::RebootId);
          if (status.value().compare("NEW") == 0 || rebootId) {
            completed = false;
          }
        }
      }
    }
    if (completed) {
      auto created = p.second->hasAsString("Timestamp");
      if (created) {
        v.emplace_back(p.first, *created);
      } else {
        v.emplace_back(p.first, "1970");  // will be sorted very early
      }
    } else {
      foundOngoing = true;
    }
  }
  std::sort(v.begin(), v.end(), [](keyDate const& a, keyDate const& b) -> bool {
    return a.second < b.second;
  });
  if (v.size() <= maximalNumberTransferJobs) {
    // Now check if anything was ongoing, if not, then we cleanup all locks
    // in /arango/Target/Hotbackup/Transfers, provided nothing has changed
    // with the transfer jobs:
    if (!foundOngoing) {
      VPackArrayBuilder guard(envelope.get());
      {
        // mutation part:
        VPackObjectBuilder guard2(envelope.get());
        envelope->add(VPackValue(HOTBACKUP_TRANSFER_LOCKS));
        {
          VPackObjectBuilder guard3(envelope.get());
          envelope->add("op", VPackValue("set"));
          envelope->add(VPackValue("new"));
          { VPackObjectBuilder guard4(envelope.get()); }
        }
      }
      {
        // precondition part:
        VPackObjectBuilder guard2(envelope.get());
        envelope->add(VPackValue(HOTBACKUP_TRANSFER_JOBS));
        {
          VPackObjectBuilder guard3(envelope.get());
          envelope->add(VPackValue("old"));
          auto oldJobs = snapshot.hasAsNode(HOTBACKUP_TRANSFER_JOBS);
          TRI_ASSERT(oldJobs);
          oldJobs.value().get().toBuilder(*envelope);
        }
      }
    }
    return;
  }
  size_t toBeDeleted =
      v.size() - maximalNumberTransferJobs;  // known to be positive
  LOG_TOPIC("98452", INFO, Logger::AGENCY) << "Deleting " << toBeDeleted
                                           << " old transfer jobs"
                                              " in "
                                           << prefix;
  // We build a single transaction here
  {
    VPackArrayBuilder guard(envelope.get());
    {
      VPackObjectBuilder guard2(envelope.get());
      for (auto it = v.begin(); toBeDeleted-- > 0 && it != v.end(); ++it) {
        envelope->add(VPackValue(prefix + it->first));
        {
          VPackObjectBuilder guard2(envelope.get());
          envelope->add("op", VPackValue("delete"));
        }
      }
    }
  }
}

// Guarded by caller
void arangodb::consensus::failBrokenHotbackupTransferJobsFunctional(
    Node const& snapshot, std::shared_ptr<VPackBuilder> envelope) {
  // This observes the hotbackup transfer jobs and declares those as failed
  // whose responsible dbservers have crashed or have been shut down.
  VPackArrayBuilder guard(envelope.get());
  constexpr char const* prefix = HOTBACKUP_TRANSFER_JOBS;
  auto const& jobs = snapshot.hasAsChildren(prefix);
  if (jobs) {
    for (auto const& p : jobs.value().get()) {
      auto const& dbservers = p.second->hasAsChildren("DBServers");
      if (!dbservers) {
        continue;
      }
      for (auto const& pp : dbservers.value().get()) {
        auto const& status = pp.second->hasAsString("Status");
        if (!status) {
          // Should not happen, just be cautious
          continue;
        }
        if (status.value().compare("COMPLETED") == 0 ||
            status.value().compare("FAILED") == 0 ||
            status.value().compare("CANCELLED") == 0) {
          // Nothing to do
          continue;
        }
        bool found = false;
        auto const& rebootId = pp.second->hasAsUInt(StaticStrings::RebootId);
        auto const& lockLocation =
            pp.second->hasAsString(StaticStrings::LockLocation);
        if (rebootId && lockLocation) {
          if (!Supervision::verifyServerRebootID(snapshot, pp.first,
                                                 rebootId.value(), found)) {
            // Cancel job, set status to FAILED and release lock:
            VPackArrayBuilder guard(envelope.get());
            // Action part:
            {
              VPackObjectBuilder guard2(envelope.get());
              envelope->add(VPackValue(prefix + p.first + "/DBServers/" +
                                       pp.first + "/Status"));
              {
                VPackObjectBuilder guard(envelope.get());
                envelope->add("op", VPackValue("set"));
                envelope->add("new", VPackValue("FAILED"));
              }
              envelope->add(VPackValue(std::string(HOTBACKUP_TRANSFER_LOCKS) +
                                       lockLocation.value()));
              {
                VPackObjectBuilder guard(envelope.get());
                envelope->add("op", VPackValue("delete"));
              }
            }
            // Precondition part: status is unchanged
            // This guards us against the case that a DBserver has finished a
            // job and was then shut down since we last renewed our snapshot.
            {
              VPackObjectBuilder guard2(envelope.get());
              envelope->add(
                  prefix + p.first + "/DBServers/" + pp.first + "/Status",
                  VPackValue(status.value()));
            }
          }
        }
      }
    }
  }
}

void Supervision::cleanupHotbackupTransferJobs() {
  _lock.assertLockedByCurrentThread();

  auto envelope = std::make_shared<VPackBuilder>();
  arangodb::consensus::cleanupHotbackupTransferJobsFunctional(snapshot(),
                                                              envelope);
  if (envelope->size() > 0) {
    write_ret_t res = singleWriteTransaction(_agent, *envelope, false);

    if (!res.accepted || (res.indices.size() == 1 && res.indices[0] == 0)) {
      LOG_TOPIC("1232b", INFO, Logger::SUPERVISION)
          << "Failed to remove old transfer jobs or locks: "
          << envelope->toJson();
    }
  }
}

void Supervision::failBrokenHotbackupTransferJobs() {
  _lock.assertLockedByCurrentThread();

  auto envelope = std::make_shared<VPackBuilder>();
  arangodb::consensus::failBrokenHotbackupTransferJobsFunctional(snapshot(),
                                                                 envelope);
  if (envelope->slice().length() > 0) {
    trans_ret_t res = generalTransaction(_agent, *envelope);

    bool good = true;
    VPackSlice resSlice = res.result->slice();
    if (res.accepted) {
      if (!resSlice.isArray()) {
        good = false;
      } else {
        for (size_t i = 0; i < resSlice.length(); ++i) {
          if (!resSlice[i].isNumber()) {
            good = false;
          } else {
            uint64_t j = resSlice[i].getNumber<uint64_t>();
            if (j == 0) {
              good = false;
            }
          }
        }
      }
    }
    if (!good) {
      LOG_TOPIC("1232c", INFO, Logger::SUPERVISION)
          << "Failed to fail bad transfer jobs: " << envelope->toJson()
          << ", response: "
          << (res.accepted ? resSlice.toJson() : std::string());
    }
  }
}

// Guarded by caller
void Supervision::workJobs() {
  _lock.assertLockedByCurrentThread();

  bool dummy = false;
  // ATTENTION: It is necessary to copy the todos here, since we modify
  // below!
  auto todos = snapshot().hasAsChildren(toDoPrefix).value().get();
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
    if (jobNode.hasAsString("type").value().compare(0, FAILED.length(),
                                                    FAILED) == 0) {
      if (selectRandom && RandomGenerator::interval(static_cast<uint64_t>(
                              todos.size())) > maximalJobsPerRound) {
        LOG_TOPIC("675fe", TRACE, Logger::SUPERVISION) << "Skipped ToDo Job";
        ++it;
        continue;
      }

      LOG_TOPIC("87812", TRACE, Logger::SUPERVISION)
          << "Begin JobContext::run()";
      JobContext(TODO, jobNode.hasAsString("jobId").value(), snapshot(), _agent)
          .run(_haveAborts);
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
      if (selectRandom && RandomGenerator::interval(static_cast<uint64_t>(
                              todos.size())) > maximalJobsPerRound) {
        LOG_TOPIC("77889", TRACE, Logger::SUPERVISION) << "Skipped ToDo Job";
        continue;
      }
      auto const& jobNode = *todoEnt.second;
      if (jobNode.hasAsString("type").value().compare(0, FAILED.length(),
                                                      FAILED) != 0) {
        LOG_TOPIC("aa667", TRACE, Logger::SUPERVISION)
            << "Begin JobContext::run()";
        JobContext(TODO, jobNode.hasAsString("jobId").value(), snapshot(),
                   _agent)
            .run(dummy);
        LOG_TOPIC("65bcd", TRACE, Logger::SUPERVISION)
            << "Finish JobContext::run()";
      }
    }
  }
  LOG_TOPIC("a55ce", TRACE, Logger::SUPERVISION)
      << "Updating snapshot after ToDo";
  updateSnapshot();

  LOG_TOPIC("08641", TRACE, Logger::SUPERVISION) << "Begin Pendings";
  auto const& pends = snapshot().hasAsChildren(pendingPrefix).value().get();
  selectRandom = pends.size() > maximalJobsPerRound;

  for (auto const& pendEnt : pends) {
    if (selectRandom && RandomGenerator::interval(static_cast<uint64_t>(
                            pends.size())) > maximalJobsPerRound) {
      LOG_TOPIC("54310", TRACE, Logger::SUPERVISION) << "Skipped Pending Job";
      continue;
    }
    auto const& jobNode = *(pendEnt.second);
    LOG_TOPIC("009ba", TRACE, Logger::SUPERVISION) << "Begin JobContext::run()";
    JobContext(PENDING, jobNode.hasAsString("jobId").value(), snapshot(),
               _agent)
        .run(dummy);
    LOG_TOPIC("99006", TRACE, Logger::SUPERVISION)
        << "Finish JobContext::run()";
  }
}

bool Supervision::verifyServerRebootID(Node const& snapshot,
                                       std::string const& serverID,
                                       uint64_t wantedRebootID,
                                       bool& serverFound) {
  // check if the server exists in health
  std::string const& health = serverHealthFunctional(snapshot, serverID);
  LOG_TOPIC("44432", DEBUG, Logger::SUPERVISION)
      << "verifyServerRebootID: serverID=" << serverID << " health=" << health;

  // if the server is not found, health is an empty string
  serverFound = !health.empty();
  if (health != HEALTH_STATUS_GOOD && health != HEALTH_STATUS_BAD) {
    return false;
  }

  // now lookup reboot id
  std::optional<uint64_t> rebootID = snapshot.hasAsUInt(
      curServersKnown + serverID + "/" + StaticStrings::RebootId);
  LOG_TOPIC("54326", DEBUG, Logger::SUPERVISION)
      << "verifyServerRebootID: rebootId=" << rebootID.value_or(0)
      << " bool=" << rebootID.has_value();
  return rebootID && *rebootID == wantedRebootID;
}

void Supervision::deleteBrokenDatabase(AgentInterface* agent,
                                       std::string const& database,
                                       std::string const& coordinatorID,
                                       uint64_t rebootID,
                                       bool coordinatorFound) {
  velocypack::Builder envelope;
  {
    VPackArrayBuilder trxs(&envelope);
    {
      VPackArrayBuilder trx(&envelope);
      {
        VPackObjectBuilder operation(&envelope);

        // increment Plan Version
        {
          VPackObjectBuilder o(&envelope, _agencyPrefix + "/" + PLAN_VERSION);
          envelope.add("op", VPackValue("increment"));
        }

        // delete the database from Plan/Databases
        {
          VPackObjectBuilder o(&envelope,
                               _agencyPrefix + planDBPrefix + database);
          envelope.add("op", VPackValue("delete"));
        }

        // delete the database from Plan/Collections
        {
          VPackObjectBuilder o(&envelope,
                               _agencyPrefix + planColPrefix + database);
          envelope.add("op", VPackValue("delete"));
        }

        // delete the database from Plan/Analyzers
        {
          VPackObjectBuilder o(&envelope,
                               _agencyPrefix + planAnalyzersPrefix + database);
          envelope.add("op", VPackValue("delete"));
        }
      }
      {
        // precondition that this database is still in Plan and is building
        VPackObjectBuilder preconditions(&envelope);
        auto const databasesPath =
            plan()->databases()->database(database)->str();
        envelope.add(databasesPath + "/" + StaticStrings::AttrIsBuilding,
                     VPackValue(true));
        envelope.add(
            databasesPath + "/" + StaticStrings::AttrCoordinatorRebootId,
            VPackValue(rebootID));
        envelope.add(databasesPath + "/" + StaticStrings::AttrCoordinator,
                     VPackValue(coordinatorID));

        {
          VPackObjectBuilder precondition(
              &envelope, _agencyPrefix + healthPrefix + coordinatorID);
          envelope.add("oldEmpty", VPackValue(!coordinatorFound));
        }
      }
    }
  }

  write_ret_t res = agent->write(envelope.slice());

  if (!res.successful()) {
    LOG_TOPIC("38482", DEBUG, Logger::SUPERVISION)
        << "failed to delete broken database in agency. Will retry "
        << envelope.toJson();
  }
}

void Supervision::deleteBrokenCollection(AgentInterface* agent,
                                         std::string const& database,
                                         std::string const& collection,
                                         std::string const& coordinatorID,
                                         uint64_t rebootID,
                                         bool coordinatorFound) {
  velocypack::Builder envelope;
  {
    VPackArrayBuilder trxs(&envelope);
    {
      std::string collection_path = plan()
                                        ->collections()
                                        ->database(database)
                                        ->collection(collection)
                                        ->str();

      VPackArrayBuilder trx(&envelope);
      {
        VPackObjectBuilder operation(&envelope);
        // increment Plan Version
        {
          VPackObjectBuilder o(&envelope, _agencyPrefix + "/" + PLAN_VERSION);
          envelope.add("op", VPackValue("increment"));
        }
        // delete the collection from Plan/Collections/<db>
        {
          VPackObjectBuilder o(&envelope, collection_path);
          envelope.add("op", VPackValue("delete"));
        }
      }
      {
        // precondition that this collection is still in Plan and is building
        VPackObjectBuilder preconditions(&envelope);
        envelope.add(collection_path + "/" + StaticStrings::AttrIsBuilding,
                     VPackValue(true));
        envelope.add(
            collection_path + "/" + StaticStrings::AttrCoordinatorRebootId,
            VPackValue(rebootID));
        envelope.add(collection_path + "/" + StaticStrings::AttrCoordinator,
                     VPackValue(coordinatorID));

        {
          VPackObjectBuilder precondition(
              &envelope, _agencyPrefix + healthPrefix + "/" + coordinatorID);
          envelope.add("oldEmpty", VPackValue(!coordinatorFound));
        }
      }
    }
  }

  write_ret_t res = agent->write(envelope.slice());

  if (!res.successful()) {
    LOG_TOPIC("38485", DEBUG, Logger::SUPERVISION)
        << "failed to delete broken collection in agency. Will retry. "
        << envelope.toJson();
  }
}

void Supervision::restoreBrokenAnalyzersRevision(
    std::string const& database, AnalyzersRevision::Revision revision,
    AnalyzersRevision::Revision buildingRevision,
    std::string const& coordinatorID, uint64_t rebootID,
    bool coordinatorFound) {
  velocypack::Builder envelope;
  {
    VPackArrayBuilder trxs(&envelope);
    {
      std::string anPath = _agencyPrefix + planAnalyzersPrefix + database + "/";

      VPackArrayBuilder trx(&envelope);
      {
        VPackObjectBuilder operation(&envelope);
        // increment Plan Version
        {
          VPackObjectBuilder o(&envelope, _agencyPrefix + "/" + PLAN_VERSION);
          envelope.add("op", VPackValue("increment"));
        }
        // restore the analyzers revision from Plan/Analyzers/<db>/
        {
          VPackObjectBuilder o(
              &envelope, anPath + StaticStrings::AnalyzersBuildingRevision);
          envelope.add("op", VPackValue("decrement"));
        }
        {
          VPackObjectBuilder o(&envelope,
                               anPath + StaticStrings::AttrCoordinatorRebootId);
          envelope.add("op", VPackValue("delete"));
        }
        {
          VPackObjectBuilder o(&envelope,
                               anPath + StaticStrings::AttrCoordinator);
          envelope.add("op", VPackValue("delete"));
        }
      }
      {
        // precondition that this analyzers revision is still in Plan and is
        // building
        VPackObjectBuilder preconditions(&envelope);
        envelope.add(anPath + StaticStrings::AnalyzersRevision,
                     VPackValue(revision));
        envelope.add(anPath + StaticStrings::AnalyzersBuildingRevision,
                     VPackValue(buildingRevision));
        envelope.add(anPath + StaticStrings::AttrCoordinatorRebootId,
                     VPackValue(rebootID));
        envelope.add(anPath + StaticStrings::AttrCoordinator,
                     VPackValue(coordinatorID));
        {
          VPackObjectBuilder precondition(
              &envelope, _agencyPrefix + healthPrefix + "/" + coordinatorID);
          envelope.add("oldEmpty", VPackValue(!coordinatorFound));
        }
      }
    }
  }

  write_ret_t res = _agent->write(envelope.slice());
  if (!res.successful()) {
    LOG_TOPIC("e43cb", DEBUG, Logger::SUPERVISION)
        << "failed to restore broken analyzers revision in agency. Will retry. "
        << envelope.toJson();
  }
}

void Supervision::resourceCreatorLost(
    std::shared_ptr<Node> const& resource,
    std::function<void(ResourceCreatorLostEvent const&)> const& action) {
  //  check if the coordinator exists and its reboot is the same as specified
  auto rebootID = resource->hasAsUInt(StaticStrings::AttrCoordinatorRebootId);
  auto coordinatorID = resource->hasAsString(StaticStrings::AttrCoordinator);

  bool keepResource = true;
  bool coordinatorFound = false;

  if (rebootID && coordinatorID) {
    keepResource = verifyServerRebootID(snapshot(), *coordinatorID, *rebootID,
                                        coordinatorFound);
    // incomplete data, should not happen
  } else {
    //          v---- Please note this awesome log-id
    LOG_TOPIC("dbbad", WARN, Logger::SUPERVISION)
        << "resource has set `isBuilding` but is missing coordinatorId and "
           "rebootId";
  }

  if (!keepResource) {
    action(ResourceCreatorLostEvent{resource, *coordinatorID, *rebootID,
                                    coordinatorFound});
  }
}

void Supervision::ifResourceCreatorLost(
    std::shared_ptr<Node> const& resource,
    std::function<void(ResourceCreatorLostEvent const&)> const& action) {
  // check if isBuilding is set and it is true
  auto isBuilding = resource->hasAsBool(StaticStrings::AttrIsBuilding);
  if (isBuilding && isBuilding.value()) {
    // this database or collection is currently being built
    resourceCreatorLost(resource, action);
  }
}

void Supervision::checkBrokenCreatedDatabases() {
  _lock.assertLockedByCurrentThread();

  // check if snapshot has databases
  auto databases = snapshot().hasAsNode(planDBPrefix);
  if (!databases) {
    return;
  }

  // dbpair is <std::string, std::shared_ptr<Node>>
  for (auto const& dbpair : databases.value().get().children()) {
    std::shared_ptr<Node> const& db = dbpair.second;

    LOG_TOPIC("24152", DEBUG, Logger::SUPERVISION) << "checkBrokenDbs: " << *db;

    ifResourceCreatorLost(db, [&](ResourceCreatorLostEvent const& ev) {
      LOG_TOPIC("fe522", INFO, Logger::SUPERVISION)
          << "checkBrokenCreatedDatabases: removing skeleton database with "
             "name "
          << dbpair.first;
      // delete this database and all of its collections
      deleteBrokenDatabase(_agent, dbpair.first, ev.coordinatorId,
                           ev.coordinatorRebootId, ev.coordinatorFound);
    });
  }
}

void Supervision::checkBrokenCollections() {
  _lock.assertLockedByCurrentThread();

  // check if snapshot has databases
  auto collections = snapshot().hasAsNode(planColPrefix);
  if (!collections) {
    return;
  }

  // dbpair is <std::string, std::shared_ptr<Node>>
  for (auto const& dbpair : collections.value().get().children()) {
    std::shared_ptr<Node> const& db = dbpair.second;

    for (auto const& collectionPair : db->children()) {
      // collectionPair.first is collection id
      auto collectionNamePair =
          collectionPair.second->hasAsString(StaticStrings::DataSourceName);
      if (!collectionNamePair || collectionNamePair.value().empty() ||
          collectionNamePair.value().front() == '_') {
        continue;
      }

      ifResourceCreatorLost(
          collectionPair.second, [&](ResourceCreatorLostEvent const& ev) {
            LOG_TOPIC("fe523", INFO, Logger::SUPERVISION)
                << "checkBrokenCollections: removing broken collection with "
                   "name "
                << dbpair.first;
            // delete this collection
            deleteBrokenCollection(_agent, dbpair.first, collectionPair.first,
                                   ev.coordinatorId, ev.coordinatorRebootId,
                                   ev.coordinatorFound);
          });

      // also check all indexes of the collection to see if they are abandoned
      if (collectionPair.second->has("indexes")) {
        Slice indexes = collectionPair.second->get("indexes")
                            .value()
                            .get()
                            .getArray()
                            .value();
        // check if the coordinator which started creating this index is
        // still present...
        for (VPackSlice planIndex : VPackArrayIterator(indexes)) {
          if (VPackSlice isBuildingSlice =
                  planIndex.get(StaticStrings::AttrIsBuilding);
              !isBuildingSlice.isTrue()) {
            // we are only interested in indexes that are still building
            continue;
          }

          VPackSlice rebootIDSlice =
              planIndex.get(StaticStrings::AttrCoordinatorRebootId);
          VPackSlice coordinatorIDSlice =
              planIndex.get(StaticStrings::AttrCoordinator);

          if (rebootIDSlice.isNumber() && coordinatorIDSlice.isString()) {
            auto rebootID = rebootIDSlice.getUInt();
            auto coordinatorID = coordinatorIDSlice.copyString();

            bool coordinatorFound = false;
            bool keepResource = verifyServerRebootID(
                snapshot(), coordinatorID, rebootID, coordinatorFound);

            if (!keepResource) {
              // index creation still ongoing, but started by a coordinator that
              // has failed by now. delete this index
              deleteBrokenIndex(_agent, dbpair.first, collectionPair.first,
                                planIndex, coordinatorID, rebootID,
                                coordinatorFound);
            }
          }
        }
      }
    }
  }
}

void Supervision::checkBrokenAnalyzers() {
  _lock.assertLockedByCurrentThread();

  // check if snapshot has analyzers
  auto node = snapshot().hasAsNode(planAnalyzersPrefix);
  if (!node) {
    return;
  }

  for (auto const& dbData : node->get().children()) {
    auto const& revisions = dbData.second;
    auto const revision =
        revisions->hasAsUInt(StaticStrings::AnalyzersRevision);
    auto const buildingRevision =
        revisions->hasAsUInt(StaticStrings::AnalyzersBuildingRevision);
    if (revision && buildingRevision && *revision != *buildingRevision) {
      resourceCreatorLost(
          revisions, [this, &dbData, &revision,
                      &buildingRevision](ResourceCreatorLostEvent const& ev) {
            LOG_TOPIC("ae5a3", INFO, Logger::SUPERVISION)
                << "checkBrokenAnalyzers: fixing broken analyzers revision "
                   "with database name "
                << dbData.first;
            restoreBrokenAnalyzersRevision(
                dbData.first, *revision, *buildingRevision, ev.coordinatorId,
                ev.coordinatorRebootId, ev.coordinatorFound);
          });
    }
  }
}

void Supervision::deleteBrokenIndex(AgentInterface* agent,
                                    std::string const& database,
                                    std::string const& collection,
                                    arangodb::velocypack::Slice index,
                                    std::string const& coordinatorID,
                                    uint64_t rebootID, bool coordinatorFound) {
  VPackBuilder envelope;
  {
    VPackArrayBuilder trxs(&envelope);
    {
      std::string collectionPath = plan()
                                       ->collections()
                                       ->database(database)
                                       ->collection(collection)
                                       ->str();
      std::string indexesPath = plan()
                                    ->collections()
                                    ->database(database)
                                    ->collection(collection)
                                    ->indexes()
                                    ->str();

      VPackArrayBuilder trx(&envelope);
      {
        VPackObjectBuilder operation(&envelope);
        // increment Plan Version
        {
          VPackObjectBuilder o(&envelope, _agencyPrefix + "/" + PLAN_VERSION);
          envelope.add("op", VPackValue("increment"));
        }
        // delete the index from Plan/Collections/<db>/<collection>
        {
          VPackObjectBuilder o(&envelope, indexesPath);
          envelope.add("op", VPackValue("erase"));
          envelope.add("val", index);
        }
      }
      {
        // precondition that the collection is still in Plan
        VPackObjectBuilder preconditions(&envelope);
        {
          VPackObjectBuilder precondition(&envelope, collectionPath);
          envelope.add("oldEmpty", VPackValue(false));
        }
      }
    }
  }

  write_ret_t res = agent->write(envelope.slice());

  if (!res.successful()) {
    LOG_TOPIC("01598", DEBUG, Logger::SUPERVISION)
        << "failed to delete broken index in agency. Will retry. "
        << envelope.toJson();
  }
}

namespace {
template<typename T>
auto parseSomethingFromNode(Node const& n) -> T {
  auto builder = n.toBuilder();
  return velocypack::deserialize<T>(builder.slice());
}

template<typename T>
auto parseIfExists(Node const& root, std::string const& url)
    -> std::optional<T> {
  if (auto node = root.get(url); node.has_value()) {
    return parseSomethingFromNode<T>(node->get());
  }
  return std::nullopt;
}

auto parseReplicatedLogAgency(Node const& root, std::string const& dbName,
                              std::string const& idString)
    -> std::optional<replication2::agency::Log> {
  auto targetPath =
      aliases::target()->replicatedLogs()->database(dbName)->log(idString);
  // first check if target exists
  if (auto targetNode = root.hasAsNode(targetPath->str(SkipComponents(1)));
      targetNode.has_value()) {
    auto log = replication2::agency::Log{
        .target = parseSomethingFromNode<replication2::agency::LogTarget>(
            *targetNode),
        .plan = parseIfExists<LogPlanSpecification>(
            root, aliases::plan()
                      ->replicatedLogs()
                      ->database(dbName)
                      ->log(idString)
                      ->str(SkipComponents(1))),
        .current =
            parseIfExists<LogCurrent>(root, aliases::current()
                                                ->replicatedLogs()
                                                ->database(dbName)
                                                ->log(idString)
                                                ->str(SkipComponents(1)))};
    return log;
  } else {
    return std::nullopt;
  }
}

auto parseReplicatedStateAgency(Node const& root, Node const& targetNode,
                                std::string const& dbName,
                                std::string const& idString)
    -> replication2::replicated_state::agency::State {
  return replication2::replicated_state::agency::State{
      .target = parseSomethingFromNode<
          replication2::replicated_state::agency::Target>(targetNode),
      .plan = parseIfExists<replication2::replicated_state::agency::Plan>(
          root, aliases::plan()
                    ->replicatedStates()
                    ->database(dbName)
                    ->state(idString)
                    ->str(SkipComponents(1))),
      .current = parseIfExists<replication2::replicated_state::agency::Current>(
          root, aliases::current()
                    ->replicatedStates()
                    ->database(dbName)
                    ->state(idString)
                    ->str(SkipComponents(1)))};
}
}  // namespace

namespace {
using namespace replication2::replicated_log;

auto replicatedLogOwnerGone(Node const& snapshot, Node const& node,
                            std::string const& dbName,
                            std::string const& idString) -> bool {
  if (auto owner = node.hasAsString("owner");
      !owner.has_value() || owner != "replicated-state") {
    return false;
  }

  auto const& targetNode = snapshot.hasAsNode(planRepStatePrefix);
  // now check if there is a replicated state in plan with that id
  if (targetNode.has_value() &&
      targetNode->get().has(std::vector{dbName, idString})) {
    return false;
  }
  return true;
}

auto handleReplicatedLog(Node const& snapshot, Node const& targetNode,
                         std::string const& dbName, std::string const& idString,
                         ParticipantsHealth const& health,
                         arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope try {
  if (replicatedLogOwnerGone(snapshot, targetNode, dbName, idString)) {
    auto logId = replication2::LogId{basics::StringUtils::uint64(idString)};
    return methods::deleteReplicatedLogTrx(std::move(envelope), dbName, logId);
  }

  std::optional<replication2::agency::Log> maybeLog;
  try {
    maybeLog = parseReplicatedLogAgency(snapshot, dbName, idString);
  } catch (std::exception const& err) {
    LOG_TOPIC("fe14e", ERR, Logger::REPLICATION2)
        << "Supervision caught exception while parsing replicated log" << dbName
        << "/" << idString << ": " << err.what();
    throw;
  }
  if (maybeLog.has_value()) {
    try {
      auto& log = *maybeLog;
      return replication2::replicated_log::executeCheckReplicatedLog(
          dbName, idString, std::move(log), health, std::move(envelope));
    } catch (std::exception const& err) {
      LOG_TOPIC("b6d7d", ERR, Logger::REPLICATION2)
          << "Supervision caught exception while handling replicated log"
          << dbName << "/" << idString << ": " << err.what();
      throw;
    }
  } else {
    LOG_TOPIC("56a0c", ERR, Logger::REPLICATION2)
        << "Supervision could not parse Target node for replicated log "
        << dbName << "/" << idString;
    return envelope;
  }
} catch (std::exception const& err) {
  LOG_TOPIC("9f7fb", ERR, Logger::REPLICATION2)
      << "Supervision caught exception while working with replicated log"
      << dbName << "/" << idString << ": " << err.what();
  return envelope;
}
}  // namespace

void Supervision::checkReplicatedLogs() {
  _lock.assertLockedByCurrentThread();

  using namespace replication2::agency;

  // check if Target has replicated logs
  auto const& targetNode = snapshot().hasAsNode(targetRepLogPrefix);
  if (!targetNode) {
    return;
  }

  using namespace replication2::replicated_log;

  ParticipantsHealth participantsHealth = std::invoke([&] {
    std::unordered_map<replication2::ParticipantId, ParticipantHealth> info;
    auto& dbservers = snapshot().hasAsChildren(plannedServers).value().get();
    for (auto const& [serverId, node] : dbservers) {
      bool const notIsFailed = (serverHealth(serverId) == HEALTH_STATUS_GOOD) or
                               (serverHealth(serverId) == HEALTH_STATUS_BAD);

      auto rebootID = snapshot().hasAsUInt(basics::StringUtils::concatT(
          curServersKnown, serverId, "/", StaticStrings::RebootId));
      if (rebootID) {
        info.emplace(serverId,
                     ParticipantHealth{RebootId{*rebootID}, notIsFailed});
      }
    }
    return ParticipantsHealth{info};
  });

  velocypack::Builder builder;
  auto envelope = arangodb::agency::envelope::into_builder(builder);

  for (auto const& [dbName, db] : targetNode->get().children()) {
    for (auto const& [idString, node] : db->children()) {
      envelope = handleReplicatedLog(snapshot(), *node, dbName, idString,
                                     participantsHealth, std::move(envelope));
    }
  }

  envelope.done();
  if (builder.slice().length() > 0) {
    write_ret_t res = _agent->write(builder.slice());
    if (!res.successful()) {
      LOG_TOPIC("12d36", WARN, Logger::SUPERVISION)
          << "failed to update term in agency. Will retry. "
          << builder.toJson();
    }
  }
}

namespace {
auto handleReplicatedState(Node const& snapshot, Node const& targetNode,
                           std::string const& dbName,
                           std::string const& idString,
                           arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope try {
  auto log = parseReplicatedLogAgency(snapshot, dbName, idString);
  auto state =
      parseReplicatedStateAgency(snapshot, targetNode, dbName, idString);

  return replication2::replicated_state::executeCheckReplicatedState(
      dbName, std::move(state), std::move(log), std::move(envelope));
} catch (std::exception const& err) {
  LOG_TOPIC("676d1", ERR, Logger::REPLICATION2)
      << "Supervision caught exception while parsing "
         "replicated state "
      << dbName << "/" << idString << ": " << err.what();
  return envelope;
}
}  // namespace

void Supervision::checkReplicatedStates() {
  _lock.assertLockedByCurrentThread();

  using namespace replication2::agency;

  auto targetNode = snapshot().hasAsNode(targetRepStatePrefix);
  if (!targetNode.has_value()) {
    return;
  }

  velocypack::Builder builder;
  auto envelope = arangodb::agency::envelope::into_builder(builder);

  for (auto const& [dbName, db] : targetNode->get().children()) {
    for (auto const& [idString, node] : db->children()) {
      envelope = handleReplicatedState(snapshot(), *node, dbName, idString,
                                       std::move(envelope));
    }
  }

  envelope.done();
  if (builder.slice().length() > 0) {
    write_ret_t res = _agent->write(builder.slice());
    if (!res.successful()) {
      LOG_TOPIC("12e37", WARN, Logger::SUPERVISION)
          << "failed to update term in agency. Will retry. "
          << builder.toJson();
    }
  }
}

void Supervision::readyOrphanedIndexCreations() {
  _lock.assertLockedByCurrentThread();

  if (snapshot().has(planColPrefix) && snapshot().has(curColPrefix)) {
    auto const& plannedDBs =
        snapshot().get(planColPrefix).value().get().children();
    auto const& currentDBs = snapshot().get(curColPrefix).value().get();

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
          indexes = collection.get("indexes").value().get().getArray().value();
          if (indexes.length() > 0) {
            for (auto planIndex : VPackArrayIterator(indexes)) {
              if (planIndex.hasKey(StaticStrings::IndexIsBuilding) &&
                  collection.has("shards")) {
                auto const& planId = planIndex.get("id");
                auto const& shards = collection.get("shards").value().get();
                if (collection.has("numberOfShards") &&
                    collection.get("numberOfShards").value().get().isUInt()) {
                  auto nshards = collection.get("numberOfShards")
                                     .value()
                                     .get()
                                     .getUInt()
                                     .value();
                  if (nshards == 0) {
                    continue;
                  }
                  size_t nIndexes = 0;
                  for (auto const& sh : shards.children()) {
                    auto const& shname = sh.first;

                    if (currentDBs.has(colPath + shname + "/indexes")) {
                      auto curIndexes =
                          currentDBs.get(colPath + shname + "/indexes")
                              .value()
                              .get()
                              .slice();
                      for (auto const& curIndex :
                           VPackArrayIterator(curIndexes)) {
                        VPackSlice errorSlice =
                            curIndex.get(StaticStrings::Error);
                        if (errorSlice.isTrue()) {
                          // index creation for this shard has failed - don't
                          // count it as valid!
                          continue;
                        }

                        auto const& curId = curIndex.get("id");
                        if (basics::VelocyPackHelper::equal(planId, curId,
                                                            false)) {
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

        // We have some indexes, that have been fully built and have their
        // isBuilding attribute still set.
        if (!built.empty()) {
          velocypack::Builder envelope;
          {
            VPackArrayBuilder trxs(&envelope);
            {
              VPackArrayBuilder trx(&envelope);
              {
                VPackObjectBuilder operation(&envelope);
                envelope.add(VPackValue(_agencyPrefix + "/" + PLAN_VERSION));
                {
                  VPackObjectBuilder o(&envelope);
                  envelope.add("op", VPackValue("increment"));
                }
                envelope.add(VPackValue(_agencyPrefix + planColPrefix +
                                        colPath + "indexes"));
                VPackArrayBuilder value(&envelope);
                for (auto planIndex : VPackArrayIterator(indexes)) {
                  if (built.find(planIndex.get("id").copyString()) !=
                      built.end()) {
                    {
                      VPackObjectBuilder props(&envelope);
                      for (auto prop : VPackObjectIterator(
                               planIndex, /*useSequentialIteration*/ true)) {
                        auto key = prop.key.stringView();
                        if (key != StaticStrings::IndexIsBuilding &&
                            key != StaticStrings::AttrCoordinator &&
                            key != StaticStrings::AttrCoordinatorRebootId) {
                          envelope.add(key, prop.value);
                        }
                      }
                    }
                  } else {
                    envelope.add(planIndex);
                  }
                }
              }
              {
                VPackObjectBuilder precondition(&envelope);
                envelope.add(VPackValue(_agencyPrefix + planColPrefix +
                                        colPath + "indexes"));
                envelope.add(indexes);
              }
            }
          }

          write_ret_t res = _agent->write(envelope.slice());
          if (!res.successful()) {
            LOG_TOPIC("3848f", DEBUG, Logger::SUPERVISION)
                << "failed to report ready index to agency. Will retry.";
          }
        }
      }
    }
  }
}

void Supervision::cleanupReplicatedLogs() {
  _lock.assertLockedByCurrentThread();

  using namespace replication2::agency;

  // check if Plan has replicated logs
  auto const& planNode = snapshot().hasAsNode(planRepLogPrefix);
  if (!planNode) {
    return;
  }

  auto const& targetNode = snapshot().hasAsNode(targetRepLogPrefix);

  velocypack::Builder builder;
  auto envelope = arangodb::agency::envelope::into_builder(builder);

  for (auto const& [dbName, db] : planNode->get().children()) {
    for (auto const& [idString, node] : db->children()) {
      // check if this node has an owner and the owner is 'target'
      if (auto owner = node->hasAsString("owner");
          !owner.has_value() || owner != "target") {
        continue;
      }

      // now check if there is a replicated log in target with that id
      if (targetNode.has_value() &&
          targetNode->get().has(std::vector{dbName, idString})) {
        continue;
      }

      // delete plan and target
      auto logId = replication2::LogId{basics::StringUtils::uint64(idString)};
      envelope =
          methods::deleteReplicatedLogTrx(std::move(envelope), dbName, logId);
    }
  }

  envelope.done();
  if (builder.slice().length() > 0) {
    write_ret_t res = _agent->write(builder.slice());
    if (!res.successful()) {
      LOG_TOPIC("df4b1", WARN, Logger::SUPERVISION)
          << "failed to update replicated log in agency. Will retry. "
          << builder.toJson();
    }
  }
}

void Supervision::cleanupReplicatedStates() {
  _lock.assertLockedByCurrentThread();

  using namespace replication2::agency;

  // check if Plan has replicated logs
  auto const& planNode = snapshot().hasAsNode(planRepStatePrefix);
  if (!planNode) {
    return;
  }

  auto const& targetNode = snapshot().hasAsNode(targetRepStatePrefix);

  velocypack::Builder builder;
  auto envelope = arangodb::agency::envelope::into_builder(builder);

  for (auto const& [dbName, db] : planNode->get().children()) {
    for (auto const& [idString, node] : db->children()) {
      // check if this node has an owner and the owner is 'target'
      if (auto owner = node->hasAsString("owner");
          !owner.has_value() || owner != "target") {
        continue;
      }

      // now check if there is a replicated log in target with that id
      if (targetNode.has_value() &&
          targetNode->get().has(std::vector{dbName, idString})) {
        continue;
      }

      // delete plan and target
      auto logId = replication2::LogId{basics::StringUtils::uint64(idString)};
      envelope =
          methods::deleteReplicatedStateTrx(std::move(envelope), dbName, logId);
    }
  }

  envelope.done();
  if (builder.slice().length() > 0) {
    write_ret_t res = _agent->write(builder.slice());
    if (!res.successful()) {
      LOG_TOPIC("df4c4", WARN, Logger::SUPERVISION)
          << "failed to update replicated log in agency. Will retry. "
          << builder.toJson();
    }
  }
}

// This is the functional version which actually does the work, it is
// called by the private method Supervision::enforceReplication and the
// unit tests:
void arangodb::consensus::enforceReplicationFunctional(
    Node const& snapshot, uint64_t& jobId,
    std::shared_ptr<VPackBuilder> envelope, uint64_t delayAddFollower) {
  // First check the number of AddFollower and RemoveFollower jobs in ToDo:
  // We always maintain that we have at most maxNrAddRemoveJobsInTodo
  // AddFollower or RemoveFollower jobs in ToDo. These are all long-term
  // cleanup jobs, so they can be done over time. This is to ensure that
  // there is no overload on the Agency job system. Therefore, if this
  // number is at least maxNrAddRemoveJobsInTodo, we skip the rest of
  // the function:
  int const maxNrAddRemoveJobsInTodo = 50;

  auto const& todos = snapshot.hasAsChildren(toDoPrefix).value().get();
  int nrAddRemoveJobsInTodo = 0;
  for (auto it = todos.begin(); it != todos.end(); ++it) {
    auto jobNode = *(it->second);
    auto t = jobNode.hasAsString("type");
    if (t && (t.value() == "addFollower" || t.value() == "removeFollower")) {
      if (++nrAddRemoveJobsInTodo >= maxNrAddRemoveJobsInTodo) {
        return;
      }
    }
  }

  // We will loop over plannedDBs, so we use hasAsChildren
  auto const& plannedDBs = snapshot.hasAsChildren(planColPrefix).value().get();
  auto const& databaseProperties =
      snapshot.hasAsChildren("/Plan/Databases").value().get();

  for (const auto& db_ : plannedDBs) {  // Planned databases
    if (isReplicationTwoDB(databaseProperties, db_.first)) {
      continue;
    }

    auto const& db = *(db_.second);
    for (const auto& col_ : db.children()) {  // Planned collections
      auto const& col = *(col_.second);

      size_t replicationFactor;
      auto replFact = col.hasAsUInt(StaticStrings::ReplicationFactor);
      if (replFact) {
        replicationFactor = replFact.value();
      } else {
        auto replFact2 = col.hasAsString(StaticStrings::ReplicationFactor);
        if (replFact2 && replFact2.value() == StaticStrings::Satellite) {
          // satellites => distribute to every server
          auto available = Job::availableServers(snapshot);
          replicationFactor =
              Job::countGoodOrBadServersInList(snapshot, available);
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
        return pair && *pair;
      });

      if (!clone && !isBuilding) {
        for (auto const& shard_ :
             col.hasAsChildren("shards").value().get()) {  // Pl shards
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
              1 +
              Job::countGoodOrBadServersInList(snapshot, onlyFollowers.slice());
          // leader plus GOOD or BAD followers (not FAILED (except maintenance
          // servers))
          size_t apparentReplicationFactor = shard.slice().length();

          if (actualReplicationFactor != replicationFactor ||
              apparentReplicationFactor != replicationFactor) {
            // Check that there is not yet an addFollower or removeFollower
            // or moveShard job in ToDo for this shard:
            auto const& todo = snapshot.hasAsChildren(toDoPrefix).value().get();
            bool found = false;
            for (auto const& pair : todo) {
              auto const& job = pair.second;
              auto tmp_type = job->hasAsString("type");
              auto tmp_shard = job->hasAsString("shard");
              if ((tmp_type == "addFollower" || tmp_type == "removeFollower" ||
                   tmp_type == "moveShard") &&
                  tmp_shard == shard_.first) {
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
            if (snapshot.has(blockedShardsPrefix + shard_.first)) {
              found = true;
            }
            if (!found) {
              auto shardsLikeMe =
                  Job::clones(snapshot, db_.first, col_.first, shard_.first);
              auto inSyncReplicas =
                  Job::findAllInSyncReplicas(snapshot, db_.first, shardsLikeMe);
              size_t inSyncReplicationFactor =
                  Job::countGoodOrBadServersInList(snapshot, inSyncReplicas);

              if (actualReplicationFactor < replicationFactor &&
                  apparentReplicationFactor < 2 + replicationFactor) {
                // Note: If apparentReplicationFactor is smaller than
                // replicationFactor, then there are fewer servers in the
                // plan than requested by the user. This means the AddFollower
                // job is not subject to the configurable delay and is
                // considered more urgent. This happens, if the
                // replicationFactor is increased by the user.
                std::string notBefore;
                if (apparentReplicationFactor >= replicationFactor) {
                  auto now = std::chrono::system_clock::now();
                  notBefore = timepointToString(
                      now + std::chrono::seconds(delayAddFollower));
                }
                AddFollower(snapshot, nullptr, std::to_string(jobId++),
                            "supervision", db_.first, col_.first, shard_.first,
                            notBefore)
                    .create(envelope);
                if (++nrAddRemoveJobsInTodo >= maxNrAddRemoveJobsInTodo) {
                  return;
                }
              } else if (apparentReplicationFactor > replicationFactor &&
                         inSyncReplicationFactor >= replicationFactor) {
                RemoveFollower(snapshot, nullptr, std::to_string(jobId++),
                               "supervision", db_.first, col_.first,
                               shard_.first)
                    .create(envelope);
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

void Supervision::enforceReplication() {
  _lock.assertLockedByCurrentThread();

  auto envelope = std::make_shared<VPackBuilder>();
  {
    VPackArrayBuilder guard1(envelope.get());
    VPackObjectBuilder guard2(envelope.get());
    arangodb::consensus::enforceReplicationFunctional(
        snapshot(), _jobId, envelope, _delayAddFollower);
  }
  if (envelope->slice()[0].length() > 0) {
    write_ret_t res = singleWriteTransaction(_agent, *envelope, false);

    if (!res.accepted || (res.indices.size() == 1 && res.indices[0] == 0)) {
      LOG_TOPIC("1232a", INFO, Logger::SUPERVISION)
          << "Failed to insert jobs: " << envelope->toJson();
    }
  }
}

// Shrink cluster if applicable, guarded by caller
void Supervision::shrinkCluster() {
  _lock.assertLockedByCurrentThread();

  auto const& todo = snapshot().hasAsChildren(toDoPrefix).value().get();
  auto const& pending = snapshot().hasAsChildren(pendingPrefix).value().get();

  if (!todo.empty() || !pending.empty()) {  // This is low priority
    return;
  }

  // Get servers from plan
  auto availServers = Job::availableServers(snapshot());

  // set by external service like Kubernetes / Starter / DCOS
  size_t targetNumDBServers;
  std::string const NDBServers("/Target/NumberOfDBServers");

  if (snapshot().hasAsUInt(NDBServers)) {
    targetNumDBServers = snapshot().hasAsUInt(NDBServers).value();
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
    auto const& databases =
        snapshot().hasAsChildren(planColPrefix).value().get();
    for (auto const& database : databases) {
      for (auto const& collptr : database.second->children()) {
        auto const& node = *collptr.second;
        if (node.hasAsUInt("replicationFactor")) {
          auto replFact = node.hasAsUInt("replicationFactor").value();
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
      if (availServers.size() > maxReplFact &&
          availServers.size() > targetNumDBServers) {
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
  _delayAddFollower = _agent->config().supervisionDelayAddFollower();
  _delayFailedFollower = _agent->config().supervisionDelayFailedFollower();
  _failedLeaderAddsFollower =
      _agent->config().supervisionFailedLeaderAddsFollower();
  return start();
}

static std::string const syncLatest = "/Sync/LatestID";

void Supervision::getUniqueIds() {
  _lock.assertLockedByCurrentThread();

  int64_t n = 10000;

  std::string path = _agencyPrefix + "/Sync/LatestID";
  velocypack::Builder builder;
  {
    VPackArrayBuilder a(&builder);
    {
      VPackArrayBuilder b(&builder);
      {
        VPackObjectBuilder c(&builder);
        {
          builder.add(VPackValue(path));
          VPackObjectBuilder b(&builder);
          builder.add("op", VPackValue("increment"));
          builder.add("step", VPackValue(n));
        }
      }
    }
    {
      VPackArrayBuilder a(&builder);
      builder.add(VPackValue(path));
    }
  }  // [[{path:{"op":"increment","step":n}}],[path]]

  auto ret = _agent->transact(builder.slice());
  if (ret.accepted) {
    try {
      _jobIdMax =
          ret.result->slice()[1]
              .get(std::vector<std::string>({"arango", "Sync", "LatestID"}))
              .getUInt();
      _jobId = _jobIdMax - n;
    } catch (std::exception const& e) {
      LOG_TOPIC("4da4b", ERR, Logger::SUPERVISION)
          << "Failed to acquire job IDs from agency: " << e.what();
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
    _snapshot = (_spearhead.has(_agencyPrefix))
                    ? _spearhead.nodePtr(_agencyPrefix)
                    : _spearhead.nodePtr();
  }
  return *_snapshot;
}

void Supervision::removeTransactionBuilder(
    velocypack::Builder& del, std::vector<std::string> const& todelete) {
  VPackArrayBuilder trxs(&del);
  {
    VPackArrayBuilder trx(&del);
    {
      VPackObjectBuilder server(&del);
      for (auto const& srv : todelete) {
        del.add(VPackValue(Supervision::agencyPrefix() +
                           arangodb::consensus::healthPrefix + srv));
        {
          VPackObjectBuilder oper(&del);
          del.add("op", VPackValue("delete"));
        }
      }
    }
  }
}

void Supervision::checkUndoLeaderChangeActions() {
  auto const databases = snapshot().hasAsChildren(PLAN_DATABASES);

  auto const isReplication2 = [&](std::string const& database) -> bool {
    if (not databases) {
      return false;
    }
    return consensus::isReplicationTwoDB(*databases, database);
  };

  auto const isServerInPlan =
      [&](std::string_view database, std::string_view collection,
          std::string_view shard, std::string_view server) -> bool {
    auto path = basics::StringUtils::joinT("/", "Plan/Collections", database,
                                           collection, "shards", shard);
    auto servers = snapshot().hasAsArray(path);
    if (not servers) {
      return false;
    }
    TRI_ASSERT(servers->isArray() && servers->length() > 0);
    for (size_t i = 0; i < servers->length(); ++i) {
      if (server == servers->at(i).stringView()) {
        return true;
      }
    }
    return false;
  };

  auto const isServerInSync =
      [&](std::string_view database, std::string_view collection,
          std::string_view shard, std::string_view server) -> bool {
    auto path = basics::StringUtils::joinT("/", "Current/Collections", database,
                                           collection, shard, "servers");
    auto servers = snapshot().hasAsArray(path);
    if (not servers) {
      return false;
    }
    TRI_ASSERT(servers->isArray() && servers->length() > 0);
    for (size_t i = 1; i < servers->length(); ++i) {
      if (server == servers->at(i).stringView()) {
        return true;
      }
    }
    return false;
  };

  auto const checkDeletion = [&](std::string const& shard,
                                 std::shared_ptr<Node> entry) -> bool {
    std::string now(timepointToString(std::chrono::system_clock::now()));
    auto deadline = entry->hasAsString("removeIfNotStartedBy");
    auto started = entry->hasAsString("started");
    if (deadline) {
      if (!started && now > deadline.value()) {
        return true;
      }
    }
    if (started) {
      auto jobId = entry->hasAsString("jobId");
      if (jobId) {
        auto inTodo = _snapshot->hasAsNode(toDoPrefix + jobId.value());
        auto inPending = _snapshot->hasAsNode(pendingPrefix + jobId.value());
        if (!inTodo && !inPending) {
          return true;
        }
      } else {
        return true;
        // This should not happen, since if started is there, we have a
        // jobId.
      }
    }
    auto jobOpt = entry->hasAsNode("moveShard");
    if (!jobOpt) {
      return true;
    }
    Node const& job(jobOpt.value());
    auto database = job.hasAsString("database");
    auto collection = job.hasAsString("collection");
    auto server = job.hasAsString("fromServer");
    if (!server) {
      return true;
    } else {
      if (isReplication2(*database)) {
        auto stateId = LogicalCollection::shardIdToStateId(shard);
        auto target = Job::readStateTarget(snapshot(), *database, stateId);
        if (not target->participants.contains(*server)) {
          return true;
        }
      } else {
        if (!isServerInPlan(database.value(), collection.value(), shard,
                            server.value())) {
          return true;
        }
      }
    }
    return false;
  };

  auto const reclaimShard =
      [&](std::shared_ptr<VPackBuilder> const& trx, std::string const& database,
          std::string const& collection, std::string const& shard,
          std::string const& newLeader, std::string const& oldLeader) {
        uint64_t jobId = _jobId++;
        MoveShard(*_snapshot, _agent, std::to_string(jobId), "supervision",
                  database, collection, shard, newLeader, oldLeader,
                  /*isLeader*/ true, /*remainsFollower*/ true,
                  /*tryUndo*/ false)
            .create(trx);

        std::string now(timepointToString(std::chrono::system_clock::now()));
        std::string path = returnLeadershipPrefix + std::string(shard) + "/";
        trx->add(path + "started", VPackValue(now));
        trx->add(path + "jobId", VPackValue(std::to_string(jobId)));
      };

  auto undos = snapshot().hasAsChildren("Target/ReturnLeadership");
  if (not undos) {
    return;
  }

  // Collect a transaction:
  auto trx = std::make_shared<VPackBuilder>();
  {
    VPackArrayBuilder guard(trx.get());
    VPackObjectBuilder guard2(trx.get());

    for (auto const& [shard, entry] : undos->get()) {
      // First check some conditions under which we simply get rid of the
      // entry:
      //  - deadline exceeded (and not yet started)
      //  - dependent MoveShard gone (and started)
      //  - fromServer no longer in Plan
      if (checkDeletion(shard, entry)) {
        // Let's remove the entry:
        trx->add(VPackValue(returnLeadershipPrefix + shard));
        {
          VPackObjectBuilder guard3(trx.get());
          trx->add("op", VPackValue("delete"));
        }
        continue;
      }

      // Now the job is still valid, let's see if it fires. Note that we
      // do not get here, if the job is not found!
      auto jobOpt = entry->hasAsNode("moveShard");
      TRI_ASSERT(jobOpt);
      Node const& job = jobOpt.value();

      // We need to check if:
      //  - it is not yet started
      //  - the fromServer is GOOD
      //  - it is in sync for the shard (and all distributeShardsLike shards)
      //  - its current rebootId is larger than the stored one
      auto started = entry->hasAsString("started");
      if (started) {
        continue;
      }

      auto database = job.hasAsString("database");
      auto collection = job.hasAsString("collection");
      auto fromServer = job.hasAsString("fromServer");
      auto toServer = job.hasAsString("toServer");
      TRI_ASSERT(fromServer && toServer && database && collection);

      // get server health
      if (serverHealth(fromServer.value()) != HEALTH_STATUS_GOOD) {
        continue;
      }

      if (not isReplication2(*database)) {
        if (!isServerInSync(database.value(), collection.value(), shard,
                            fromServer.value())) {
          continue;
        }
      }

      // get current reboot id
      auto rebootId = snapshot().hasAsUInt(basics::StringUtils::concatT(
          curServersKnown, fromServer.value(), "/", StaticStrings::RebootId));
      if (!rebootId) {
        continue;
      }

      // check if reboot id is bigger than the stored one
      auto storedRebootId = entry->hasAsUInt("rebootId");
      if (!storedRebootId || storedRebootId.value() < rebootId.value()) {
        reclaimShard(trx, database.value(), collection.value(), shard,
                     toServer.value(), fromServer.value());
      }
    }
  }

  // And finally write out the transaction:
  if (trx->slice()[0].length() > 0) {
    write_ret_t res = singleWriteTransaction(_agent, *trx, false);

    if (!res.accepted || (res.indices.size() == 1 && res.indices[0] == 0)) {
      LOG_TOPIC("fad4b", INFO, Logger::SUPERVISION)
          << "Failed to modify returnLeadership jobs: " << trx->toJson();
    }
  }
}
