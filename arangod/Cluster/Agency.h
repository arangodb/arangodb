////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Cluster/Utils/ShardID.h"

#include <velocypack/Buffer.h>
#include <velocypack/SharedSlice.h>
#include <velocypack/Slice.h>

#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace arangodb::agency {

struct AnalyzerDefinition {
  std::string name;
  std::string type_;
  std::unordered_map<std::string, velocypack::SharedSlice> properties;
  std::vector<std::string> features;

  AnalyzerDefinition() = default;
  AnalyzerDefinition(const std::string& name, const std::string& type_,
                     const std::unordered_map<
                         std::string, velocypack::SharedSlice>& properties = {},
                     const std::vector<std::string>& features = {})
      : name(name), type_(type_), properties(properties), features(features) {}
};

struct ConsolidationPolicy {
  std::string type_;
  std::optional<uint64_t> segmentsBytesFloor;
  std::optional<uint64_t> segmentsBytesMax;
  std::optional<uint64_t> segmentsMax;
  std::optional<uint64_t> segmentsMin;
  std::optional<uint64_t> minScore;
  std::optional<uint64_t> threshold;

  ConsolidationPolicy() = default;
  ConsolidationPolicy(const std::string& type_) : type_(type_) {}
};

struct Index {
  // Note that most fields here must be optional for the following reason:
  // First of all, some Index entry in the Plan are in fact inverted indexes
  // and thus can have most attributes that a view can have. For other indexes,
  // these are simply not present. Secondly, this object is also used for
  // Current, but there it is possible that there is no index data whatsoever
  // but only an error message. In the good case, all the index fields can
  // be there, though. For a few fields (mostly bools) we have fallbacks,
  // and so we do not need the std::optional.
  std::optional<velocypack::SharedSlice> fields;
  std::string id;
  std::optional<std::string> name;
  std::optional<std::string> objectId;
  bool sparse = false;  // fallback false
  std::string type_;    // empty fallback
  bool unique = false;  // fallback false
  std::optional<bool> inBackground;
  std::optional<bool> cacheEnabled;
  std::optional<bool> deduplicate;
  std::optional<bool> estimates;
  std::optional<bool> cache;
  std::optional<std::vector<AnalyzerDefinition>> analyzerDefinitions;
  std::optional<std::vector<std::string>> analyzers;
  std::optional<std::string> collectionName;
  std::optional<bool> includeAllFields;
  std::optional<std::vector<velocypack::SharedSlice>> optimizeTopK;
  std::optional<velocypack::SharedSlice> primarySort;
  std::optional<std::string> primarySortCompression;
  std::optional<bool> primaryKeyCache;
  std::optional<std::string> storeValues;
  std::optional<std::vector<velocypack::SharedSlice>> storedValues;
  std::optional<bool> trackListPositions;
  std::optional<uint64_t> version;
  std::optional<std::string> view;
  std::optional<uint64_t> expireAfter;
  std::optional<uint64_t> writebufferActive;
  std::optional<uint64_t> writebufferIdle;
  std::optional<uint64_t> writebufferSizeMax;
  std::optional<uint64_t> worstIndexedLevel;
  std::optional<uint64_t> minLength;
  bool legacyPolygons = false;
  std::optional<bool> searchField;
  std::optional<velocypack::SharedSlice> analyzer;
  std::optional<uint64_t> maxNumCoverCells;
  std::optional<uint64_t> cleanupIntervalStep;
  std::optional<uint64_t> commitIntervalMsec;
  std::optional<uint64_t> consolidationIntervalMsec;
  std::optional<ConsolidationPolicy> consolidationPolicy;
  std::optional<std::vector<std::string>> features;
  std::optional<bool> geoJson;
  std::optional<uint64_t> bestIndexedLevel;
  // Usually, the following is not present in Current but, if present, is
  // a bool. Unfortunately, for inverted indexes it can be a string.
  std::optional<velocypack::SharedSlice> error;
  std::optional<std::string> errorMessage;
  std::optional<uint64_t> errorNum;
  std::optional<std::string> tempObjectId;
  std::optional<std::vector<std::string>> prefixFields;
  std::optional<bool> isBuilding;
  std::optional<std::string> coordinator;
  std::optional<uint64_t> coordinatorRebootId;
  std::optional<std::string> fieldValueTypes;
  std::optional<bool> isNewlyCreated;

  Index() = default;
  Index(const std::string& id, const std::string& type_)
      : id(id), type_(type_) {}
};

