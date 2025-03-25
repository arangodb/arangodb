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
/// @author Jan Steemann
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Cluster/Agency.h"
// #include "Inspection/InspectorBase.h"
// #include "Inspection/VPack.h"
// #include "Inspection/Format.h"

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
      f.field("fields", x.fields), f.field("id", x.id),
      f.field("name", x.name).fallback(""),
      f.field("objectId", x.objectId).fallback(""),
      f.field("sparse", x.sparse).fallback(false), f.field("type", x.type_),
      f.field("unique", x.unique).fallback(false),
      f.field("cacheEnabled", x.cacheEnabled),
      f.field("deduplicate", x.deduplicate), f.field("estimates", x.estimates),
      f.field("analyzerDefinitions", x.analyzerDefinitions),
      f.field("analyzers", x.analyzers),
      f.field("collectionName", x.collectionName),
      f.field("includeAllFields", x.includeAllFields),
      f.field("optimizeTopK", x.optimizeTopK),
      f.field("primarySort", x.primarySort),
      f.field("primarySortCompression", x.primarySortCompression),
      f.field("storeValues", x.storeValues),
      f.field("storedValues", x.storedValues),
      f.field("trackListPositions", x.trackListPositions),
      f.field("version", x.version), f.field("view", x.view));
}

// Shard Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Shard& x) {
  return f.object(x).fields(
      f.field("error", x.error).fallback(false),
      f.field("errorMessage", x.errorMessage).fallback(""),
      f.field("errorNum", x.errorNum).fallback(0),
      f.field("indexes", x.indexes), f.field("servers", x.servers),
      f.field("failoverCandidates", x.failoverCandidates));
}

// ServerInfo Inspect Function
template<class Inspector>
auto inspect(Inspector& f, ServerInfo& x) {
  return f.object(x).fields(
      f.field("endpoint", x.endpoint),
      f.field("advertisedEndpoint", x.advertisedEndpoint),
      f.field("host", x.host),
      f.field("physicalMemory", x.physicalMemory).fallback(0),
      f.field("numberOfCores", x.numberOfCores).fallback(0),
      f.field("version", x.version).fallback(0),
      f.field("versionString", x.versionString), f.field("engine", x.engine),
      f.field("extendedNamesDatabases", x.extendedNamesDatabases),
      f.field("timestamp", x.timestamp));
}

// ServersRegistered Inspect Function
template<class Inspector>
auto inspect(Inspector& f, ServersRegistered& x) {
  return f.object(x).fields(f.field("Version", x.Version).fallback(0),
                            f.field("servers", x.servers));
}

// ServerKnown Inspect Function
template<class Inspector>
auto inspect(Inspector& f, ServerKnown& x) {
  return f.object(x).fields(f.field("rebootId", x.rebootId).fallback(0));
}

// DatabaseInfo Inspect Function
template<class Inspector>
auto inspect(Inspector& f, DatabaseInfo& x) {
  return f.object(x).fields(
      f.field("error", x.error).fallback(false),
      f.field("errorNum", x.errorNum).fallback(0),
      f.field("errorMessage", x.errorMessage).fallback(""), f.field("id", x.id),
      f.field("name", x.name));
}

// KeyOptions Inspect Function
template<class Inspector>
auto inspect(Inspector& f, KeyOptions& x) {
  return f.object(x).fields(
      f.field("type", x.type_),
      f.field("allowUserKeys", x.allowUserKeys).fallback(false),
      f.field("lastValue", x.lastValue));
}

// ConsolidationPolicy Inspect Function
template<class Inspector>
auto inspect(Inspector& f, ConsolidationPolicy& x) {
  return f.object(x).fields(f.field("type", x.type_),
                            f.field("segmentsBytesFloor", x.segmentsBytesFloor),
                            f.field("segmentsBytesMax", x.segmentsBytesMax),
                            f.field("segmentsMax", x.segmentsMax),
                            f.field("segmentsMin", x.segmentsMin),
                            f.field("minScore", x.minScore));
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
      f.field("storedValues", x.storedValues), f.field("version", x.version),
      f.field("writebufferActive", x.writebufferActive),
      f.field("writebufferIdle", x.writebufferIdle),
      f.field("writebufferSizeMax", x.writebufferSizeMax));
}

// Database Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Database& x) {
  return f.object(x).fields(
      f.field("name", x.name), f.field("id", x.id),
      f.field("isSystem", x.isSystem), f.field("sharding", x.sharding),
      f.field("replicationFactor", x.replicationFactor),
      f.field("writeConcern", x.writeConcern),
      f.field("replicationVersion", x.replicationVersion));
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
      f.field("minReplicationFactor", x.minReplicationFactor).fallback(0),
      f.field("name", x.name),
      f.field("numberOfShards", x.numberOfShards).fallback(0),
      f.field("replicationFactor", x.replicationFactor),
      f.field("schema", x.schema), f.field("shardKeys", x.shardKeys),
      f.field("shardingStrategy", x.shardingStrategy),
      f.field("shardsR2", x.shardsR2),
      f.field("syncByRevision", x.syncByRevision),
      f.field("type", x.type_).fallback(0),
      f.field("usesRevisionsAsDocumentIds", x.usesRevisionsAsDocumentIds),
      f.field("waitForSync", x.waitForSync).fallback(false),
      f.field("writeConcern", x.writeConcern).fallback(0),
      f.field("indexes", x.indexes), f.field("shards", x.shards));
}

