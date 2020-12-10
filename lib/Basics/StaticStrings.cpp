////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "StaticStrings.h"

using namespace arangodb;

// constants
std::string const StaticStrings::Base64("base64");
std::string const StaticStrings::Binary("binary");
std::string const StaticStrings::Empty("");
std::string const StaticStrings::N1800("1800");

// index lookup strings
std::string const StaticStrings::IndexEq("eq");
std::string const StaticStrings::IndexIn("in");
std::string const StaticStrings::IndexLe("le");
std::string const StaticStrings::IndexLt("lt");
std::string const StaticStrings::IndexGe("ge");
std::string const StaticStrings::IndexGt("gt");

// system attribute names
std::string const StaticStrings::AttachmentString("_attachment");
std::string const StaticStrings::IdString("_id");
std::string const StaticStrings::KeyString("_key");
std::string const StaticStrings::RevString("_rev");
std::string const StaticStrings::FromString("_from");
std::string const StaticStrings::ToString("_to");
std::string const StaticStrings::TimeString("_time");

// URL parameter names
std::string const StaticStrings::IgnoreRevsString("ignoreRevs");
std::string const StaticStrings::IsRestoreString("isRestore");
std::string const StaticStrings::KeepNullString("keepNull");
std::string const StaticStrings::MergeObjectsString("mergeObjects");
std::string const StaticStrings::ReturnNewString("returnNew");
std::string const StaticStrings::ReturnOldString("returnOld");
std::string const StaticStrings::SilentString("silent");
std::string const StaticStrings::WaitForSyncString("waitForSync");
std::string const StaticStrings::SkipDocumentValidation(
    "skipDocumentValidation");
std::string const StaticStrings::IsSynchronousReplicationString(
    "isSynchronousReplication");
std::string const StaticStrings::Group("group");
std::string const StaticStrings::Namespace("namespace");
std::string const StaticStrings::Prefix("prefix");
std::string const StaticStrings::Overwrite("overwrite");
std::string const StaticStrings::OverwriteMode("overwriteMode");
std::string const StaticStrings::PreserveRevisionIds("preserveRevisionIds");
std::string const StaticStrings::Compact("compact");

// replication headers
std::string const StaticStrings::ReplicationHeaderCheckMore(
    "x-arango-replication-checkmore");
std::string const StaticStrings::ReplicationHeaderLastIncluded(
    "x-arango-replication-lastincluded");
std::string const StaticStrings::ReplicationHeaderLastScanned(
    "x-arango-replication-lastscanned");
std::string const StaticStrings::ReplicationHeaderLastTick(
    "x-arango-replication-lasttick");
std::string const StaticStrings::ReplicationHeaderFromPresent(
    "x-arango-replication-frompresent");
std::string const StaticStrings::ReplicationHeaderActive(
    "x-arango-replication-active");

// database names
std::string const StaticStrings::SystemDatabase("_system");

// collection names
std::string const StaticStrings::AnalyzersCollection("_analyzers");
std::string const StaticStrings::LegacyAnalyzersCollection(
    "_iresearch_analyzers");
std::string const StaticStrings::UsersCollection("_users");
std::string const StaticStrings::GraphsCollection("_graphs");
std::string const StaticStrings::AqlFunctionsCollection("_aqlfunctions");
std::string const StaticStrings::QueuesCollection("_queues");
std::string const StaticStrings::JobsCollection("_jobs");
std::string const StaticStrings::AppsCollection("_apps");
std::string const StaticStrings::AppBundlesCollection("_appbundles");
std::string const StaticStrings::ModulesCollection("_modules");
std::string const StaticStrings::FishbowlCollection("_fishbowl");
std::string const StaticStrings::FrontendCollection("_frontend");
std::string const StaticStrings::StatisticsCollection("_statistics");
std::string const StaticStrings::Statistics15Collection("_statistics15");
std::string const StaticStrings::StatisticsRawCollection("_statisticsRaw");

// analyzers names
std::string const StaticStrings::AnalyzersRevision("revision");
std::string const StaticStrings::AnalyzersBuildingRevision("buildingRevision");
std::string const StaticStrings::AnalyzersDeletedRevision("revisionDeleted");

// Database definition fields
std::string const StaticStrings::DatabaseId("id");
std::string const StaticStrings::DatabaseName("name");
std::string const StaticStrings::DatabaseOptions("options");
std::string const StaticStrings::Properties("properties");

