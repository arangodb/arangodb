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

#include "Cluster/Agency.h"
#include "Inspection/Access.h"
#include "Inspection/Transformers.h"

#include <velocypack/Iterator.h>

namespace arangodb::agency {

// AnalyzerDefinition Inspect Function
template<class Inspector>
auto inspect(Inspector& f, AnalyzerDefinition& x) {
  return f.object(x).fields(f.field("name", x.name), f.field("type", x.type_),
                            f.field("properties", x.properties),
                            f.field("features", x.features));
}

// Index Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Index& x) {
  return f.object(x).fields(
      f.field("fields", x.fields), f.field("id", x.id), f.field("name", x.name),
      f.field("objectId", x.objectId).fallback(""),
      f.field("sparse", x.sparse).fallback(false),
      f.field("type", x.type_).fallback(""),
      f.field("unique", x.unique).fallback(false),
      f.field("cache", x.cache).fallback(false),
      f.field("inBackground", x.inBackground).fallback(false),
      f.field("cacheEnabled", x.cacheEnabled),
      f.field("deduplicate", x.deduplicate), f.field("estimates", x.estimates),
      f.field("analyzerDefinitions", x.analyzerDefinitions),
      f.field("analyzers", x.analyzers),
      f.field("collectionName", x.collectionName),
      f.field("includeAllFields", x.includeAllFields),
      f.field("optimizeTopK", x.optimizeTopK),
      f.field("primarySort", x.primarySort),
      f.field("primarySortCompression", x.primarySortCompression),
      f.field("primaryKeyCache", x.primaryKeyCache),
      f.field("storeValues", x.storeValues),
      f.field("storedValues", x.storedValues),
      f.field("trackListPositions", x.trackListPositions),
      f.field("version", x.version), f.field("view", x.view),
      f.field("expireAfter", x.expireAfter),
      f.field("writebufferActive", x.writebufferActive),
      f.field("writebufferIdle", x.writebufferIdle),
      f.field("writebufferSizeMax", x.writebufferSizeMax),
      f.field("worstIndexedLevel", x.worstIndexedLevel),
      f.field("minLength", x.minLength),
      f.field("legacyPolygons", x.legacyPolygons).fallback(false),
      f.field("searchField", x.searchField), f.field("analyzer", x.analyzer),
      f.field("maxNumCoverCells", x.maxNumCoverCells),
      f.field("cleanupIntervalStep", x.cleanupIntervalStep),
      f.field("commitIntervalMsec", x.commitIntervalMsec),
      f.field("consolidationIntervalMsec", x.consolidationIntervalMsec),
      f.field("consolidationPolicy", x.consolidationPolicy),
      f.field("features", x.features), f.field("geoJson", x.geoJson),
      f.field("bestIndexedLevel", x.bestIndexedLevel),
      f.field("error", x.error), f.field("errorMessage", x.errorMessage),
      f.field("errorNum", x.errorNum), f.field("tempObjectId", x.tempObjectId),
      f.field("isBuilding", x.isBuilding),
      f.field("coordinator", x.coordinator),
      f.field("coordinatorRebootId", x.coordinatorRebootId),
      f.field("prefixFields", x.prefixFields),
      f.field("fieldValueTypes", x.fieldValueTypes),
      f.field("isNewlyCreated", x.isNewlyCreated));
}

// Shard Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Shard& x) {
  return f.object(x).fields(
      f.field("error", x.error).fallback(false),
      f.field("errorMessage", x.errorMessage).fallback(""),
      f.field("errorNum", x.errorNum).fallback(uint64_t{0}),
      f.field("indexes", x.indexes), f.field("servers", x.servers),
      f.field("failoverCandidates", x.failoverCandidates));
}

// ServerKnown Inspect Function
template<class Inspector>
auto inspect(Inspector& f, ServerKnown& x) {
  return f.object(x).fields(
      f.field("rebootId", x.rebootId).fallback(uint64_t{0}));
}

// DatabaseInfo Inspect Function
template<class Inspector>
auto inspect(Inspector& f, DatabaseInfo& x) {
  return f.object(x).fields(
      f.field("error", x.error).fallback(false),
      f.field("errorNum", x.errorNum).fallback(uint64_t{0}),
      f.field("errorMessage", x.errorMessage).fallback(""), f.field("id", x.id),
      f.field("name", x.name));
}

