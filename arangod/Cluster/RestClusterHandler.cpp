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
#include "VocBase/vocbase.h"
#include "Rest/Version.h"

#include <velocypack/Builder.h>

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
      handleClusterInfo();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-doesDatabaseExist") {
      handleCI_doesDatabaseExist(suffixes);
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-flush") {
      handleCI_flush();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-databases") {
      handleCI_databases();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getCollectionInfo") {
      handleCI_getCollectionInfo(suffixes);
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getCollectionInfoCurrent") {
      handleCI_getCollectionInfoCurrent(suffixes);
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getResponsibleServer") {
      handleCI_getResponsibleServer(suffixes);
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getResponsibleServers") {
      handleCI_getResponsibleServers(suffixes);
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getResponsibleShard") {
      handleCI_getResponsibleShard();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getServerEndpoint") {
      handleCI_getServerEndpoint(suffixes);
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getServerName") {
      handleCI_getServerName(suffixes);
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getDBServers") {
      handleCI_getDBServers();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getCoordinators") {
      handleCI_getCoordinators();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-uniqid") {
      handleCI_uniqid(suffixes);
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getAnalyzersRevision") {
      handleCI_getAnalyzersRevision(suffixes);
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-waitForPlanVersion") {
      handleCI_waitForPlanVersion(suffixes);
      return RestStatus::DONE;
    }
  } else {
    if (_request->requestType() != RequestType::GET) {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                    "only the GET method is allowed");
      return RestStatus::DONE;
    }

    std::vector<std::string> const& suffixes = _request->suffixes();
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
  }
  generateError(
      Result(TRI_ERROR_HTTP_NOT_FOUND,
             "expecting /_api/cluster/[endpoints,agency-dump,agency-cache]"));

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

bool RestClusterHandler::isAdmin() {
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
                    "you need admin rights to produce a cluster info dump");
      return false;
    }
  }
  return true;
}

void RestClusterHandler::handleClusterInfo() {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}



void RestClusterHandler::handleCI_doesDatabaseExist(std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  if (suffixes.size() < 3 || suffixes[1] != "database") {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_BAD_PARAMETER,
                  "database argument missing");
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();

  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();

  { VPackObjectBuilder x(&(*body));
    body->add("exists", ci.doesDatabaseExist(suffixes[2]));
  }
  generateResult(rest::ResponseCode::OK, body->slice());
  return;
}
void RestClusterHandler::handleCI_databases() {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::vector<DatabaseID> res = ci.databases();
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  body->openArray();
  std::vector<DatabaseID>::iterator it;
  for (it = res.begin(); it != res.end(); ++it) {
    body->add(VPackValue(*it));
  }
  body->close();

  generateResult(rest::ResponseCode::OK, body->slice());
}

void RestClusterHandler::handleCI_flush() {
  if (_request->requestType() != RequestType::PUT) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  ci.flush();
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  body->openObject();
  body->add("OK", true);
  body->close();

  generateResult(rest::ResponseCode::OK, body->slice());
}

