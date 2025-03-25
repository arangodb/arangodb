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

#include <velocypack/Buffer.h>
#include <velocypack/SharedSlice.h>
#include <velocypack/Slice.h>

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

struct Index {
  velocypack::SharedSlice fields;
  std::string id;
  std::string name;
  std::string objectId;
  bool sparse = false;
  std::string type_;
  bool unique = false;
  std::optional<bool> cacheEnabled;
  std::optional<bool> deduplicate;
  std::optional<bool> estimates;
  std::optional<std::vector<AnalyzerDefinition>> analyzerDefinitions;
  std::optional<std::vector<std::string>> analyzers;
  std::optional<std::string> collectionName;
  std::optional<bool> includeAllFields;
  std::optional<std::vector<velocypack::SharedSlice>> optimizeTopK;
  std::optional<velocypack::SharedSlice> primarySort;
  std::optional<std::string> primarySortCompression;
  std::optional<std::string> storeValues;
  std::optional<std::vector<velocypack::SharedSlice>> storedValues;
  std::optional<bool> trackListPositions;
  std::optional<uint64_t> version;
  std::optional<std::string> view;

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

struct ServerInfo {
  std::string endpoint;
  std::string advertisedEndpoint;
  std::string host;
  uint64_t physicalMemory = 0;
  uint64_t numberOfCores = 0;
  uint64_t version = 0;
  std::string versionString;
  std::string engine;
  std::optional<bool> extendedNamesDatabases;
  std::string timestamp;

  ServerInfo() = default;
};

struct ServersRegistered {
  uint64_t Version = 0;
  std::unordered_map<std::string, ServerInfo> servers;

  ServersRegistered() = default;
};

struct ServerKnown {
  uint64_t rebootId = 0;

  ServerKnown() = default;
};

struct DatabaseInfo {
  bool error = false;
  uint64_t errorNum = 0;
  std::string errorMessage;
  std::string id;
  std::string name;

  DatabaseInfo() = default;
};

struct KeyOptions {
  std::string type_;
  bool allowUserKeys = false;
  std::optional<uint64_t> lastValue;

  KeyOptions() = default;
  KeyOptions(const std::string& type_, bool allowUserKeys = false)
      : type_(type_), allowUserKeys(allowUserKeys) {}
};

struct ConsolidationPolicy {
  std::string type_;
  std::optional<uint64_t> segmentsBytesFloor;
  std::optional<uint64_t> segmentsBytesMax;
  std::optional<uint64_t> segmentsMax;
  std::optional<uint64_t> segmentsMin;
  std::optional<uint64_t> minScore;

  ConsolidationPolicy() = default;
  ConsolidationPolicy(const std::string& type_) : type_(type_) {}
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
  std::optional<std::vector<velocypack::SharedSlice>> storedValues;
  std::optional<uint64_t> version;
  std::optional<uint64_t> writebufferActive;
  std::optional<uint64_t> writebufferIdle;
  std::optional<uint64_t> writebufferSizeMax;

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

  Collection() = default;
  Collection(const std::string& id, const std::string& name,
             bool isSmart = false)
      : id(id), isSmart(isSmart), name(name) {}
};

struct AnalyzerInfo {
  uint64_t revision = 0;
  uint64_t buildingRevision = 0;

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
  std::string hash;
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
  std::string Timestamp;
  std::string SyncTime;
  std::string LastAckedTime;

  Health() = default;
};

struct State {
  std::string Mode;
  std::string Timestamp;

  State() = default;
};

// Forward declarations to resolve circular dependencies
struct Current;
struct Plan;
struct Sync;
struct Supervision;
struct Target;

struct ArangoAgency {
  uint64_t Definition = 0;

  ArangoAgency() = default;
};

// More complex structs due to nested collections
struct Current {
  std::unordered_map<std::string, velocypack::SharedSlice> AsyncReplication;
  std::unordered_map<
      std::string,
      std::unordered_map<std::string, std::unordered_map<std::string, Shard>>>
      Collections;
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
  Metrics Metrics;

  Plan() = default;
};

struct Sync {
  uint64_t LatestID = 0;
  std::unordered_map<std::string, velocypack::SharedSlice> Problems;
  uint64_t UserVersion = 0;
  std::unordered_map<std::string, velocypack::SharedSlice> ServerStates;
  uint64_t HeartbeatIntervalMs = 0;

  Sync() = default;
};

struct Supervision {
  std::unordered_map<std::string, Health> Health;
  std::unordered_map<std::string, velocypack::SharedSlice> Shards;
  std::unordered_map<std::string, velocypack::SharedSlice> DBServers;
  State State;

  Supervision() = default;
};

struct Target {
  std::optional<velocypack::SharedSlice> NumberOfCoordinators;
  std::optional<velocypack::SharedSlice> NumberOfDBServers;
  std::vector<velocypack::SharedSlice> CleanedServers;
  std::vector<velocypack::SharedSlice> ToBeCleanedServers;
  std::unordered_map<std::string, velocypack::SharedSlice> FailedServers;
  std::string Lock;
  std::unordered_map<std::string, velocypack::SharedSlice> Failed;
  std::unordered_map<std::string, velocypack::SharedSlice> Finished;
  std::unordered_map<std::string, velocypack::SharedSlice> Pending;
  std::unordered_map<std::string, velocypack::SharedSlice> ToDo;
  uint64_t Version = 0;
  uint64_t LatestDBServerId = 0;
  std::unordered_map<std::string, MapUniqueToShortID> MapUniqueToShortID;
  uint64_t LatestCoordinatorId = 0;

  Target() = default;
};

struct Arango {
  std::string Cluster;
  ArangoAgency Agency;
  Current Current;
  bool InitDone = false;
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

  AgencyData() = default;
};

}  // namespace arangodb::agency