// KeyOptions Inspect Function
template<class Inspector>
auto inspect(Inspector& f, KeyOptions& x) {
  return f.object(x)
      .fields(f.field("type", x.type_),
              f.field("allowUserKeys", x.allowUserKeys).fallback(false),
              f.field("lastValue", x.lastValue), f.field("offset", x.offset),
              f.field("increment", x.increment))
      .invariant([](KeyOptions& ko) {
        return ko.type_ != "autoincrement" ||
               (ko.offset.has_value() && ko.increment.has_value());
      });
}

// ConsolidationPolicy Inspect Function
template<class Inspector>
auto inspect(Inspector& f, ConsolidationPolicy& x) {
  return f.object(x)
      .fields(f.field("type", x.type_),
              f.field("segmentsBytesFloor", x.segmentsBytesFloor),
              f.field("segmentsBytesMax", x.segmentsBytesMax),
              f.field("segmentsMax", x.segmentsMax),
              f.field("segmentsMin", x.segmentsMin),
              f.field("minScore", x.minScore),
              f.field("threshold", x.threshold))
      .invariant([](ConsolidationPolicy& c) {
        // TODO
        return true;
      });
}

// View Inspect Function
template<class Inspector>
auto inspect(Inspector& f, View& x) {
  return f.object(x).fields(
      f.field("globallyUniqueId", x.globallyUniqueId), f.field("id", x.id),
      f.field("name", x.name), f.field("deleted", x.deleted),
      f.field("isSystem", x.isSystem), f.field("planId", x.planId),
      f.field("type", x.type_),
      f.field("cleanupIntervalStep", x.cleanupIntervalStep),
      f.field("commitIntervalMsec", x.commitIntervalMsec),
      f.field("consolidationIntervalMsec", x.consolidationIntervalMsec),
      f.field("consolidationPolicy", x.consolidationPolicy),
      f.field("optimizeTopK", x.optimizeTopK),
      f.field("primarySort", x.primarySort),
      f.field("primarySortCompression", x.primarySortCompression),
      f.field("primaryKeyCache", x.primaryKeyCache),
      f.field("storedValues", x.storedValues), f.field("version", x.version),
      f.field("writebufferActive", x.writebufferActive),
      f.field("writebufferIdle", x.writebufferIdle),
      f.field("writebufferSizeMax", x.writebufferSizeMax),
      f.field("indexes", x.indexes));
}

// Database Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Database& x) {
  return f.object(x).fields(
      f.field("name", x.name), f.field("id", x.id),
      f.field("isSystem", x.isSystem), f.field("sharding", x.sharding),
      f.field("replicationFactor", x.replicationFactor),
      f.field("writeConcern", x.writeConcern),
      f.field("replicationVersion", x.replicationVersion),
      f.field("options", x.options), f.field("coordinator", x.coordinator),
      f.field("coordinatorRebootId", x.coordinatorRebootId),
      f.field("isBuilding", x.isBuilding));
}

// Collection Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Collection& x) {
  return f.object(x).fields(
      f.field("cacheEnabled", x.cacheEnabled).fallback(false),
      f.field("computedValues", x.computedValues),
      f.field("distributeShardsLike", x.distributeShardsLike),
      f.field("id", x.id),
      f.field("internalValidatorType", x.internalValidatorType),
      f.field("isDisjoint", x.isDisjoint),
      f.field("isSmart", x.isSmart).fallback(false),
      f.field("isSmartChild", x.isSmartChild),
      f.field("isSystem", x.isSystem).fallback(false),
      f.field("keyOptions", x.keyOptions),
      f.field("minReplicationFactor", x.minReplicationFactor)
          .fallback(uint64_t{0}),
      f.field("name", x.name),
      f.field("numberOfShards", x.numberOfShards).fallback(uint64_t{0}),
      f.field("replicationFactor", x.replicationFactor),
      f.field("schema", x.schema), f.field("shardKeys", x.shardKeys),
      f.field("shardingStrategy", x.shardingStrategy),
      f.field("shardsR2", x.shardsR2),
      f.field("syncByRevision", x.syncByRevision),
      f.field("type", x.type_).fallback(uint64_t{0}),
      f.field("usesRevisionsAsDocumentIds", x.usesRevisionsAsDocumentIds),
      f.field("waitForSync", x.waitForSync).fallback(false),
      f.field("writeConcern", x.writeConcern).fallback(uint64_t{0}),
      f.field("indexes", x.indexes), f.field("shards", x.shards),
      f.field("status", x.status), f.field("deleted", x.deleted),
      f.field("statusString", x.statusString),
      f.field("shadowCollections", x.shadowCollections),
      f.field("isBuilding", x.isBuilding),
      f.field("coordinator", x.coordinator),
      f.field("coordinatorRebootId", x.coordinatorRebootId),
      f.field("smartGraphAttribute", x.smartGraphAttribute),
      f.field("smartJoinAttribute", x.smartJoinAttribute));
}

