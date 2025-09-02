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
/// @author Simon Grätzer
/// @author Kaveh Vahedipour
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#include "RestClusterHandler.h"

#include "Basics/StringUtils.h"
#include "Agency/AgencyComm.h"
#include "Agency/AsyncAgencyComm.h"
#include "Agency/Supervision.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Auth/UserManager.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/CollectionInfoCurrent.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Replication/ReplicationFeature.h"
#include "VocBase/LogicalCollection.h"
#include "Rest/Version.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestClusterHandler::RestClusterHandler(ArangodServer& server,
                                       GeneralRequest* request,
                                       GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestClusterHandler::execute() {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (!suffixes.empty()) {
    if (suffixes[0] == "cluster-info") {
      if (!ExecContext::current().isAdminUser()) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                      "you need admin rights to produce a cluster info dump");
        return RestStatus::DONE;
      }
      if (suffixes.size() == 1) {
        handleClusterInfo();
        return RestStatus::DONE;
      }
#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
      if (!ExecContext::current().isSuperuser()) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                      "system level access is needed for this API");
        return RestStatus::DONE;
      }
#endif
      if (suffixes[1] == "flush") {
        handleCI_flush();
        return RestStatus::DONE;
      } else if (suffixes[1] == "get_collection_info") {
        handleCI_getCollectionInfo(suffixes);
        return RestStatus::DONE;
      } else if (suffixes[1] == "get_collection_info_current") {
        handleCI_getCollectionInfoCurrent(suffixes);
        return RestStatus::DONE;
      } else if (suffixes[1] == "get_responsible_servers") {
        handleCI_getResponsibleServers();
        return RestStatus::DONE;
      } else if (suffixes[1] == "get_responsible_shard") {
        handleCI_getResponsibleShard(suffixes);
        return RestStatus::DONE;
      } else if (suffixes[1] == "get_analyzers_revision") {
        handleCI_getAnalyzersRevision(suffixes);
        return RestStatus::DONE;
      } else if (suffixes[1] == "wait_for_plan_version") {
        handleCI_waitForPlanVersion(suffixes);
        return RestStatus::DONE;
      } else if (suffixes[1] == "get_max_number_of_shards") {
        handleCI_getMaxNumberOfShards();
        return RestStatus::DONE;
      } else if (suffixes[1] == "get_max_replication_factor") {
        handleCI_getMaxReplicationFactor();
        return RestStatus::DONE;
      } else if (suffixes[1] == "get_min_replication_factor") {
        handleCI_getMinReplicationFactor();
        return RestStatus::DONE;
      } else {
        generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                      "no such handler in the cluster info.");
        return RestStatus::DONE;
      }
    }
  }
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return RestStatus::DONE;
  }

  if (!suffixes.empty()) {
    if (suffixes[0] == "endpoints") {
      handleCommandEndpoints();
      return RestStatus::DONE;
    } else if (suffixes[0] == "agency-dump") {
      handleAgencyDump();
      return RestStatus::DONE;
    } else if (suffixes[0] == "agency-cache") {
      handleAgencyCache();
      return RestStatus::DONE;
    }
  }
  generateError(Result(TRI_ERROR_HTTP_NOT_FOUND,
                       "expecting /_api/cluster/[endpoints,agency-dump,"
                       "agency-cache,cluster-info]"));

  return RestStatus::DONE;
}

void RestClusterHandler::handleAgencyDump() {
  if (!ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                  TRI_ERROR_NOT_IMPLEMENTED,
                  "only to be executed on coordinators");
    return;
  }

  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af->isActive() && !_request->user().empty()) {
    auth::Level lvl;
    if (af->userManager() != nullptr) {
      lvl = af->userManager()->databaseAuthLevel(_request->user(), "_system",
                                                 true);
    } else {
      lvl = auth::Level::RW;
    }
    if (lvl < auth::Level::RW) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "you need admin rights to produce an agency dump");
      return;
    }
  }

  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  Result res = ci.agencyDump(body);
  if (res.ok()) {
    generateResult(rest::ResponseCode::OK, body->slice());
  } else {
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, res.errorNumber(),
                  res.errorMessage());
  }
}

void RestClusterHandler::handleAgencyCache() {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af->isActive() && !_request->user().empty()) {
    auth::Level lvl;
    if (af->userManager() != nullptr) {
      lvl = af->userManager()->databaseAuthLevel(_request->user(), "_system",
                                                 true);
    } else {
      lvl = auth::Level::RW;
    }
    if (lvl < auth::Level::RW) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "you need admin rights to produce an agency cache dump");
      return;
    }
  }

  auto& ac = server().getFeature<ClusterFeature>().agencyCache();
  auto acb = ac.dump();

  generateResult(rest::ResponseCode::OK, acb->slice());
}

void RestClusterHandler::handleClusterInfo() {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}

void RestClusterHandler::handleCI_flush() {
  if (_request->requestType() != RequestType::PUT) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  ci.flush();
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder x(&(*body));
    body->add("OK", true);
  }
  generateResult(rest::ResponseCode::OK, body->slice());
}

