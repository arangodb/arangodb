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

// URL parameter names
std::string const StaticStrings::IgnoreRevsString("ignoreRevs");
std::string const StaticStrings::IsRestoreString("isRestore");
std::string const StaticStrings::KeepNullString("keepNull");
std::string const StaticStrings::MergeObjectsString("mergeObjects");
std::string const StaticStrings::ReturnNewString("returnNew");
std::string const StaticStrings::ReturnOldString("returnOld");
std::string const StaticStrings::SilentString("silent");
std::string const StaticStrings::WaitForSyncString("waitForSync");
std::string const StaticStrings::IsSynchronousReplicationString(
    "isSynchronousReplication");
std::string const StaticStrings::Group("group");
std::string const StaticStrings::Namespace("namespace");
std::string const StaticStrings::Prefix("prefix");
std::string const StaticStrings::ReplaceExisting("replaceExisting");
std::string const StaticStrings::OverWrite("overwrite");

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

// database and collection names
std::string const StaticStrings::SystemDatabase("_system");

// LogicalDataSource definition fields
std::string const StaticStrings::DataSourceDeleted("deleted");
std::string const StaticStrings::DataSourceGuid("globallyUniqueId");
std::string const StaticStrings::DataSourceId("id");
std::string const StaticStrings::DataSourceName("name");
std::string const StaticStrings::DataSourcePlanId("planId");
std::string const StaticStrings::DataSourceSystem("isSystem");
std::string const StaticStrings::DataSourceType("type");

// Index definition fields
std::string const StaticStrings::IndexFields("fields");
std::string const StaticStrings::IndexId("id");
std::string const StaticStrings::IndexSparse("sparse");
std::string const StaticStrings::IndexType("type");
std::string const StaticStrings::IndexUnique("unique");

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
std::string const StaticStrings::PotentialDirtyRead("x-arango-potential-dirty-read");
std::string const StaticStrings::RequestForwardedTo(
    "x-arango-request-forwarded-to");
std::string const StaticStrings::ResponseCode("x-arango-response-code");
std::string const StaticStrings::Server("server");
std::string const StaticStrings::Unlimited = "unlimited";
std::string const StaticStrings::WwwAuthenticate("www-authenticate");
std::string const StaticStrings::XContentTypeOptions("x-content-type-options");
std::string const StaticStrings::XArangoNoLock("x-arango-nolock");
std::string const StaticStrings::XArangoFrontend("x-arango-frontend");

// mime types
std::string const StaticStrings::MimeTypeJson(
    "application/json; charset=utf-8");
std::string const StaticStrings::MimeTypeText("text/plain; charset=utf-8");
std::string const StaticStrings::MimeTypeVPack("application/x-velocypack");
std::string const StaticStrings::MultiPartContentType("multipart/form-data");

// collection attributes
std::string const StaticStrings::DistributeShardsLike("distributeShardsLike");
std::string const StaticStrings::IsSmart("isSmart");
std::string const StaticStrings::NumberOfShards("numberOfShards");
std::string const StaticStrings::ReplicationFactor("replicationFactor");
std::string const StaticStrings::ShardKeys("shardKeys");

// graph attribute names
std::string const StaticStrings::GraphCollection("_graphs");
std::string const StaticStrings::GraphIsSmart("isSmart");
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

// rest query parameter
std::string const StaticStrings::GraphDropCollections("dropCollections");
std::string const StaticStrings::GraphDropCollection("dropCollection");
std::string const StaticStrings::GraphCreateCollections("createCollections");
std::string const StaticStrings::GraphCreateCollection("createCollection");

// Replication
std::string const StaticStrings::ReplicationSoftLockOnly("doSoftLockOnly");

// misc strings
std::string const StaticStrings::LastValue("lastValue");
std::string const StaticStrings::checksumFileJs("JS_SHA1SUM.txt");