// AnalyzerInfo Inspect Function
template<class Inspector>
auto inspect(Inspector& f, AnalyzerInfo& x) {
  return f.object(x).fields(
      f.field("revision", x.revision).fallback(0),
      f.field("buildingRevision", x.buildingRevision).fallback(0));
}

// Metrics Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Metrics& x) {
  return f.object(x).fields(f.field("RebootId", x.RebootId).fallback(0),
                            f.field("ServerId", x.ServerId));
}

// MapUniqueToShortID Inspect Function
template<class Inspector>
auto inspect(Inspector& f, MapUniqueToShortID& x) {
  return f.object(x).fields(
      f.field("TransactionID", x.TransactionID).fallback(0),
      f.field("ShortName", x.ShortName));
}

// Features Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Features& x) {
  return f.object(x).fields(f.field("expires", x.expires).fallback(0));
}

// License Inspect Function
template<class Inspector>
auto inspect(Inspector& f, License& x) {
  return f.object(x).fields(f.field("features", x.features),
                            f.field("version", x.version).fallback(0),
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
      f.field("Engine", x.Engine), f.field("Timestamp", x.Timestamp),
      f.field("SyncTime", x.SyncTime),
      f.field("LastAckedTime", x.LastAckedTime));
}

// State Inspect Function
template<class Inspector>
auto inspect(Inspector& f, State& x) {
  return f.object(x).fields(f.field("Mode", x.Mode),
                            f.field("Timestamp", x.Timestamp));
}

// ArangoAgency Inspect Function
template<class Inspector>
auto inspect(Inspector& f, ArangoAgency& x) {
  return f.object(x).fields(f.field("Definition", x.Definition).fallback(0));
}

// Arango Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Arango& x) {
  return f.object(x).fields(
      f.field("Cluster", x.Cluster), f.field("Agency", x.Agency),
      f.field("Current", x.Current),
      f.field("InitDone", x.InitDone).fallback(false), f.field("Plan", x.Plan),
      f.field("Sync", x.Sync), f.field("Supervision", x.Supervision),
      f.field("Target", x.Target), f.field(".license", x.license),
      f.field("Bootstrap", x.Bootstrap),
      f.field("ClusterUpgradeVersion", x.ClusterUpgradeVersion).fallback(0),
      f.field("SystemCollectionsCreated", x.SystemCollectionsCreated)
          .fallback(false));
}

// AgencyData Inspect Function
template<class Inspector>
auto inspect(Inspector& f, AgencyData& x) {
  return f.object(x).fields(f.field("arango", x.arango));
}

// Current Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Current& x) {
  return f.object(x).fields(
      f.field("AsyncReplication", x.AsyncReplication),
      f.field("Collections", x.Collections),
      f.field("Version", x.Version).fallback(0),
      f.field("ShardsCopied", x.ShardsCopied),
      f.field("NewServers", x.NewServers),
      f.field("Coordinators", x.Coordinators), f.field("Lock", x.Lock),
      f.field("DBServers", x.DBServers), f.field("Singles", x.Singles),
      f.field("ServersRegistered", x.ServersRegistered),
      f.field("Databases", x.Databases),
      f.field("ServersKnown", x.ServersKnown),
      f.field("Foxxmaster", x.Foxxmaster),
      f.field("FoxxmasterQueueupdate", x.FoxxmasterQueueupdate)
          .fallback(false));
}

// Plan Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Plan& x) {
  return f.object(x).fields(
      f.field("AsyncReplication", x.AsyncReplication),
      f.field("Coordinators", x.Coordinators),
      f.field("Databases", x.Databases), f.field("Lock", x.Lock),
      f.field("DBServers", x.DBServers), f.field("Singles", x.Singles),
      f.field("Version", x.Version).fallback(0),
      f.field("Collections", x.Collections), f.field("Views", x.Views),
      f.field("Analyzers", x.Analyzers), f.field("Metrics", x.Metrics));
}

// Sync Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Sync& x) {
  return f.object(x).fields(
      f.field("LatestID", x.LatestID).fallback(0),
      f.field("Problems", x.Problems),
      f.field("UserVersion", x.UserVersion).fallback(0),
      f.field("ServerStates", x.ServerStates),
      f.field("HeartbeatIntervalMs", x.HeartbeatIntervalMs).fallback(0));
}

// Supervision Inspect Function
template<class Inspector>
auto inspect(Inspector& f, Supervision& x) {
  return f.object(x).fields(
      f.field("Health", x.Health), f.field("Shards", x.Shards),
      f.field("DBServers", x.DBServers), f.field("State", x.State));
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
      f.field("Version", x.Version).fallback(0),
      f.field("LatestDBServerId", x.LatestDBServerId).fallback(0),
      f.field("MapUniqueToShortID", x.MapUniqueToShortID),
      f.field("LatestCoordinatorId", x.LatestCoordinatorId).fallback(0));
}

}  // namespace arangodb::agency