void RestClusterHandler::handleCI_getCollectionInfo(
    std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (suffixes.size() < 4) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "database and collection arguments are missing");
    return;
  }
  std::string const& databaseName = suffixes[2];
  std::string const& collectionName = suffixes[3];
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::shared_ptr<LogicalCollection> col =
      ci.getCollectionNT(databaseName, collectionName);
  if (col == nullptr) {
    generateError(
        rest::ResponseCode::NOT_FOUND, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        ClusterInfo::getCollectionNotFoundMsg(databaseName, collectionName));
    return;
  }

  std::unordered_set<std::string> ignoreKeys{
      StaticStrings::AllowUserKeys,
      "avoidServers",
      StaticStrings::DataSourceCid,
      StaticStrings::DataSourceGuid,
      "count",
      StaticStrings::DistributeShardsLike,
      StaticStrings::KeyOptions,
      StaticStrings::NumberOfShards,
      "path",
      StaticStrings::DataSourcePlanId,
      StaticStrings::Version,
      StaticStrings::ObjectId};
  VPackBuilder infoBuilder = col->toVelocyPackIgnore(
      ignoreKeys, LogicalDataSource::Serialization::List);
  VPackSlice info = infoBuilder.slice();

  TRI_ASSERT(info.isObject());
  std::shared_ptr<VPackBuilder> serverAliasesShorts =
      std::make_shared<VPackBuilder>();
  // Compute ShardShorts
  auto serverAliases = ci.getServerAliases();
  VPackSlice shards = info.get("shards");
  TRI_ASSERT(shards.isObject());
  {
    VPackObjectBuilder x(&(*serverAliasesShorts));
    serverAliasesShorts->add(VPackValue("shardShorts"));
    {
      VPackObjectBuilder y(&(*serverAliasesShorts));
      for (auto const& p : VPackObjectIterator(shards)) {
        TRI_ASSERT(p.value.isArray());
        serverAliasesShorts->add(p.key);
        {
          VPackArrayBuilder z(&(*serverAliasesShorts));
          for (VPackSlice s : VPackArrayIterator(p.value)) {
            try {
              std::string t = s.copyString();
              if (t.at(0) == '_') {
                t = t.substr(1);
              }
              if (auto it = serverAliases.find(t); it != serverAliases.end()) {
                serverAliasesShorts->add(VPackValue(it->second));
              }
            } catch (...) {
            }
          }
        }
      }
    }
  }
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  VPackCollection::merge(*body, serverAliasesShorts->slice(),
                         infoBuilder.slice(), true, false);
  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getCollectionInfoCurrent(
    std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (suffixes.size() < 4) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "database, collection, shardID arguments are missing");
    return;
  }
  std::string const& databaseID = suffixes[2];
  std::string const& collectionName = suffixes[3];
  auto maybeShardID = ShardID::shardIdFromString(suffixes[4]);
  if (maybeShardID.fail()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "invalid shardID");
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::shared_ptr<LogicalCollection> col =
      ci.getCollectionNT(databaseID, collectionName);
  if (col == nullptr) {
    generateError(
        rest::ResponseCode::NOT_FOUND, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        ClusterInfo::getCollectionNotFoundMsg(databaseID, collectionName));
    return;
  }
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  // First some stuff from Plan for which Current does not make sense:
  auto cid = std::to_string(col->id().id());
  std::string const& name = col->name();
  {
    VPackObjectBuilder x(&(*body));
    body->add("id", cid);
    body->add("name", name);

    std::shared_ptr<CollectionInfoCurrent> cic =
        ci.getCollectionCurrent(databaseID, cid);

    body->add("currentVersion", (double)cic->getCurrentVersion());
    body->add("type", (int)col->type());

    auto& shardID = maybeShardID.get();

    VPackSlice slice = cic->getIndexes(shardID);
    body->add("indexes", slice);

    // Finally, report any possible error:
    bool error = cic->error(shardID);
    body->add(StaticStrings::Error, error);
    if (error) {
      body->add(StaticStrings::ErrorNum, cic->errorNum(shardID));
      body->add(StaticStrings::ErrorMessage, cic->errorMessage(shardID));
    }
    auto servers = cic->servers(shardID);

    body->add(VPackValue("shorts"));
    {
      VPackArrayBuilder y(&(*body));
      auto serverAliases = ci.getServerAliases();
      for (auto const& s : servers) {
        try {
          if (auto it = serverAliases.find(s); it != serverAliases.end()) {
            body->add(VPackValue(it->second));
          }
        } catch (...) {
        }
      }
    }
    body->add(VPackValue("servers"));
    {
      VPackArrayBuilder y(&(*body));
      for (auto const& s : servers) {
        body->add(VPackValue(s));
      }
    }
    body->add(VPackValue("failoverCandidates"));
    {
      VPackArrayBuilder y(&(*body));
      servers = cic->failoverCandidates(shardID);
      for (auto const& s : servers) {
        body->add(VPackValue(s));
      }
    }
  }
  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getResponsibleServers() {
  if (_request->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the POST method is allowed");
    return;
  }
  bool parseSuccess = false;
  VPackSlice postBody = this->parseVPackBody(parseSuccess);
  if (!parseSuccess || !postBody.isArray()) {
    // error message generated in parseVPackBody
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "shardID argument is missing");
    return;
  }
  containers::FlatHashSet<ShardID> shardIds;
  VPackArrayIterator itBody(postBody);
  while (itBody.valid()) {
    auto shardID = (*itBody).stringView();
    auto maybeShard = ShardID::shardIdFromString(shardID);
    if (maybeShard.fail()) {
      // For API compatibility we throw DataSourceNotFound error here
      // And ignore the parsing issue. (Illegally named shard cannot be found)
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    absl::StrCat("no shard found with ID ", shardID));
      return;
    }
    shardIds.emplace(std::move(maybeShard.get()));
    itBody.next();
  }

  if (shardIds.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "no shard found");
    return;
  }

  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto result = ci.getResponsibleServers(shardIds);
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder y(&(*body));
    for (auto const& it : result) {
      body->add(std::string{it.first}, VPackValue(it.second));
    }
  }

  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getResponsibleShard(
    std::vector<std::string> const& suffixes) {
  if (_request->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the POST method is allowed");
    return;
  }
  if (suffixes.size() < 4) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "databaseName, collectionName, documentIsComplete arguments "
                  "are missing");
    return;
  }
  bool parseSuccess = false;
  VPackSlice document = this->parseVPackBody(parseSuccess);
  if (!parseSuccess || !document.isObject()) {
    // error message generated in parseVPackBody
    return;
  }
  auto databaseName = suffixes[2];
  auto collectionName = suffixes[3];
  bool documentIsComplete = suffixes[4] == "true";
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();

  auto collInfo = ci.getCollectionNT(databaseName, collectionName);
  if (collInfo == nullptr) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
        ClusterInfo::getCollectionNotFoundMsg(databaseName, collectionName));
    return;
  }

  bool usesDefaultShardingAttributes;

  auto maybeShard = collInfo->getResponsibleShard(
      document, documentIsComplete, usesDefaultShardingAttributes);

  if (maybeShard.fail()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "no shard found for the document");
    return;
  }
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder y(&(*body));
    body->add("shardId", std::string{maybeShard.get()});
    body->add("usesDefaultShardingAttributes", usesDefaultShardingAttributes);
  }
  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getAnalyzersRevision(
    std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (suffixes.size() < 3) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "database argument is missing");
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto const analyzerRevision = ci.getAnalyzersRevision(suffixes[2]);

  if (!analyzerRevision) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "<databaseName> is invalid");
    return;
  }

  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  analyzerRevision->toVelocyPack(*body);

  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_waitForPlanVersion(
    std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_BAD_PARAMETER, "only the GET method is allowed");
    return;
  }
  if (suffixes.size() < 3) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "time wait argument is missing");
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  VPackBuilder result;
  auto fut = ci.waitForPlanVersion(basics::StringUtils::uint64(suffixes[2]));
  // Block and wait.
  fut.wait();
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();

  generateResult(rest::ResponseCode::OK, body->slice());
}