// AnalyzerInfo Inspect Function
template<class Inspector>
auto inspect(Inspector& f, AnalyzerInfo& x) {
  return f.object(x)
      .fields(
          f.field("revision", x.revision).fallback(uint64_t{0}),
          f.field("buildingRevision", x.buildingRevision).fallback(uint64_t{0}),
          f.field("coordinator", x.coordinator),
          f.field("coordinatorRebootId", x.coordinatorRebootId))
      .invariant([](AnalyzerInfo& c) {
        // Both present or none:
        return !(c.coordinator.has_value() ^ c.coordinatorRebootId.has_value());
      });
}

// DiskUsageDBServer Inspect Function
template<class Inspector>
auto inspect(Inspector& f, DiskUsageDBServer& x) {
  return f.object(x).fields(f.field("Usage", x.Usage).fallback(uint64_t{0}));
}

// DiskUsageLimit Inspect Function
template<class Inspector>
auto inspect(Inspector& f, DiskUsageLimit& x) {
  return f.object(x).fields(
      f.field("Version", x.Version).fallback(uint64_t{0}),
      f.field("TotalUsageBytes", x.TotalUsageBytes).fallback(uint64_t{0}),
      f.field("TotalUsageBytesLastUpdate", x.TotalUsageBytesLastUpdate)
          .fallback(uint64_t{0}),
      f.field("LimitReached", x.LimitReached).fallback(false),
      f.field("LimitReachedLastUpdate", x.LimitReachedLastUpdate)
          .fallback(uint64_t{0}));
}

// DiskUsage Inspect Function
template<class Inspector>
auto inspect(Inspector& f, DiskUsage& x) {
  return f.object(x).fields(f.field("Servers", x.Servers),
                            f.field("Limit", x.Limit));
}

// Metrics Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Metrics& x) {
  return f.object(x).fields(
      f.field("RebootId", x.RebootId).fallback(uint64_t{0}),
      f.field("ServerId", x.ServerId));
}

// MapUniqueToShortID Inspect Function
template<class Inspector>
auto inspect(Inspector& f, MapUniqueToShortID& x) {
  return f.object(x).fields(
      f.field("TransactionID", x.TransactionID).fallback(uint64_t{0}),
      f.field("ShortName", x.ShortName));
}

// Features Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Features& x) {
  return f.object(x).fields(
      f.field("expires", x.expires).fallback(uint64_t{0}));
}

// License Inspect Function
template<class Inspector>
auto inspect(Inspector& f, License& x) {
  return f.object(x).fields(f.field("features", x.features),
                            f.field("version", x.version).fallback(uint64_t{0}),
                            f.field("hash", x.hash),
                            f.field("license", x.license));
}

// Health Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Health& x) {
  return f.object(x).fields(
      f.field("ShortName", x.ShortName), f.field("Endpoint", x.Endpoint),
      f.field("Host", x.Host), f.field("SyncStatus", x.SyncStatus),
      f.field("Status", x.Status), f.field("Version", x.Version),
      f.field("Engine", x.Engine),
      f.field("Timestamp", x.Timestamp)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("SyncTime", x.SyncTime)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("LastAckedTime", x.LastAckedTime)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("AdvertisedEndpoint", x.AdvertisedEndpoint));
}

// State Inspect Function
template<class Inspector>
auto inspect(Inspector& f, State& x) {
  return f.object(x).fields(
      f.field("Mode", x.Mode),
      f.field("Timestamp", x.Timestamp)
          .transformWith(arangodb::inspection::TimeStampTransformer{}));
}

// ArangoAgency Inspect Function
template<class Inspector>
auto inspect(Inspector& f, ArangoAgency& x) {
  return f.object(x).fields(
      f.field("Definition", x.Definition).fallback(uint64_t{0}));
}