struct Shard {
  bool error = false;
  std::string errorMessage;
  uint64_t errorNum = 0;
  std::vector<Index> indexes;
  std::vector<std::string> servers;
  std::vector<std::string> failoverCandidates;

  Shard() = default;
};

struct ServerKnown {
  uint64_t rebootId = 0;

  ServerKnown() = default;
};

struct DatabaseInfo {
  bool error = false;
  uint64_t errorNum = 0;
  std::string errorMessage;
  std::optional<std::string> id;
  std::optional<std::string> name;

  DatabaseInfo() = default;
};

struct KeyOptions {
  std::string type_;
  bool allowUserKeys = false;
  std::optional<uint64_t> lastValue;
  std::optional<uint64_t> offset;
  std::optional<uint64_t> increment;

  KeyOptions() = default;
  KeyOptions(const std::string& type_, bool allowUserKeys = false)
      : type_(type_), allowUserKeys(allowUserKeys) {}
};

struct View {
  std::optional<std::string> globallyUniqueId;
  std::string id;
  std::string name;
  std::optional<bool> deleted;
  std::optional<bool> isSystem;
  std::optional<std::string> planId;
  std::string type_;
  std::optional<uint64_t> cleanupIntervalStep;
  std::optional<uint64_t> commitIntervalMsec;
  std::optional<uint64_t> consolidationIntervalMsec;
  std::optional<ConsolidationPolicy> consolidationPolicy;
  std::optional<std::vector<velocypack::SharedSlice>> optimizeTopK;
  std::optional<velocypack::SharedSlice> primarySort;
  std::optional<std::string> primarySortCompression;
  std::optional<bool> primaryKeyCache;
  std::optional<std::vector<velocypack::SharedSlice>> storedValues;
  std::optional<uint64_t> version;
  std::optional<uint64_t> writebufferActive;
  std::optional<uint64_t> writebufferIdle;
  std::optional<uint64_t> writebufferSizeMax;
  std::optional<std::vector<velocypack::SharedSlice>>
      indexes;  // for search aliases

  View() = default;
  View(const std::string& id, const std::string& name, const std::string& type_)
      : id(id), name(name), type_(type_) {}
};

struct Database {
  std::string name;
  std::string id;
  std::optional<bool> isSystem;
  std::optional<std::string> sharding;
  std::optional<uint64_t> replicationFactor;
  std::optional<uint64_t> writeConcern;
  std::optional<std::string> replicationVersion;
  std::optional<velocypack::SharedSlice> options;
  std::optional<std::string> coordinator;
  std::optional<uint64_t> coordinatorRebootId;
  std::optional<bool> isBuilding;

  Database() = default;
  Database(const std::string& name, const std::string& id)
      : name(name), id(id) {}
};

struct Collection {
  bool cacheEnabled = false;
  std::optional<velocypack::SharedSlice> computedValues;
  std::optional<std::string> distributeShardsLike;
  std::string id;
  std::optional<uint64_t> internalValidatorType;
  std::optional<bool> isDisjoint;
  bool isSmart = false;
  std::optional<bool> isSmartChild;
  bool isSystem = false;
  KeyOptions keyOptions;
  uint64_t minReplicationFactor = 0;
  std::string name;
  uint64_t numberOfShards = 0;
  velocypack::SharedSlice replicationFactor;
  std::optional<velocypack::SharedSlice> schema;
  std::vector<std::string> shardKeys;
  std::string shardingStrategy;
  std::optional<std::vector<std::string>> shardsR2;
  std::optional<bool> syncByRevision;
  uint64_t type_ = 0;
  std::optional<bool> usesRevisionsAsDocumentIds;
  bool waitForSync = false;
  uint64_t writeConcern = 0;
  std::vector<Index> indexes;
  std::unordered_map<std::string, std::vector<std::string>> shards;
  std::optional<uint64_t> status;
  std::optional<bool> deleted;
  std::optional<std::string> statusString;
  std::optional<std::vector<uint64_t>> shadowCollections;
  std::optional<bool> isBuilding;
  std::optional<std::string> coordinator;
  std::optional<uint64_t> coordinatorRebootId;
  std::optional<std::string> smartGraphAttribute;
  std::optional<std::string> smartJoinAttribute;

  Collection() = default;
  Collection(const std::string& id, const std::string& name,
             bool isSmart = false)
      : id(id), isSmart(isSmart), name(name) {}
};

struct AnalyzerInfo {
  uint64_t revision = 0;
  uint64_t buildingRevision = 0;
  std::optional<std::string> coordinator;
  std::optional<uint64_t> coordinatorRebootId;

  AnalyzerInfo() = default;
};

