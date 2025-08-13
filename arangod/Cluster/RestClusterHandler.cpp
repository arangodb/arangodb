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
      return handleCI_doesDatabaseExist(suffixes);
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
      handleCI_getCollectionInfoCurrent();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getResponsibleServer") {
      handleCI_getResponsibleServer();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getResponsibleServers") {
      handleCI_getResponsibleServers();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getResponsibleShard") {
      handleCI_getResponsibleShard();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getServerEndpoint") {
      handleCI_getServerEndpoint();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getServerName") {
      handleCI_getServerName();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getDBServers") {
      handleCI_getDBServers();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getCoordinators") {
      handleCI_getCoordinators();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-uniqid") {
      handleCI_uniqid();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-getAnalyzersRevision") {
      handleCI_getAnalyzersRevision();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info-waitForPlanVersion") {
      handleCI_waitForPlanVersion();
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
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}



RestStatus  RestClusterHandler::handleCI_doesDatabaseExist(std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return RestStatus::DONE;
  }
  if (!isAdmin()) {
    return RestStatus::DONE;
  }
  if (suffixes.size() < 2) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "database argument missing");
    return RestStatus::DONE;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();

  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();

  body->openObject();

  body->add("exists", ci.doesDatabaseExist(suffixes[1]));
  body->close();

  generateResult(rest::ResponseCode::OK, body->slice());
  return RestStatus::DONE;
}
void  RestClusterHandler::handleCI_databases() {
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

void  RestClusterHandler::handleCI_getCollectionInfo(std::vector<std::string> const& suffixes) {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return;
  }
  if (suffixes.size() < 3) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "database and collection argument missing");
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
/*
  // Compute ShardShorts
  auto serverAliases = ci.getServerAliases();
  VPackSlice shards = info.get("shards");
  TRI_ASSERT(shards.isObject());
  body->openObject();
  body->add("shardShorts");
  body->openObject();
  for (auto const& p : VPackObjectIterator(shards)) {
    TRI_ASSERT(p.value.isArray());
    body->add(p.key);
    body->openArray();
    std::shared_ptr<VPackBuilder> serverAliasesArray = std::make_shared<VPackBuilder>();
    serverAliasesArray->openArray();
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
    body->close();
  }
  body->close();
  body->close();
*/
  generateResult(rest::ResponseCode::OK, body->slice());
}
void  RestClusterHandler::handleCI_getCollectionInfoCurrent() {
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}
void  RestClusterHandler::handleCI_getResponsibleServer() {
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}
void  RestClusterHandler::handleCI_getResponsibleServers() {
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}
void  RestClusterHandler::handleCI_getResponsibleShard() {
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}
void  RestClusterHandler::handleCI_getServerEndpoint() {
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}
void  RestClusterHandler::handleCI_getServerName() {
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}
void  RestClusterHandler::handleCI_getDBServers() {
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}
void  RestClusterHandler::handleCI_getCoordinators() {
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}
void  RestClusterHandler::handleCI_uniqid() {
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}
void  RestClusterHandler::handleCI_getAnalyzersRevision() {
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
}
void  RestClusterHandler::handleCI_waitForPlanVersion() {
  if (!isAdmin()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  generateResult(rest::ResponseCode::OK, dump.slice());
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