// Arango Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Arango& x) {
  return f.object(x).fields(
      f.field("Cluster", x.Cluster), f.field("Agency", x.Agency),
      f.field("Current", x.Current),
      f.field("InitDone", x.InitDone).fallback(false), f.field("Plan", x.Plan),
      f.field("Readonly", x.Readonly).fallback(false), f.field("Sync", x.Sync),
      f.field("Supervision", x.Supervision), f.field("Target", x.Target),
      f.field(".license", x.license), f.field("Bootstrap", x.Bootstrap),
      f.field("ClusterUpgradeVersion", x.ClusterUpgradeVersion)
          .fallback(uint32_t{0}),
      f.field("SystemCollectionsCreated", x.SystemCollectionsCreated)
          .fallback(false));
}

// AgencyData Inspect Function
template<class Inspector>
auto inspect(Inspector& f, AgencyData& x) {
  return f.object(x).fields(f.field("arango", x.arango),
                            f.field(".agency", x.dotAgency),
                            f.field("arangodb-helper", x.arangodbHelper),
                            f.field("arangodb", x.arangodb));
}

// DBServerMaintenance
template<class Inspector>
auto inspect(Inspector& f, DBServerMaintenance& x) {
  return f.object(x).fields(f.field("Mode", x.Mode), f.field("Until", x.Until));
}

// Current Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Current& x) {
  return f.object(x).fields(
      f.field("AsyncReplication", x.AsyncReplication),
      f.field("Collections", x.Collections),
      f.field("Version", x.Version).fallback(uint64_t{0}),
      f.field("ShardsCopied", x.ShardsCopied),
      f.field("NewServers", x.NewServers),
      f.field("Coordinators", x.Coordinators), f.field("Lock", x.Lock),
      f.field("DBServers", x.DBServers), f.field("Singles", x.Singles),
      f.field("ServersRegistered", x.ServersRegistered),
      f.field("Databases", x.Databases),
      f.field("ServersKnown", x.ServersKnown),
      f.field("Foxxmaster", x.Foxxmaster),
      f.field("FoxxmasterQueueupdate", x.FoxxmasterQueueupdate).fallback(false),
      f.field("MaintenanceDBServers", x.MaintenanceDBServers),
      f.field("CollectionGroups", x.CollectionGroups),
      f.field("Views", x.Views), f.field("ReplicatedLogs", x.ReplicatedLogs));
}

// Plan Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Plan& x) {
  return f.object(x).fields(
      f.field("AsyncReplication", x.AsyncReplication),
      f.field("Coordinators", x.Coordinators),
      f.field("Databases", x.Databases), f.field("Lock", x.Lock),
      f.field("DBServers", x.DBServers), f.field("Singles", x.Singles),
      f.field("Version", x.Version).fallback(uint64_t{0}),
      f.field("Collections", x.Collections), f.field("Views", x.Views),
      f.field("Analyzers", x.Analyzers), f.field("Metrics", x.Metrics));
}

// Sync Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Sync& x) {
  return f.object(x).fields(
      f.field("LatestID", x.LatestID).fallback(uint64_t{0}),
      f.field("Problems", x.Problems),
      f.field("UserVersion", x.UserVersion).fallback(uint64_t{0}),
      f.field("ServerStates", x.ServerStates),
      f.field("HeartbeatIntervalMs", x.HeartbeatIntervalMs)
          .fallback(uint64_t{0}),
      f.field("HotBackupRestoreDone", x.HotBackupRestoreDone),
      f.field("FoxxQueueVersion", x.FoxxQueueVersion));
}

// Supervision Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Supervision& x) {
  return f.object(x).fields(
      f.field("Health", x.Health), f.field("Shards", x.Shards),
      f.field("DBServers", x.DBServers), f.field("State", x.State),
      f.field("Maintenance", x.Maintenance)
          .transformWith(arangodb::inspection::TimeStampTransformer{}));
}

// ReconfigureReplicatedLog Inspect Function
template<class Inspector>
auto inspect(Inspector& f, ReconfigureReplicatedLog& x) {
  return f.object(x).fields(f.field("database", x.database),
                            f.field("server", x.server));
}