struct Metrics {
  uint64_t RebootId = 0;
  std::string ServerId;

  Metrics() = default;
};

struct MapUniqueToShortID {
  uint64_t TransactionID = 0;
  std::string ShortName;

  MapUniqueToShortID() = default;
};

struct Features {
  uint64_t expires = 0;

  Features() = default;
};

struct License {
  Features features;
  uint64_t version = 0;
  std::optional<std::string> hash;
  std::string license;

  License() = default;
};

struct Health {
  std::string ShortName;
  std::string Endpoint;
  std::string Host;
  std::string SyncStatus;
  std::string Status;
  std::string Version;
  std::string Engine;
  std::chrono::system_clock::time_point Timestamp;
  std::chrono::system_clock::time_point SyncTime;
  std::chrono::system_clock::time_point LastAckedTime;
  std::optional<std::string> AdvertisedEndpoint;  // legacy

  Health() = default;
};

struct State {
  std::string Mode;
  std::chrono::system_clock::time_point Timestamp;

  State() = default;
};

struct ArangoAgency {
  uint64_t Definition = 0;

  ArangoAgency() = default;
};

struct DBServerMaintenance {
  std::string Mode;
  std::string Until;
};

// Your existing structures
struct ServerInfo {
  std::optional<uint32_t> numberOfCores;
  std::chrono::system_clock::time_point timestamp;
  std::string host;
  uint32_t version;
  std::optional<uint64_t> physicalMemory;
  std::string versionString;
  std::string engine;
  std::string endpoint;
  std::optional<std::string> advertisedEndpoint;
  std::optional<bool> extendedNamesDatabases;
};

struct ServersRegistered {
  std::unordered_map<std::string, ServerInfo> servers;
  uint64_t Version;
};

// More complex structs due to nested collections
struct Current {
  std::unordered_map<std::string, velocypack::SharedSlice> AsyncReplication;
  std::unordered_map<
      std::string,
      std::unordered_map<std::string, std::unordered_map<std::string, Shard>>>
      Collections;
  std::optional<std::unordered_map<std::string, velocypack::SharedSlice>> Views;
  uint64_t Version = 0;
  std::unordered_map<std::string, velocypack::SharedSlice> ShardsCopied;
  std::unordered_map<std::string, velocypack::SharedSlice> NewServers;
  std::unordered_map<std::string, std::string> Coordinators;
  std::string Lock;
  std::unordered_map<std::string, std::string> DBServers;
  std::unordered_map<std::string, velocypack::SharedSlice> Singles;
  ServersRegistered ServersRegistered;
  std::unordered_map<std::string, std::unordered_map<std::string, DatabaseInfo>>
      Databases;
  std::unordered_map<std::string, ServerKnown> ServersKnown;
  std::string Foxxmaster;
  bool FoxxmasterQueueupdate = false;
  std::optional<std::unordered_map<std::string, DBServerMaintenance>>
      MaintenanceDBServers;
  std::optional<std::unordered_map<std::string, velocypack::SharedSlice>>
      CollectionGroups;
  std::optional<std::unordered_map<std::string, velocypack::SharedSlice>>
      ReplicatedLogs;

  Current() = default;
};

struct Plan {
  std::unordered_map<std::string, velocypack::SharedSlice> AsyncReplication;
  std::unordered_map<std::string, std::string> Coordinators;
  std::unordered_map<std::string, Database> Databases;
  std::string Lock;
  std::unordered_map<std::string, std::string> DBServers;
  std::unordered_map<std::string, velocypack::SharedSlice> Singles;
  uint64_t Version = 0;
  std::unordered_map<std::string, std::unordered_map<std::string, Collection>>
      Collections;
  std::unordered_map<std::string, std::unordered_map<std::string, View>> Views;
  std::optional<std::unordered_map<std::string, AnalyzerInfo>> Analyzers;
  std::optional<Metrics> Metrics;

  Plan() = default;
};

struct Sync {
  uint64_t LatestID = 0;
  std::unordered_map<std::string, velocypack::SharedSlice> Problems;
  uint64_t UserVersion = 0;
  std::unordered_map<std::string, velocypack::SharedSlice> ServerStates;
  uint64_t HeartbeatIntervalMs = 0;
  std::optional<uint64_t> HotBackupRestoreDone;
  std::optional<uint64_t> FoxxQueueVersion;
  Sync() = default;
};

struct Supervision {
  std::unordered_map<std::string, Health> Health;
  std::unordered_map<std::string, velocypack::SharedSlice> Shards;
  std::unordered_map<std::string, velocypack::SharedSlice> DBServers;
  State State;
  std::optional<std::chrono::system_clock::time_point> Maintenance;