// LogicalDataSource definition fields
std::string const StaticStrings::DataSourceDeleted("deleted");
std::string const StaticStrings::DataSourceGuid("globallyUniqueId");
std::string const StaticStrings::DataSourceId("id");
std::string const StaticStrings::DataSourceName("name");
std::string const StaticStrings::DataSourcePlanId("planId");
std::string const StaticStrings::DataSourceSystem("isSystem");
std::string const StaticStrings::DataSourceType("type");

// Index definition fields
std::string const StaticStrings::IndexExpireAfter("expireAfter");
std::string const StaticStrings::IndexFields("fields");
std::string const StaticStrings::IndexId("id");
std::string const StaticStrings::IndexInBackground("inBackground");
std::string const StaticStrings::IndexIsBuilding("isBuilding");
std::string const StaticStrings::IndexName("name");
std::string const StaticStrings::IndexSparse("sparse");
std::string const StaticStrings::IndexType("type");
std::string const StaticStrings::IndexUnique("unique");

// static index names
std::string const StaticStrings::IndexNameEdge("edge");
std::string const StaticStrings::IndexNameEdgeFrom("edge_from");
std::string const StaticStrings::IndexNameEdgeTo("edge_to");
std::string const StaticStrings::IndexNameInaccessible("inaccessible");
std::string const StaticStrings::IndexNamePrimary("primary");
std::string const StaticStrings::IndexNameTime("time");

// index hint strings
std::string const StaticStrings::IndexHintAny("any");
std::string const StaticStrings::IndexHintCollection("collection");
std::string const StaticStrings::IndexHintHint("hint");
std::string const StaticStrings::IndexHintDepth("depth");
std::string const StaticStrings::IndexHintInbound("inbound");
std::string const StaticStrings::IndexHintOption("indexHint");
std::string const StaticStrings::IndexHintOptionForce("forceIndexHint");
std::string const StaticStrings::IndexHintOutbound("outbound");
std::string const StaticStrings::IndexHintWildcard("*");

// HTTP headers
std::string const StaticStrings::Accept("accept");
std::string const StaticStrings::AcceptEncoding("accept-encoding");
std::string const StaticStrings::AccessControlAllowCredentials(
    "access-control-allow-credentials");
std::string const StaticStrings::AccessControlAllowHeaders(
    "access-control-allow-headers");
std::string const StaticStrings::AccessControlAllowMethods(
    "access-control-allow-methods");
std::string const StaticStrings::AccessControlAllowOrigin(
    "access-control-allow-origin");
std::string const StaticStrings::AccessControlExposeHeaders(
    "access-control-expose-headers");
std::string const StaticStrings::AccessControlMaxAge("access-control-max-age");
std::string const StaticStrings::AccessControlRequestHeaders(
    "access-control-request-headers");
std::string const StaticStrings::Allow("allow");
std::string const StaticStrings::AllowDirtyReads("x-arango-allow-dirty-read");
std::string const StaticStrings::Async("x-arango-async");
std::string const StaticStrings::AsyncId("x-arango-async-id");
std::string const StaticStrings::Authorization("authorization");
std::string const StaticStrings::BatchContentType(
    "application/x-arango-batchpart");
std::string const StaticStrings::CacheControl("cache-control");
std::string const StaticStrings::Close("Close");
std::string const StaticStrings::ClusterCommSource("x-arango-source");
std::string const StaticStrings::Code("code");
std::string const StaticStrings::Connection("connection");
std::string const StaticStrings::ContentEncoding("content-encoding");
std::string const StaticStrings::ContentLength("content-length");
std::string const StaticStrings::ContentTypeHeader("content-type");
std::string const StaticStrings::CorsMethods(
    "DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT");
std::string const StaticStrings::Error("error");
std::string const StaticStrings::ErrorCode("errorCode");
std::string const StaticStrings::ErrorMessage("errorMessage");
std::string const StaticStrings::ErrorNum("errorNum");
std::string const StaticStrings::Errors("x-arango-errors");
std::string const StaticStrings::ErrorCodes("x-arango-error-codes");
std::string const StaticStrings::Etag("etag");
std::string const StaticStrings::Expect("expect");
std::string const StaticStrings::ExposedCorsHeaders(
    "etag, content-encoding, content-length, location, server, "
    "x-arango-errors, x-arango-async-id");