// ReturnLeadershipEntry Inspect Function
template<class Inspector>
auto inspect(Inspector& f, ReturnLeadershipEntry& x) {
  return f.object(x)
      .fields(f.field("removeIfNotStartedBy", x.removeIfNotStartedBy)
                  .transformWith(arangodb::inspection::TimeStampTransformer{}),
              f.field("started", x.started)
                  .transformWith(arangodb::inspection::TimeStampTransformer{}),
              f.field("jobId", x.jobId),
              f.field("timeStamp", x.timeStamp)
                  .transformWith(arangodb::inspection::TimeStampTransformer{}),
              f.field("rebootId", x.rebootId),
              f.field("moveShard", x.moveShard),
              f.field("reconfigureReplicatedLog", x.reconfigureReplicatedLog))
      .invariant([](ReturnLeadershipEntry& e) {
        // Either one of the two:
        return (e.moveShard.has_value() ^
                e.reconfigureReplicatedLog.has_value()) != 0;
      });
}

// JobBase Inspect Function
template<class Inspector>
auto inspect(Inspector& f, JobBase& x) {
  return f.object(x).fields(
      f.field("type", x.type), f.field("jobId", x.jobId),
      f.field("creator", x.creator),
      f.field("timeCreated", x.timeCreated)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeStarted", x.timeStarted)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeFinished", x.timeFinished)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("notBefore", x.notBefore), f.field("parentJob", x.parentJob),
      f.field("abort", x.abort), f.field("reason", x.reason));
}

// AddFollowerJob Inspect Function
template<class Inspector>
auto inspect(Inspector& f, AddFollowerJob& x) {
  return f.object(x).fields(
      f.field("type", x.type), f.field("jobId", x.jobId),
      f.field("creator", x.creator),
      f.field("timeCreated", x.timeCreated)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeStarted", x.timeStarted)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeFinished", x.timeFinished)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("notBefore", x.notBefore), f.field("reason", x.reason),
      f.field("abort", x.abort), f.field("database", x.database),
      f.field("parentJob", x.parentJob), f.field("collection", x.collection),
      f.field("shard", x.shard));
}

// ResignLeadershipJob Inspect Function
template<class Inspector>
auto inspect(Inspector& f, ResignLeadershipJob& x) {
  return f.object(x).fields(
      f.field("type", x.type), f.field("jobId", x.jobId),
      f.field("creator", x.creator),
      f.field("timeCreated", x.timeCreated)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeStarted", x.timeStarted)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeFinished", x.timeFinished)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("notBefore", x.notBefore), f.field("parentJob", x.parentJob),
      f.field("undoMoves", x.undoMoves), f.field("reason", x.reason),
      f.field("abort", x.abort), f.field("server", x.server));
}

// MoveShardJob Inspect Function
template<class Inspector>
auto inspect(Inspector& f, MoveShardJob& x) {
  return f.object(x).fields(
      f.field("type", x.type), f.field("jobId", x.jobId),
      f.field("creator", x.creator),
      f.field("timeCreated", x.timeCreated)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeStarted", x.timeStarted)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeFinished", x.timeFinished)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("notBefore", x.notBefore), f.field("reason", x.reason),
      f.field("database", x.database), f.field("collection", x.collection),
      f.field("shard", x.shard), f.field("fromServer", x.fromServer),
      f.field("toServer", x.toServer),
      f.field("remainsFollower", x.remainsFollower),
      f.field("parentJob", x.parentJob), f.field("isLeader", x.isLeader),
      f.field("abort", x.abort), f.field("tryUndo", x.tryUndo));
}

// CleanUpLostCollectionJob Inspect Function
template<class Inspector>
auto inspect(Inspector& f, CleanUpLostCollectionJob& x) {
  return f.object(x).fields(
      f.field("type", x.type), f.field("jobId", x.jobId),
      f.field("creator", x.creator),
      f.field("timeCreated", x.timeCreated)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeStarted", x.timeStarted)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeFinished", x.timeFinished)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("notBefore", x.notBefore), f.field("parentJob", x.parentJob),
      f.field("abort", x.abort), f.field("reason", x.reason),
      f.field("server", x.server));
}

// CleanOutServerJob Inspect Function
template<class Inspector>
auto inspect(Inspector& f, CleanOutServerJob& x) {
  return f.object(x).fields(
      f.field("type", x.type), f.field("jobId", x.jobId),
      f.field("creator", x.creator),
      f.field("timeCreated", x.timeCreated)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeStarted", x.timeStarted)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeFinished", x.timeFinished)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("notBefore", x.notBefore), f.field("parentJob", x.parentJob),
      f.field("abort", x.abort), f.field("reason", x.reason),
      f.field("server", x.server));
}