void RestClusterHandler::handleCI_getCollectionInfo(std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (suffixes.size() < 3) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "database and collection arguments are missing");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  std::string const& databaseID = suffixes[1];
  std::string const& collectionID = suffixes[2];
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::shared_ptr<LogicalCollection> col =
      ci.getCollectionNT(databaseID, collectionID);
  if (col == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  ClusterInfo::getCollectionNotFoundMsg(databaseID, collectionID));
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
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  // Compute ShardShorts
  auto serverAliases = ci.getServerAliases();
  VPackSlice shards = info.get("shards");
  TRI_ASSERT(shards.isObject());
  { VPackObjectBuilder x(&(*body));
    body->add(VPackValue("shardShorts"));
    { VPackObjectBuilder y(&(*body));
      for (auto const& p : VPackObjectIterator(shards)) {
        TRI_ASSERT(p.value.isArray());
        body->add(p.key);
        { VPackArrayBuilder z(&(*body));
          for (VPackSlice s : VPackArrayIterator(p.value)) {
            try {
              std::string t = s.copyString();
              if (t.at(0) == '_') {
                t = t.substr(1);
              }
              if (auto it = serverAliases.find(t); it != serverAliases.end()) {
                body->add(VPackValue(it->second));
              }
            } catch (...) {
            }
          }
        }
      }
    }
  }
  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getCollectionInfoCurrent(std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (suffixes.size() < 4) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "database, collection, shardID arguments are missing");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  std::string const& databaseID = suffixes[1];
  std::string const& collectionID = suffixes[2];
  auto maybeShardID = ShardID::shardIdFromString(suffixes[3]);
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::shared_ptr<LogicalCollection> col =
      ci.getCollectionNT(databaseID, collectionID);
  if (col == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  ClusterInfo::getCollectionNotFoundMsg(databaseID, collectionID));
  }
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  // First some stuff from Plan for which Current does not make sense:
  auto cid = std::to_string(col->id().id());
  std::string const& name = col->name();
  { VPackObjectBuilder x(&(*body));
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
    { VPackArrayBuilder y(&(*body));
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
    { VPackArrayBuilder y(&(*body));
      for (auto const& s : servers) {
        body->add(VPackValue(s));
      }
    }
    body->add(VPackValue("failoverCandidates"));
    { VPackArrayBuilder y(&(*body));
      servers = cic->failoverCandidates(shardID);
      for (auto const& s : servers) {
        body->add(VPackValue(s));
      }
    }
  }
  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getResponsibleServer(std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (suffixes.size() < 1) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "shardID argument is missing");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  auto maybeShardID = ShardID::shardIdFromString(suffixes[1]);
  if (maybeShardID.fail()) {
    // Asking for non-shard name pattern.
    // Compatibility with original API return empty array.
    { VPackArrayBuilder y(&(*body));
    }
  } else {
    auto result = ci.getResponsibleServer(maybeShardID.get());
    { VPackArrayBuilder x(&(*body));
      for (auto const& s : *result) {
        body->add(VPackValue(s));
      }
    }
  }

  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getResponsibleServers(std::vector<std::string> const& suffixes) {
  if (_request->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the POST method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  bool parseSuccess = false;
  VPackSlice postBody = this->parseVPackBody(parseSuccess);
  if (!parseSuccess || !postBody.isArray()) {
    // error message generated in parseVPackBody
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "shardID argument is missing");
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  containers::FlatHashSet<ShardID> shardIds;
  VPackArrayIterator itBody(postBody);
  while (itBody.valid()) {
    auto shardID = (*itBody).stringView();
    auto maybeShard = ShardID::shardIdFromString(shardID);
    if (maybeShard.fail()) {
      // For API compatibility we throw DataSourceNotFound error here
      // And ignore the parsing issue. (Illegally named shard cannot be found)
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                    absl::StrCat("no shard found with ID ", shardID));
      return;
    }
    shardIds.emplace(std::move(maybeShard.get()));
    itBody.next();
  }

  if (shardIds.empty()) {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                    absl::StrCat("no shard found"));
      return;
  }

  auto result = ci.getResponsibleServers(shardIds);
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  { VPackObjectBuilder y(&(*body));
    for (auto const& it : result) {
      body->add(std::string{it.first}, VPackValue(it.second));
    }
  }

  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getResponsibleShard() {
  if (_request->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the POST method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }/*
  auto colIdKey = "collectionId";
  auto docKey = "document";
  auto docIsCompleteKey = "documentIsComplete";
  bool parseSuccess = false;
  VPackSlice postBody = this->parseVPackBody(parseSuccess);
  if (!parseSuccess || !postBody.isObject()) {
    // error message generated in parseVPackBody
    generateError(rest::ResponseCode::ERROR_HTTP_BAD_PARAMETER,
                  TRI_ERROR_BAD_PARAMETER,
                  "collectionId, document, documentIsComplete arguments are missing");
    return;
  }
  bool documentIsComplete = postBody.get(docIsCompleteKey).getBoolean();
  auto document = postBody.get(docKey);
  auto collectionId = postBody.get(colIdKey).stringView();
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();

  TRI_vocbase_t& vocbase();
  /// auto& vocbase = GetContextVocBase(); // todo
  auto collInfo = ci.getCollectionNT(vocbase.name(), collectionId);
  if (collInfo == nullptr) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  ClusterInfo::getCollectionNotFoundMsg(
                    vocbase.name(), collectionId));
    return;
  }

  bool usesDefaultShardingAttributes;

  auto maybeShard = collInfo->getResponsibleShard(
      document, documentIsComplete, usesDefaultShardingAttributes);

  if (maybeShard.fail()) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  maybeShard.result());
    return;
  }
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  { VPackObjectBuilder y(&(*body));
    body->add("shardId", std::string{maybeShard.get()});
    body->add("usesDefaultShardingAttributes", usesDefaultShardingAttributes);
  }
  generateResult(rest::ResponseCode::OK, body->slice());*/
}
void RestClusterHandler::handleCI_getServerEndpoint(std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  if (suffixes.size() < 3 || suffixes[1] != "serverID") {
      generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_BAD_PARAMETER,
                  "serverID argument is missing");
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::string endpoint =
    ci.getServerEndpoint(suffixes[2]);
  if (endpoint.length() == 0) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_HTTP_NOT_FOUND,
                  "No server found by that ID");
    return;
  }
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  { VPackObjectBuilder x(&(*body));
    body->add("endpoint", endpoint);
  }
  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getServerName(std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::string result =
      ci.getServerName(suffixes[1]);
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  body->add(VPackValue(result));

  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getDBServers() {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto DBServers = ci.getCurrentDBServers();
  auto serverAliases = ci.getServerAliases();

  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  { VPackArrayBuilder x(&(*body));
    for (size_t i = 0; i < DBServers.size(); ++i) {
      { VPackObjectBuilder x(&(*body));
        auto id = DBServers[i];
        body->add("serverId", id);
        auto itr = serverAliases.find(id);

        if (itr != serverAliases.end()) {
          body->add("serverName", itr->second);
        } else {
          body->add("serverName", id);
        }
      }
    }
  }
  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getCoordinators() {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::vector<std::string> coordinators = ci.getCurrentCoordinators();

  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  { VPackArrayBuilder x(&(*body));
    for (size_t i = 0; i < coordinators.size(); ++i) {
      ServerID const sid = coordinators[i];
      body->add(VPackValue(sid));
    }
  }

  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_uniqid(std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  uint64_t value = ci.uniqid(basics::StringUtils::uint64(suffixes[1]));
  body->add(VPackValue(value));
  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_getAnalyzersRevision(std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto const analyzerRevision = ci.getAnalyzersRevision(suffixes[1]);

  if (!analyzerRevision) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "<databaseName> is invalid");
    return;
  }

  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  analyzerRevision->toVelocyPack(*body);

  generateResult(rest::ResponseCode::OK, body->slice());
}
void RestClusterHandler::handleCI_waitForPlanVersion(std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  VPackBuilder result;
  auto fut = ci.waitForPlanVersion(basics::StringUtils::uint64(suffixes[1]));
  // Block and wait.
  fut.wait();
  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();

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