std::string const StaticStrings::HLCHeader("x-arango-hlc");
std::string const StaticStrings::KeepAlive("Keep-Alive");
std::string const StaticStrings::LeaderEndpoint("x-arango-endpoint");
std::string const StaticStrings::Location("location");
std::string const StaticStrings::NoSniff("nosniff");
std::string const StaticStrings::Origin("origin");
std::string const StaticStrings::PotentialDirtyRead(
    "x-arango-potential-dirty-read");
std::string const StaticStrings::RequestForwardedTo(
    "x-arango-request-forwarded-to");
std::string const StaticStrings::Server("server");
std::string const StaticStrings::TransferEncoding("transfer-encoding");
std::string const StaticStrings::TransactionBody("x-arango-trx-body");
std::string const StaticStrings::TransactionId("x-arango-trx-id");

std::string const StaticStrings::Unlimited = "unlimited";
std::string const StaticStrings::WwwAuthenticate("www-authenticate");
std::string const StaticStrings::XContentTypeOptions("x-content-type-options");
std::string const StaticStrings::XArangoFrontend("x-arango-frontend");

// mime types
std::string const StaticStrings::MimeTypeDump(
    "application/x-arango-dump; charset=utf-8");
std::string const StaticStrings::MimeTypeHtml("text/html; charset=utf-8");
std::string const StaticStrings::MimeTypeJson(
    "application/json; charset=utf-8");
std::string const StaticStrings::MimeTypeJsonNoEncoding("application/json");
std::string const StaticStrings::MimeTypeText("text/plain; charset=utf-8");
std::string const StaticStrings::MimeTypeVPack("application/x-velocypack");
std::string const StaticStrings::MultiPartContentType("multipart/form-data");

// accept-encodings
std::string const StaticStrings::EncodingIdentity("identity");
std::string const StaticStrings::EncodingDeflate("deflate");

// collection attributes
std::string const StaticStrings::CacheEnabled("cacheEnabled");
std::string const StaticStrings::DistributeShardsLike("distributeShardsLike");
std::string const StaticStrings::DoCompact("doCompact");
std::string const StaticStrings::Indexes("indexes");
std::string const StaticStrings::IsSmart("isSmart");
std::string const StaticStrings::IsSmartChild("isSmartChild");
std::string const StaticStrings::NumberOfShards("numberOfShards");
std::string const StaticStrings::IndexBuckets("indexBuckets");
std::string const StaticStrings::JournalSize("journalSize");
std::string const StaticStrings::MinReplicationFactor("minReplicationFactor");
std::string const StaticStrings::MinRevision("minRevision");
std::string const StaticStrings::ObjectId("objectId");
std::string const StaticStrings::ReplicationFactor("replicationFactor");
std::string const StaticStrings::Satellite("satellite");
std::string const StaticStrings::ShardKeys("shardKeys");
std::string const StaticStrings::Sharding("sharding");
std::string const StaticStrings::ShardingStrategy("shardingStrategy");
std::string const StaticStrings::SmartJoinAttribute("smartJoinAttribute");
std::string const StaticStrings::SyncByRevision("syncByRevision");
std::string const StaticStrings::TempObjectId("tempObjectId");
std::string const StaticStrings::UsesRevisionsAsDocumentIds(
    "usesRevisionsAsDocumentIds");
std::string const StaticStrings::Schema("schema");
std::string const StaticStrings::Version("version");
std::string const StaticStrings::WriteConcern("writeConcern");
std::string const StaticStrings::ShardingSingle("single");

// graph attribute names
std::string const StaticStrings::GraphCollection("_graphs");
std::string const StaticStrings::IsDisjoint("isDisjoint");
std::string const StaticStrings::GraphIsSmart("isSmart");
std::string const StaticStrings::GraphIsSatellite("isSatellite");
std::string const StaticStrings::GraphFrom("from");
std::string const StaticStrings::GraphTo("to");
std::string const StaticStrings::GraphOptions("options");
std::string const StaticStrings::GraphSmartGraphAttribute(
    "smartGraphAttribute");
std::string const StaticStrings::GraphEdgeDefinitions("edgeDefinitions");
std::string const StaticStrings::GraphOrphans("orphanCollections");
std::string const StaticStrings::GraphInitial("initial");
std::string const StaticStrings::GraphInitialCid("initialCid");
std::string const StaticStrings::GraphName("name");