// FailedFollowerJob Inspect Function
template<class Inspector>
auto inspect(Inspector& f, FailedFollowerJob& x) {
  return f.object(x).fields(
      f.field("type", x.type), f.field("jobId", x.jobId),
      f.field("creator", x.creator),
      f.field("timeCreated", x.timeCreated)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeStarted", x.timeStarted)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeFinished", x.timeFinished)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("notBefore", x.notBefore), f.field("parentJob", x.parentJob),
      f.field("reason", x.reason), f.field("database", x.database),
      f.field("collection", x.collection), f.field("shard", x.shard),
      f.field("abort", x.abort), f.field("fromServer", x.fromServer),
      f.field("toServer", x.toServer));
}

// FailedLeaderJob Inspect Function
template<class Inspector>
auto inspect(Inspector& f, FailedLeaderJob& x) {
  return f.object(x).fields(
      f.field("type", x.type), f.field("jobId", x.jobId),
      f.field("creator", x.creator),
      f.field("timeCreated", x.timeCreated)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeStarted", x.timeStarted)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeFinished", x.timeFinished)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("notBefore", x.notBefore), f.field("parentJob", x.parentJob),
      f.field("reason", x.reason), f.field("database", x.database),
      f.field("collection", x.collection), f.field("shard", x.shard),
      f.field("fromServer", x.fromServer), f.field("toServer", x.toServer),
      f.field("abort", x.abort), f.field("addsFollower", x.addsFollower));
}

// FailedServerJob Inspect Function
template<class Inspector>
auto inspect(Inspector& f, FailedServerJob& x) {
  return f.object(x).fields(
      f.field("type", x.type), f.field("jobId", x.jobId),
      f.field("creator", x.creator),
      f.field("timeCreated", x.timeCreated)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeStarted", x.timeStarted)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeFinished", x.timeFinished)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("notBefore", x.notBefore), f.field("parentJob", x.parentJob),
      f.field("reason", x.reason), f.field("server", x.server),
      f.field("abort", x.abort),
      f.field("failedLeaderAddsFollower", x.failedLeaderAddsFollower));
}

// RemoveFollowerJob Inspect Function
template<class Inspector>
auto inspect(Inspector& f, RemoveFollowerJob& x) {
  return f.object(x).fields(
      f.field("type", x.type), f.field("jobId", x.jobId),
      f.field("creator", x.creator),
      f.field("timeCreated", x.timeCreated)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeStarted", x.timeStarted)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("timeFinished", x.timeFinished)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("abort", x.abort), f.field("notBefore", x.notBefore),
      f.field("reason", x.reason), f.field("database", x.database),
      f.field("parentJob", x.parentJob), f.field("collection", x.collection),
      f.field("shard", x.shard));
}

template<class Inspector>
auto inspect(Inspector& f, AgencyJob& x) {
  return f.variant(x).embedded("type").alternatives(
      inspection::type<AddFollowerJob>("addFollower"),
      inspection::type<ResignLeadershipJob>("resignLeadership"),
      inspection::type<MoveShardJob>("moveShard"),
      inspection::type<CleanUpLostCollectionJob>("cleanUpLostCollection"),
      inspection::type<CleanOutServerJob>("cleanOutServer"),
      inspection::type<FailedFollowerJob>("failedFollower"),
      inspection::type<FailedLeaderJob>("failedLeader"),
      inspection::type<FailedServerJob>("failedServer"),
      inspection::type<RemoveFollowerJob>("removeFollower"));
}

// Target Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Target& x) {
  return f.object(x).fields(
      f.field("NumberOfCoordinators", x.NumberOfCoordinators),
      f.field("NumberOfDBServers", x.NumberOfDBServers),
      f.field("CleanedServers", x.CleanedServers),
      f.field("ToBeCleanedServers", x.ToBeCleanedServers),
      f.field("FailedServers", x.FailedServers), f.field("Lock", x.Lock),
      f.field("Failed", x.Failed), f.field("Finished", x.Finished),
      f.field("Pending", x.Pending), f.field("ToDo", x.ToDo),
      f.field("Version", x.Version).fallback(uint64_t{0}),
      f.field("LatestDBServerId", x.LatestDBServerId).fallback(uint64_t{0}),
      f.field("MapUniqueToShortID", x.MapUniqueToShortID),
      f.field("LatestCoordinatorId", x.LatestCoordinatorId)
          .fallback(uint64_t{0}),
      f.field("MaintenanceDBServers", x.MaintenanceDBServers),
      f.field("ReturnLeadership", x.ReturnLeadership),
      f.field("HotBackup", x.HotBackup), f.field("Hotbackup", x.Hotbackup),
      f.field("RemovedServers", x.RemovedServers),
      f.field("MapLocalToID", x.MapLocalToID),
      f.field("DiskUsage", x.DiskUsage));
}