void RestClusterHandler::handleCI_getMaxNumberOfShards() {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_BAD_PARAMETER, "only the GET method is allowed");
    return;
  }
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder y(&(*body));
    body->add("maxNumberOfShards",
              server().getFeature<ClusterFeature>().maxNumberOfShards());
  }
  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getMaxReplicationFactor() {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_BAD_PARAMETER, "only the GET method is allowed");
    return;
  }
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder y(&(*body));
    body->add("maxReplicationfactor",
              server().getFeature<ClusterFeature>().maxReplicationFactor());
  }
  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getMinReplicationFactor() {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_BAD_PARAMETER, "only the GET method is allowed");
    return;
  }
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder y(&(*body));
    body->add("minReplicationfactor",
              server().getFeature<ClusterFeature>().minReplicationFactor());
  }
  generateResult(rest::ResponseCode::OK, body->slice());
}

/// @brief returns information about all coordinator endpoints
void RestClusterHandler::handleCommandEndpoints() {
  ClusterInfo& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::vector<ServerID> endpoints;

  if (ServerState::instance()->isCoordinator()) {
    endpoints = ci.getCurrentCoordinators();
  } else {
    generateError(Result(TRI_ERROR_NOT_IMPLEMENTED,
                         "cannot serve this request for this deployment type"));
    return;
  }

  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::Error, VPackValue(false));
  builder.add(StaticStrings::Code, VPackValue(200));
  {
    VPackArrayBuilder array(&builder, "endpoints", true);

    for (ServerID const& sid : endpoints) {
      VPackObjectBuilder obj(&builder);
      std::string advertised = ci.getServerAdvertisedEndpoint(sid);
      std::string internal = ci.getServerEndpoint(sid);
      if (!advertised.empty()) {
        builder.add("endpoint", VPackValue(advertised));
        builder.add("internal", VPackValue(internal));
      } else {
        builder.add("endpoint", VPackValue(internal));
      }
    }
  }
  builder.close();
  generateResult(rest::ResponseCode::OK, builder.slice());
}