// Query Strings
std::string const StaticStrings::QuerySortASC("ASC");
std::string const StaticStrings::QuerySortDESC("DESC");

// Graph Query Strings
std::string const StaticStrings::GraphQueryEdges("edges");
std::string const StaticStrings::GraphQueryVertices("vertices");
std::string const StaticStrings::GraphQueryPath("path");
std::string const StaticStrings::GraphQueryGlobal("global");
std::string const StaticStrings::GraphQueryNone("none");

// rest query parameter
std::string const StaticStrings::GraphDropCollections("dropCollections");
std::string const StaticStrings::GraphDropCollection("dropCollection");
std::string const StaticStrings::GraphCreateCollections("createCollections");
std::string const StaticStrings::GraphCreateCollection("createCollection");

// Replication
std::string const StaticStrings::ReplicationSoftLockOnly("doSoftLockOnly");
std::string const StaticStrings::FailoverCandidates("failoverCandidates");
std::string const StaticStrings::RevisionTreeBranchingFactor("branchingFactor");
std::string const StaticStrings::RevisionTreeCount("count");
std::string const StaticStrings::RevisionTreeHash("hash");
std::string const StaticStrings::RevisionTreeMaxDepth("maxDepth");
std::string const StaticStrings::RevisionTreeNodes("nodes");
std::string const StaticStrings::RevisionTreeRangeMax("rangeMax");
std::string const StaticStrings::RevisionTreeRangeMin("rangeMin");
std::string const StaticStrings::RevisionTreeRanges("ranges");
std::string const StaticStrings::RevisionTreeResume("resume");
std::string const StaticStrings::RevisionTreeVersion("version");

// Generic attribute names
std::string const StaticStrings::AttrCoordinator("coordinator");
std::string const StaticStrings::AttrCoordinatorRebootId("coordinatorRebootId");
std::string const StaticStrings::AttrCoordinatorId("coordinatorId");
std::string const StaticStrings::AttrIsBuilding("isBuilding");

// misc strings
std::string const StaticStrings::LastValue("lastValue");
std::string const StaticStrings::checksumFileJs("JS_SHA1SUM.txt");

std::string const StaticStrings::RebootId("rebootId");

std::string const StaticStrings::New("new");
std::string const StaticStrings::Old("old");
std::string const StaticStrings::UpgradeEnvName(
    "ARANGODB_UPGRADE_DURING_RESTORE");
std::string const StaticStrings::BackupToDeleteName("DIRECTORY_TO_DELETE");
std::string const StaticStrings::BackupSearchToDeleteName(
    "DIRECTORY_TO_DELETE_SEARCH");

// aql api strings
std::string const StaticStrings::SerializationFormat("serializationFormat");
std::string const StaticStrings::AqlRemoteApi("api");
std::string const StaticStrings::AqlRemoteExecute("execute");
std::string const StaticStrings::AqlRemoteCallStack("callStack");
std::string const StaticStrings::AqlRemoteLimit("limit");
std::string const StaticStrings::AqlRemoteLimitType("limitType");
std::string const StaticStrings::AqlRemoteLimitTypeSoft("soft");
std::string const StaticStrings::AqlRemoteLimitTypeHard("hard");
std::string const StaticStrings::AqlRemoteFullCount("fullCount");
std::string const StaticStrings::AqlRemoteOffset("offset");
std::string const StaticStrings::AqlRemoteInfinity("infinity");
std::string const StaticStrings::AqlRemoteResult("result");
std::string const StaticStrings::AqlRemoteBlock("block");
std::string const StaticStrings::AqlRemoteSkipped("skipped");
std::string const StaticStrings::AqlRemoteState("state");
std::string const StaticStrings::AqlRemoteStateDone("done");
std::string const StaticStrings::AqlRemoteStateHasmore("hasmore");
std::string const StaticStrings::AqlCallListSpecific("specifics");
std::string const StaticStrings::AqlCallListDefault("default");
std::string const StaticStrings::ArangoSearchAnalyzersRevision("analyzersRevision");
std::string const StaticStrings::ArangoSearchCurrentAnalyzersRevision("current");
std::string const StaticStrings::ArangoSearchSystemAnalyzersRevision("system");