// HotBackupProgress Inspect Function
template<class Inspector>
auto inspect(Inspector& f, HotBackupProgress& x) {
  return f.object(x).fields(
      f.field("Time", x.Time)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("Done", x.Done), f.field("Total", x.Total));
}

// HotBackupDBServer Inspect Function
template<class Inspector>
auto inspect(Inspector& f, HotBackupDBServer& x) {
  return f.object(x).fields(
      f.field("Progress", x.Progress), f.field("lockLocation", x.lockLocation),
      f.field("rebootId", x.rebootId).fallback(uint64_t{0}),
      f.field("Status", x.Status), f.field("Error", x.Error),
      f.field("ErrorMessage", x.ErrorMessage));
}

// HotBackupJob Inspect Function
template<class Inspector>
auto inspect(Inspector& f, HotBackupJob& x) {
  return f.object(x).fields(
      f.field("BackupId", x.BackupId), f.field("DBServers", x.DBServers),
      f.field("Timestamp", x.Timestamp)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("Cancelled", x.Cancelled));
}

// HotBackup Inspect Function
template<class Inspector>
auto inspect(Inspector& f, HotBackup& x) {
  return f.object(x).fields(f.field("TransferJobs", x.TransferJobs),
                            f.field("Transfers", x.Transfers),
                            f.field("Create", x.Create));
}

// Define inspection for ServerInfo with regular approach
template<class Inspector>
auto inspect(Inspector& f, ServerInfo& info) {
  return f.object(info).fields(
      f.field("numberOfCores", info.numberOfCores),
      f.field("timestamp", info.timestamp)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("host", info.host), f.field("version", info.version),
      f.field("physicalMemory", info.physicalMemory),
      f.field("versionString", info.versionString),
      f.field("engine", info.engine), f.field("endpoint", info.endpoint),
      f.field("advertisedEndpoint", info.advertisedEndpoint),
      f.field("extendedNamesDatabases", info.extendedNamesDatabases));
}

}  // namespace arangodb::agency

namespace arangodb::inspection {

template<>
struct Access<arangodb::agency::ServersRegistered>
    : AccessBase<arangodb::agency::ServersRegistered> {
  template<class Inspector>
  static Status apply(Inspector& f, arangodb::agency::ServersRegistered& x) {
    if constexpr (Inspector::isLoading) {
      // During deserialization (loading)

      // Start with an empty object
      auto status = f.beginObject();
      if (!status.ok()) {
        return status;
      }

      VPackSlice slice = f.slice();
      for (auto const& p : VPackObjectIterator(slice)) {
        auto fieldName = p.key.copyString();
        Inspector ff(p.value, f.options());
        //  "Version" field is special and goes directly into our structure
        if (fieldName == "Version") {
          status = ff.apply(x.Version);
        } else {
          // All other fields are keys in our map
          arangodb::agency::ServerInfo serverInfo;
          status = ff.apply(serverInfo);
          if (status.ok()) {
            x.servers[std::string(fieldName)] = std::move(serverInfo);
          } else {
            return status;
          }
        }
      }
      if (!status.ok()) {
        return status;
      }
      return f.endObject();
    } else {
      // During serialization (saving)

      // Start building the object
      inspection::Status status = f.beginObject();
      if (!status.ok()) {
        return status;
      }

      // Add Version field first
      status = f.beginField("Version") | [&]() { return f.apply(x.Version); } |
               [&]() { return f.endField(); };
      if (!status.ok()) {
        return status;
      }

      // Add all server entries as top-level fields
      for (const auto& [serverId, serverInfo] : x.servers) {
        status = f.beginField(serverId) | [&]() {
          return f.apply(serverInfo);
        } | [&]() { return f.endField(); };
        if (!status.ok()) {
          return status;
        }
      }

      return f.endObject();
    }
  }
};

}  // namespace arangodb::inspection