  Supervision() = default;
};

struct ReconfigureReplicatedLog {
  std::string database;
  std::string server;
};

struct HotBackupProgress {
  std::chrono::system_clock::time_point Time;
  uint64_t Done = 0;
  uint64_t Total = 0;

  HotBackupProgress() = default;
};

struct HotBackupDBServer {
  std::optional<HotBackupProgress> Progress;
  std::optional<std::string> lockLocation;
  std::optional<uint64_t> rebootId = 0;
  std::optional<std::string> Status;
  std::optional<uint64_t> Error;
  std::optional<std::string> ErrorMessage;

  HotBackupDBServer() = default;
};

struct HotBackupJob {
  std::string BackupId;
  std::unordered_map<std::string, HotBackupDBServer> DBServers;
  std::chrono::system_clock::time_point Timestamp;
  std::optional<bool> Cancelled;

  HotBackupJob() = default;
};

struct HotBackup {
  std::optional<std::unordered_map<std::string, HotBackupJob>> TransferJobs;
  std::optional<std::unordered_map<std::string, velocypack::SharedSlice>>
      Transfers;
  std::optional<std::string> Create;  // probably obsolete

  HotBackup() = default;
};

struct DiskUsageDBServer {
  uint64_t Usage = 0;

  DiskUsageDBServer() = default;
};

struct DiskUsageLimit {
  uint64_t Version = 0;
  uint64_t TotalUsageBytes = 0;
  uint64_t TotalUsageBytesLastUpdate = 0;
  bool LimitReached = false;
  uint64_t LimitReachedLastUpdate = 0;

  DiskUsageLimit() = default;
};

struct DiskUsage {
  std::unordered_map<std::string, DiskUsageDBServer> Servers;
  DiskUsageLimit Limit;

  DiskUsage() = default;
};

// Base struct for common job fields
struct JobBase {
  std::string type;
  std::string jobId;
  std::string creator;
  std::optional<std::chrono::system_clock::time_point> timeCreated;
  std::optional<std::chrono::system_clock::time_point> timeStarted;
  std::optional<std::chrono::system_clock::time_point> timeFinished;
  std::optional<std::string>
      notBefore;  // can be empty, so we do not parse it as timestamp
  std::optional<std::string> parentJob;
  std::optional<std::string> reason;  // for errors, only in error case
  std::optional<bool> abort;          // only when job is aborted or shall abort

  JobBase() = default;
  JobBase(const std::string& type, const std::string& jobId,
          const std::string& creator)
      : type(type), jobId(jobId), creator(creator) {}
};

// AddFollower job type
struct AddFollowerJob : JobBase {
  std::string database;
  std::string collection;
  std::string shard;

  AddFollowerJob() : JobBase("addFollower", "", "") {}
  AddFollowerJob(const std::string& jobId, const std::string& creator)
      : JobBase("addFollower", jobId, creator) {}
};

// ResignLeadership job type
struct ResignLeadershipJob : JobBase {
  std::string server;
  std::optional<bool> undoMoves;

  ResignLeadershipJob() : JobBase("resignLeadership", "", "") {}
  ResignLeadershipJob(const std::string& jobId, const std::string& creator)
      : JobBase("resignLeadership", jobId, creator) {}
};

// MoveShard job type
struct MoveShardJob : JobBase {
  std::string database;
  std::string collection;
  std::string shard;
  std::string fromServer;
  std::string toServer;
  std::optional<bool> remainsFollower;
  std::optional<bool> isLeader;
  std::optional<bool> tryUndo;

  MoveShardJob() : JobBase("moveShard", "", "") {}
  MoveShardJob(const std::string& jobId, const std::string& creator)
      : JobBase("moveShard", jobId, creator) {}
};

// CleanUpLostCollection job type
struct CleanUpLostCollectionJob : JobBase {
  std::string server;

  CleanUpLostCollectionJob() : JobBase("cleanUpLostCollection", "", "") {}
  CleanUpLostCollectionJob(const std::string& jobId, const std::string& creator)
      : JobBase("cleanUpLostCollection", jobId, creator) {}
};

// CleanOutServer job type
struct CleanOutServerJob : JobBase {
  std::string server;

  CleanOutServerJob() : JobBase("cleanOutServer", "", "") {}
  CleanOutServerJob(const std::string& jobId, const std::string& creator)
      : JobBase("cleanOutServer", jobId, creator) {}
};