// validation
std::string const StaticStrings::ValidationLevelNone("none");
std::string const StaticStrings::ValidationLevelNew("new");
std::string const StaticStrings::ValidationLevelModerate("moderate");
std::string const StaticStrings::ValidationLevelStrict("strict");

std::string const StaticStrings::ValidationParameterMessage("message");
std::string const StaticStrings::ValidationParameterLevel("level");
std::string const StaticStrings::ValidationParameterRule("rule");

// metrics prometheus labels
std::string const StaticStrings::HeartbeatSendTimeMs(
    "arangodb_heartbeat_send_time_msec");
std::string const StaticStrings::HeartbeatFailureCounter(
    "arangodb_heartbeat_failures");

std::string const StaticStrings::MaintenancePhaseOneRuntimeMs(
    "arangodb_maintenance_phase1_runtime_msec");
std::string const StaticStrings::MaintenancePhaseTwoRuntimeMs(
    "arangodb_maintenance_phase2_runtime_msec");
std::string const StaticStrings::MaintenanceAgencySyncRuntimeMs(
    "arangodb_maintenance_agency_sync_runtime_msec");

std::string const StaticStrings::MaintenanceActionRuntimeMs(
    "arangodb_maintenance_action_runtime_msec");
std::string const StaticStrings::MaintenanceActionAccumRuntimeMs(
    "arangodb_maintenance_action_accum_runtime_msec");
std::string const StaticStrings::MaintenanceActionFailureCounter(
    "arangodb_maintenance_action_failure_counter");
std::string const StaticStrings::MaintenanceActionQueueTimeMs(
    "arangodb_maintenance_action_queue_time_msec");
std::string const StaticStrings::MaintenanceActionAccumQueueTimeMs(
    "arangodb_maintenance_action_accum_queue_time_msec");

std::string const StaticStrings::MaintenancePhaseOneAccumRuntimeMs(
    "arangodb_maintenance_phase1_accum_runtime_msec");
std::string const StaticStrings::MaintenancePhaseTwoAccumRuntimeMs(
    "arangodb_maintenance_phase2_accum_runtime_msec");
std::string const StaticStrings::MaintenanceAgencySyncAccumRuntimeMs(
    "arangodb_maintenance_agency_sync_accum_runtime_msec");

std::string const StaticStrings::ActionDuplicateCounter(
    "arangodb_maintenance_action_duplicate_counter");
std::string const StaticStrings::ActionRegisteredCounter(
    "arangodb_maintenance_action_registered_counter");
std::string const StaticStrings::ActionDoneCounter(
    "arangodb_maintenance_action_done_counter");

std::string const StaticStrings::ShardsOutOfSync("arangodb_shards_out_of_sync");
std::string const StaticStrings::ShardsTotalCount(
    "arangodb_shards_total_count");
std::string const StaticStrings::ShardsLeaderCount(
    "arangodb_shards_leader_count");
std::string const StaticStrings::ShardsNotReplicated(
    "arangodb_shards_not_replicated");

std::string const StaticStrings::AgencyCommRequestTimeMs(
    "arangodb_agencycomm_request_time_msec");

std::string const StaticStrings::AqlQueryRuntimeMs(
    "arangodb_aql_total_query_time_msec");

std::string const StaticStrings::SchedulerQueueLength(
    "arangodb_scheduler_queue_length");
std::string const StaticStrings::SchedulerAwakeWorkers(
    "arangodb_scheduler_awake_threads");
std::string const StaticStrings::SchedulerNumWorker(
    "arangodb_scheduler_num_worker_threads");

std::string const StaticStrings::DroppedFollowerCount(
    "arangodb_dropped_followers_count");

std::string const StaticStrings::SupervisionRuntimeMs(
    "arangodb_agency_supervision_runtime_msec");
std::string const StaticStrings::SupervisionRuntimeWaitForSyncMs(
    "arangodb_agency_supervision_runtime_wait_for_replication_msec");

std::string const StaticStrings::SupervisionAccumRuntimeMs(
    "arangodb_agency_supervision_accum_runtime_msec");
std::string const StaticStrings::SupervisionAccumRuntimeWaitForSyncMs(
    "arangodb_agency_supervision_accum_runtime_wait_for_replication_msec");
std::string const StaticStrings::SupervisionFailedServerCount(
    "arangodb_agency_supervision_failed_server_count");