// FailedFollower job type
struct FailedFollowerJob : JobBase {
  std::string database;
  std::string collection;
  std::string shard;
  std::string fromServer;
  std::optional<std::string> toServer;

  FailedFollowerJob() : JobBase("failedFollower", "", "") {}
  FailedFollowerJob(const std::string& jobId, const std::string& creator)
      : JobBase("failedFollower", jobId, creator) {}
};

// FailedLeader job type
struct FailedLeaderJob : JobBase {
  std::string database;
  std::string collection;
  std::string shard;
  std::string fromServer;
  std::optional<std::string> toServer;
  std::optional<bool> addsFollower;

  FailedLeaderJob() : JobBase("failedLeader", "", "") {}
  FailedLeaderJob(const std::string& jobId, const std::string& creator)
      : JobBase("failedLeader", jobId, creator) {}
};

// FailedServer job type
struct FailedServerJob : JobBase {
  std::string server;
  std::optional<bool> failedLeaderAddsFollower;

  FailedServerJob() : JobBase("failedServer", "", "") {}
  FailedServerJob(const std::string& jobId, const std::string& creator)
      : JobBase("failedServer", jobId, creator) {}
};

// RemoveFollower job type
struct RemoveFollowerJob : JobBase {
  std::string database;
  std::string collection;
  std::string shard;

  RemoveFollowerJob() : JobBase("removeFollower", "", "") {}
  RemoveFollowerJob(const std::string& jobId, const std::string& creator)
      : JobBase("removeFollower", jobId, creator) {}
};

// Define the AgencyJob variant type that combines all job types
using AgencyJob =
    std::variant<AddFollowerJob, ResignLeadershipJob, MoveShardJob,
                 CleanUpLostCollectionJob, CleanOutServerJob, FailedFollowerJob,
                 FailedLeaderJob, FailedServerJob, RemoveFollowerJob>;

struct ReturnLeadershipEntry {
  std::chrono::system_clock::time_point removeIfNotStartedBy;
  std::optional<std::chrono::system_clock::time_point> started;
  std::optional<std::string> jobId;
  std::optional<std::chrono::system_clock::time_point> timeStamp;
  std::optional<uint64_t> rebootId;
  std::optional<MoveShardJob> moveShard;
  std::optional<ReconfigureReplicatedLog> reconfigureReplicatedLog;
};

struct Target {
  std::optional<velocypack::SharedSlice> NumberOfCoordinators;
  std::optional<velocypack::SharedSlice> NumberOfDBServers;
  std::vector<velocypack::SharedSlice> CleanedServers;
  std::vector<velocypack::SharedSlice> ToBeCleanedServers;
  std::unordered_map<std::string, velocypack::SharedSlice> FailedServers;
  std::string Lock;
  std::unordered_map<std::string, AgencyJob> Failed;
  std::unordered_map<std::string, AgencyJob> Finished;
  std::unordered_map<std::string, AgencyJob> Pending;
  std::unordered_map<std::string, AgencyJob> ToDo;
  uint64_t Version = 0;
  uint64_t LatestDBServerId = 0;
  std::unordered_map<std::string, MapUniqueToShortID> MapUniqueToShortID;
  uint64_t LatestCoordinatorId = 0;
  std::optional<std::unordered_map<std::string, DBServerMaintenance>>
      MaintenanceDBServers;
  std::optional<std::unordered_map<std::string, ReturnLeadershipEntry>>
      ReturnLeadership;
  std::optional<HotBackup> HotBackup;
  std::optional<velocypack::SharedSlice> Hotbackup;  // define format later!
  std::optional<std::unordered_map<std::string, std::string>> RemovedServers;
  std::optional<std::unordered_map<std::string, velocypack::SharedSlice>>
      MapLocalToID;
  std::optional<DiskUsage> DiskUsage;

  Target() = default;
};

struct Arango {
  std::string Cluster;
  ArangoAgency Agency;
  Current Current;
  bool InitDone = false;
  bool Readonly = false;
  Plan Plan;
  Sync Sync;
  Supervision Supervision;
  Target Target;
  std::optional<License> license;
  std::string Bootstrap;
  uint32_t ClusterUpgradeVersion = 0;
  bool SystemCollectionsCreated = false;

  Arango() = default;
};

struct AgencyData {
  Arango arango;
  std::optional<velocypack::SharedSlice> dotAgency;
  std::optional<velocypack::SharedSlice> arangodbHelper;  // for starter
  std::optional<velocypack::SharedSlice> arangodb;        // for arangosync

  AgencyData() = default;
};

}  // namespace arangodb::agency
