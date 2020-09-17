////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminClusterHandler.h"

#include <chrono>

#include <boost/functional/hash.hpp>

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Agency/AgencyPaths.h"
#include "Agency/AsyncAgencyComm.h"
#include "Agency/TimeString.h"
#include "Agency/TransactionBuilder.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ResultT.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Sharding/ShardDistributionReporter.h"
#include "Utils/ExecContext.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

using namespace std::chrono_literals;

namespace {

struct agentConfigHealthResult {
  std::string endpoint, name;
  futures::Try<network::Response> response;
};

void removePlanServers(std::unordered_set<std::string>& servers, VPackSlice plan) {
  for (auto const& database : VPackObjectIterator(plan.get("Collections"))) {
    for (auto const& collection : VPackObjectIterator(database.value)) {
      VPackSlice shards = collection.value.get("shards");
      for (auto const& shard : VPackObjectIterator(shards)) {
        for (auto const& server : VPackArrayIterator(shard.value)) {
          servers.erase(server.copyString());
          if (servers.empty()) {
            return;
          }
        }
      }
    }
  }
}

void removeCurrentServers(std::unordered_set<std::string>& servers, VPackSlice current) {
  for (auto const& database : VPackObjectIterator(current.get("Collections"))) {
    for (auto const& collection : VPackObjectIterator(database.value)) {
      for (auto const& shard : VPackObjectIterator(collection.value)) {
        for (auto const& server :
             VPackArrayIterator(shard.value.get("servers"))) {
          servers.erase(server.copyString());
          if (servers.empty()) {
            return;
          }
        }
      }
    }
  }
}

bool isServerResponsibleForSomething(std::string const& server, VPackSlice plan,
                                     VPackSlice current) {
  std::unordered_set<std::string> servers = {server};
  removePlanServers(servers, plan);
  removeCurrentServers(servers, current);
  return servers.size() == 1;
}

template <typename T, typename F>
struct delayedCalculator {
  bool constructed;
  T content;
  F constructor;

  delayedCalculator(delayedCalculator const&) = delete;
  delayedCalculator(delayedCalculator&&) noexcept = delete;
  template <typename U>
  explicit delayedCalculator(U&& c)
      : constructed(false), constructor(std::forward<U>(c)) {}

  T* operator->() {
    if (!constructed) {
      constructed = true;
      content = constructor();
    }
    return &content;
  }
};

template <typename F>
delayedCalculator(F)->delayedCalculator<std::invoke_result_t<F>, F>;

void buildHealthResult(VPackBuilder& builder,
                       std::vector<futures::Try<agentConfigHealthResult>> const& config,
                       VPackSlice store) {
  auto rootPath = arangodb::cluster::paths::root()->arango();

  using server_set = std::unordered_set<std::string>;

  auto canBeDeletedConstructor = [&]() {
    server_set set;
    {
      VPackObjectIterator memberIter(store.get(rootPath->supervision()->health()->vec()));
      for (auto member : memberIter) {
        set.insert(member.key.copyString());
      }
    }

    removePlanServers(set, store.get(rootPath->plan()->vec()));
    removeCurrentServers(set, store.get(rootPath->current()->vec()));
    return set;
  };
  delayedCalculator canBeDeleted(canBeDeletedConstructor);

  struct AgentInformation {
    bool leader;
    double lastAcked;
    AgentInformation() : leader(false), lastAcked(0.0) {}
  };

  std::unordered_map<std::string, AgentInformation> agents;

  // gather information about the agents
  for (auto const& agentTry : config) {
    TRI_ASSERT(agentTry.hasValue());

    auto& agent = agentTry.get();
    // check if the agent responded. If not, ignore. This is just for building up agent information.
    if (agent.response.hasValue()) {
      auto& response = agent.response.get();
      if (response.ok() && response.response->statusCode() == fuerte::StatusOK) {
        VPackSlice lastAcked = response.slice().get("lastAcked");
        if (lastAcked.isNone()) {
          continue;
        }
        agents[agent.name].leader = true;
        for (const auto& agentIter : VPackObjectIterator(lastAcked)) {
          agents[agentIter.key.copyString()].lastAcked =
              agentIter.value.get("lastAckedTime").getDouble();
        }
      }
    }
  }

  builder.add("ClusterId", store.get(rootPath->cluster()->vec()));
  {
    VPackObjectBuilder ob(&builder, "Health");

    VPackObjectIterator memberIter(store.get(rootPath->supervision()->health()->vec()));
    for (auto member : memberIter) {
      std::string serverId = member.key.copyString();

      {
        VPackObjectBuilder obMember(&builder, serverId);

        builder.add(VPackObjectIterator(member.value));
        if (serverId.compare(0, 4, "PRMR", 4) == 0) {
          builder.add("Role", VPackValue("DBServer"));
          builder.add("CanBeDeleted",
                      VPackValue(VPackValue(member.value.get("Status").isEqualString("FAILED") &&
                                            canBeDeleted->count(serverId) == 1)));
        } else if (serverId.compare(0, 4, "CRDN", 4) == 0) {
          builder.add("Role", VPackValue("Coordinator"));
          builder.add("CanBeDeleted", VPackValue(member.value.get("Status").isEqualString("FAILED")));
        }
      }
    }

    for (auto& memberTry : config) {
      // this should always be true since this future is always fulfilled (even when an exception is thrown)
      TRI_ASSERT(memberTry.hasValue());

      auto& member = memberTry.get();

      {
        VPackObjectBuilder obMember(&builder, member.name);

        builder.add("Role", VPackValue("Agent"));
        builder.add("Endpoint", VPackValue(member.endpoint));
        builder.add("CanBeDeleted", VPackValue(false));

        // check for additional information
        auto info = agents.find(member.name);
        if (info != agents.end()) {
          builder.add("Leading", VPackValue(info->second.leader));
          builder.add("LastAckedTime", VPackValue(info->second.lastAcked));
        }

        if (member.response.hasValue()) {
          if (auto& response = member.response.get();
              response.ok() && response.response->statusCode() == fuerte::StatusOK) {
            VPackSlice localConfig = response.slice();
            builder.add("Engine", localConfig.get("engine"));
            builder.add("Version", localConfig.get("version"));
            builder.add("Leader", localConfig.get("leaderId"));
            builder.add("Status", VPackValue("GOOD"));
          } else {
            builder.add("Status", VPackValue("BAD"));
          }
        } else {
          builder.add("Status", VPackValue("BAD"));
        }
      }
    }
  }
}

}  // namespace

RestAdminClusterHandler::RestAdminClusterHandler(application_features::ApplicationServer& server,
                                                 GeneralRequest* request,
                                                 GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

std::string const RestAdminClusterHandler::Health = "health";
std::string const RestAdminClusterHandler::NumberOfServers = "numberOfServers";
std::string const RestAdminClusterHandler::Maintenance = "maintenance";
std::string const RestAdminClusterHandler::NodeVersion = "nodeVersion";
std::string const RestAdminClusterHandler::NodeEngine = "nodeEngine";
std::string const RestAdminClusterHandler::NodeStatistics = "nodeStatistics";
std::string const RestAdminClusterHandler::Statistics = "statistics";
std::string const RestAdminClusterHandler::ShardDistribution =
    "shardDistribution";
std::string const RestAdminClusterHandler::CollectionShardDistribution =
    "collectionShardDistribution";
std::string const RestAdminClusterHandler::CleanoutServer = "cleanOutServer";
std::string const RestAdminClusterHandler::ResignLeadership =
    "resignLeadership";
std::string const RestAdminClusterHandler::MoveShard = "moveShard";
std::string const RestAdminClusterHandler::QueryJobStatus = "queryAgencyJob";
std::string const RestAdminClusterHandler::RemoveServer = "removeServer";
std::string const RestAdminClusterHandler::RebalanceShards = "rebalanceShards";

RestStatus RestAdminClusterHandler::execute() {
  // No more check for admin rights here, since we handle this in every individual
  // method below. Some of them do no longer require admin access
  // (e.g. /_admin/cluster/health). If you add a new API below here, please
  // make sure to check for permissions!

  auto const& suffixes = request()->suffixes();
  size_t const len = suffixes.size();

  if (len == 1) {
    std::string const& command = suffixes.at(0);

    if (command == Health) {
      return handleHealth();
    } else if (command == NumberOfServers) {
      return handleNumberOfServers();
    } else if (command == Maintenance) {
      return handleMaintenance();
    } else if (command == NodeVersion) {
      return handleNodeVersion();
    } else if (command == NodeEngine) {
      return handleNodeEngine();
    } else if (command == NodeStatistics) {
      return handleNodeStatistics();
    } else if (command == Statistics) {
      return handleStatistics();
    } else if (command == ShardDistribution) {
      return handleShardDistribution();
    } else if (command == CollectionShardDistribution) {
      return handleCollectionShardDistribution();
    } else if (command == CleanoutServer) {
      return handleCleanoutServer();
    } else if (command == ResignLeadership) {
      return handleResignLeadership();
    } else if (command == MoveShard) {
      return handleMoveShard();
    } else if (command == QueryJobStatus) {
      return handleQueryJobStatus();
    } else if (command == RemoveServer) {
      return handleRemoveServer();
    } else if (command == RebalanceShards) {
      return handleRebalanceShards();
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    std::string("invalid command '") + command + "'");
      return RestStatus::DONE;
    }
  }

  generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                "expecting URL /_admin/cluster/<command>");
  return RestStatus::DONE;
}

RestAdminClusterHandler::FutureVoid RestAdminClusterHandler::retryTryDeleteServer(
    std::unique_ptr<RemoveServerContext>&& ctx) {
  if (++ctx->tries < 60) {
    return SchedulerFeature::SCHEDULER->delay(1s).thenValue(
        [this, ctx = std::move(ctx)](auto) mutable {
          return tryDeleteServer(std::move(ctx));
        });
  } else {
    generateError(rest::ResponseCode::PRECONDITION_FAILED, TRI_ERROR_HTTP_PRECONDITION_FAILED,
                  "server may not be deleted");
    return futures::makeFuture();
  }
}

RestAdminClusterHandler::FutureVoid RestAdminClusterHandler::tryDeleteServer(
    std::unique_ptr<RemoveServerContext>&& ctx) {
  auto rootPath = arangodb::cluster::paths::root()->arango();
  VPackBuffer<uint8_t> trx;
  {
    VPackBuilder builder(trx);
    arangodb::agency::envelope<VPackBuilder>::create(builder)
        .read()
        .key(rootPath->supervision()->health()->str())
        .key(rootPath->plan()->str())
        .key(rootPath->current()->str())
        .done()
        .done();
  }

  return AsyncAgencyComm().sendReadTransaction(20s, std::move(trx)).thenValue([this, ctx = std::move(ctx)](AsyncAgencyCommResult&& result) mutable {
    auto rootPath = arangodb::cluster::paths::root()->arango();
    if (result.ok() && result.statusCode() == 200) {
      VPackSlice agency = result.slice().at(0);

      VPackSlice health = agency.get(
          rootPath->supervision()->health()->server(ctx->server)->status()->vec());
      if (!health.isNone()) {
        bool isResponsible =
            isServerResponsibleForSomething(ctx->server,
                                            agency.get(rootPath->plan()->vec()),
                                            agency.get(rootPath->current()->vec()));

        // if the server is still in the list, it was neither in plan nor in current
        if (isResponsible) {
          auto planVersionPath = rootPath->plan()->version();
          // do a write transaction if server is no longer used
          VPackBuffer<uint8_t> trx;
          {
            VPackBuilder builder(trx);
            arangodb::agency::envelope<VPackBuilder>::create(builder)

                .write()
                .remove(rootPath->plan()->coordinators()->server(ctx->server)->str())
                .remove(rootPath->plan()->dBServers()->server(ctx->server)->str())
                .remove(rootPath->current()
                            ->serversRegistered()
                            ->server(ctx->server)
                            ->str())
                .remove(rootPath->current()->dBServers()->server(ctx->server)->str())
                .remove(rootPath->current()->coordinators()->server(ctx->server)->str())
                .remove(rootPath->supervision()->health()->server(ctx->server)->str())
                .remove(rootPath->target()
                            ->mapUniqueToShortID()
                            ->server(ctx->server)
                            ->str())
                .remove(rootPath->current()->serversKnown()->server(ctx->server)->str())
                .set(rootPath->target()->removedServers()->server(ctx->server)->str(),
                     timepointToString(std::chrono::system_clock::now()))
                .precs()
                .isEqual(rootPath->supervision()
                             ->health()
                             ->server(ctx->server)
                             ->status()
                             ->str(),
                         "FAILED")
                .isEmpty(
                    rootPath->supervision()->dbServers()->server(ctx->server)->str())
                .isEqual(planVersionPath->str(), agency.get(planVersionPath->vec()))
                .done()
                .done();
          }

          return AsyncAgencyComm()
              .sendWriteTransaction(20s, std::move(trx))
              .thenValue([this, ctx = std::move(ctx)](AsyncAgencyCommResult&& result) mutable {
                if (result.ok()) {
                  if (result.statusCode() == fuerte::StatusOK) {
                    resetResponse(rest::ResponseCode::OK);
                    return futures::makeFuture();
                  } else if (result.statusCode() == fuerte::StatusPreconditionFailed) {
                    return retryTryDeleteServer(std::move(ctx));
                  }
                }
                generateError(result.asResult());
                return futures::makeFuture();
              });
        }

        return retryTryDeleteServer(std::move(ctx));
      } else {
        generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
      }
    } else {
      generateError(result.asResult());
    }

    return futures::makeFuture();
  });
}

RestStatus RestAdminClusterHandler::handlePostRemoveServer(std::string const& server) {
  auto ctx = std::make_unique<RemoveServerContext>(server);

  return waitForFuture(tryDeleteServer(std::move(ctx))
                           .thenError<VPackException>([this](VPackException const& e) {
                             generateError(Result{e.errorCode(), e.what()});
                           })
                           .thenError<std::exception>([this](std::exception const& e) {
                             generateError(rest::ResponseCode::SERVER_ERROR,
                                           TRI_ERROR_HTTP_SERVER_ERROR, e.what());
                           }));
}

RestStatus RestAdminClusterHandler::handleRemoveServer() {
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (request()->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  bool parseSuccess;
  VPackSlice body = parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return RestStatus::DONE;
  }

  VPackSlice server = VPackSlice::noneSlice();
  if (body.isString()) {
    server = body;
  } else if (body.isObject()) {
    server = body.get("server");
  }

  if (server.isString()) {
    std::string serverId = resolveServerNameID(server.copyString());
    return handlePostRemoveServer(serverId);
  }

  generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                "expecting string or object with key `server`");
  return RestStatus::DONE;
}

RestStatus RestAdminClusterHandler::handleCleanoutServer() {
  return handleSingleServerJob("cleanOutServer");
}

RestStatus RestAdminClusterHandler::handleResignLeadership() {
  return handleSingleServerJob("resignLeadership");
}

std::unique_ptr<RestAdminClusterHandler::MoveShardContext>
RestAdminClusterHandler::MoveShardContext::fromVelocyPack(VPackSlice slice) {
  if (slice.isObject()) {
    auto database = slice.get("database");
    auto collection = slice.get("collection");
    auto shard = slice.get("shard");
    auto fromServer = slice.get("fromServer");
    auto toServer = slice.get("toServer");
    auto remainsFollower = slice.get("remainsFollower");

    bool valid = collection.isString() && shard.isString() &&
                 fromServer.isString() && toServer.isString();

    std::string databaseStr = database.isString() ? database.copyString() : std::string{};
    if (valid) {
      return std::make_unique<MoveShardContext>(std::move(databaseStr),
                                                collection.copyString(),
                                                shard.copyString(),
                                                fromServer.copyString(),
                                                toServer.copyString(), std::string{},
                                                remainsFollower.isNone() || remainsFollower.isTrue());
    }
  }

  return nullptr;
}

RestStatus RestAdminClusterHandler::handleMoveShard() {
  if (!ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on coordinators");
    return RestStatus::DONE;
  }

  if (request()->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  bool parseSuccess;
  VPackSlice body = parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return RestStatus::DONE;
  }

  std::unique_ptr<MoveShardContext> ctx = MoveShardContext::fromVelocyPack(body);
  if (ctx) {
    if (ctx->database.empty()) {
      ctx->database = _vocbase.name();
    }

    auto const& exec = ExecContext::current();
    bool canAccess = exec.isAdminUser() ||
                     exec.collectionAuthLevel(ctx->database, ctx->collection) ==
                         auth::Level::RW;
    if (!canAccess) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "insufficent permissions on database to move shard");
      return RestStatus::DONE;
    }

    ctx->fromServer = resolveServerNameID(ctx->fromServer);
    ctx->toServer = resolveServerNameID(ctx->toServer);
    return handlePostMoveShard(std::move(ctx));
  }

  generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                "object with keys `database`, `collection`, `shard`, "
                "`fromServer` and `toServer` (all strings) expected");
  return RestStatus::DONE;
}

RestAdminClusterHandler::FutureVoid RestAdminClusterHandler::createMoveShard(
    std::unique_ptr<MoveShardContext>&& ctx, VPackSlice plan) {
  auto planPath = arangodb::cluster::paths::root()->arango()->plan();

  VPackSlice dbServers = plan.get(planPath->dBServers()->vec());
  bool serversFound = dbServers.isObject() && dbServers.hasKey(ctx->fromServer) &&
                      dbServers.hasKey(ctx->toServer);
  if (!serversFound) {
    generateError(ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "one or both dbservers not found");
    return futures::makeFuture();
  }

  VPackSlice collection = plan.get(planPath->collections()
                                       ->database(ctx->database)
                                       ->collection(ctx->collectionID)
                                       ->vec());
  if (!collection.isObject()) {
    generateError(ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "database/collection not found");
    return futures::makeFuture();
  }

  if (collection.hasKey("distributeShardsLike")) {
    generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "MoveShard only allowed for collections which have "
                  "distributeShardsLike unset.");
    return futures::makeFuture();
  }

  VPackSlice shard = collection.get(std::vector<std::string>{"shards", ctx->shard});
  if (!shard.isArray()) {
    generateError(ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "shard not found");
    return futures::makeFuture();
  }

  bool fromFound = false;
  bool isLeader = false;
  for (VPackArrayIterator i(shard); i != i.end(); i++) {
    if (i.value().isEqualString(ctx->fromServer)) {
      isLeader = i.isFirst();
      fromFound = true;
    }
  }

  if (!fromFound) {
    generateError(ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "shard is not located on the server");
    return futures::makeFuture();
  }

  std::string jobId =
      std::to_string(server().getFeature<ClusterFeature>().clusterInfo().uniqid());
  auto jobToDoPath =
      arangodb::cluster::paths::root()->arango()->target()->toDo()->job(jobId);

  VPackBuffer<uint8_t> trx;
  {
    VPackBuilder builder(trx);
    arangodb::agency::envelope<VPackBuilder>::create(builder)
        .write()
        .emplace(jobToDoPath->str(),
                 [&](VPackBuilder& builder) {
                   builder.add("type", VPackValue("moveShard"));
                   builder.add("database", VPackValue(ctx->database));
                   builder.add("collection", collection.get("id"));
                   builder.add("jobId", VPackValue(jobId));
                   builder.add("shard", VPackValue(ctx->shard));
                   builder.add("fromServer", VPackValue(ctx->fromServer));
                   builder.add("toServer", VPackValue(ctx->toServer));
                   builder.add("isLeader", VPackValue(isLeader));
                   builder.add("remainsFollower", isLeader ? VPackValue(ctx->remainsFollower) : VPackValue(false));
                   builder.add("creator", VPackValue(ServerState::instance()->getId()));
                   builder.add("timeCreated", VPackValue(timepointToString(
                                                  std::chrono::system_clock::now())));
                 })
        .done()
        .done();
  }

  return AsyncAgencyComm()
      .sendWriteTransaction(20s, std::move(trx))
      .thenValue([this, ctx = std::move(ctx),
                  jobId = std::move(jobId)](AsyncAgencyCommResult&& result) {
        if (result.ok() && result.statusCode() == fuerte::StatusOK) {
          VPackBuffer<uint8_t> payload;
          {
            VPackBuilder builder(payload);
            VPackObjectBuilder ob(&builder);
            builder.add(StaticStrings::Error, VPackValue(false));
            builder.add("code", VPackValue(int(ResponseCode::ACCEPTED)));
            builder.add("id", VPackValue(jobId));
          }

          resetResponse(rest::ResponseCode::ACCEPTED);
          response()->setPayload(std::move(payload));

        } else {
          generateError(result.asResult());
        }
      });
}

RestStatus RestAdminClusterHandler::handlePostMoveShard(std::unique_ptr<MoveShardContext>&& ctx) {
  std::shared_ptr<LogicalCollection> collection =
      server().getFeature<ClusterFeature>().clusterInfo().getCollectionNT(ctx->database,
                                                                          ctx->collection);

  if (collection == nullptr) {
    generateError(ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "database/collection not found");
    return RestStatus::DONE;
  }

  // base64-encode collection id with RevisionId
  ctx->collectionID = RevisionId{collection->planId().id()}.toString();
  auto planPath = arangodb::cluster::paths::root()->arango()->plan();

  VPackBuffer<uint8_t> trx;
  {
    VPackBuilder builder(trx);
    arangodb::agency::envelope<VPackBuilder>::create(builder)
        .read()
        .key(planPath->dBServers()->str())
        .key(planPath->collections()
                 ->database(ctx->database)
                 ->collection(ctx->collectionID)
                 ->str())
        .done()
        .done();
  }

  // gather information about that shard
  return waitForFuture(
      AsyncAgencyComm()
          .sendReadTransaction(20s, std::move(trx))
          .thenValue([this, ctx = std::move(ctx)](AsyncAgencyCommResult&& result) mutable {
            if (result.ok()) {
              switch (result.statusCode()) {
                case fuerte::StatusOK:
                  return createMoveShard(std::move(ctx), result.slice().at(0));
                case fuerte::StatusNotFound:
                  generateError(rest::ResponseCode::NOT_FOUND,
                                TRI_ERROR_HTTP_NOT_FOUND, "unknown collection");
                  return futures::makeFuture();
                default:
                  break;
              }
            }

            generateError(result.asResult());
            return futures::makeFuture();
          })
          .thenError<VPackException>([this](VPackException const& e) {
            generateError(Result{e.errorCode(), e.what()});
          })
          .thenError<std::exception>([this](std::exception const& e) {
            generateError(rest::ResponseCode::SERVER_ERROR,
                          TRI_ERROR_HTTP_SERVER_ERROR, e.what());
          }));
}

RestStatus RestAdminClusterHandler::handleQueryJobStatus() {
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (!ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on coordinators");
    return RestStatus::DONE;
  }

  if (request()->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  std::string jobId = request()->value("id");
  if (jobId.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "missing id parameter");
    return RestStatus::DONE;
  }

  auto targetPath = arangodb::cluster::paths::root()->arango()->target();
  std::vector<std::string> paths = {
      targetPath->pending()->job(jobId)->str(),
      targetPath->failed()->job(jobId)->str(),
      targetPath->finished()->job(jobId)->str(),
      targetPath->toDo()->job(jobId)->str(),
  };

  return waitForFuture(
      AsyncAgencyComm()
          .sendTransaction(20s, AgencyReadTransaction{std::move(paths)})
          .thenValue([this, targetPath, jobId = std::move(jobId)](AsyncAgencyCommResult&& result) {
            if (result.ok() && result.statusCode() == fuerte::StatusOK) {
              auto paths = std::vector{
                  targetPath->pending()->job(jobId)->vec(),
                  targetPath->failed()->job(jobId)->vec(),
                  targetPath->finished()->job(jobId)->vec(),
                  targetPath->toDo()->job(jobId)->vec(),
              };

              for (auto const& path : paths) {
                VPackSlice job = result.slice().at(0).get(path);

                if (job.isObject()) {
                  VPackBuffer<uint8_t> payload;
                  {
                    VPackBuilder builder(payload);
                    VPackObjectBuilder ob(&builder);

                    // append all the job keys
                    builder.add(VPackObjectIterator(job));
                    builder.add("error", VPackValue(false));
                    builder.add("job", VPackValue(jobId));
                    builder.add("status", VPackValue(path[2]));
                  }

                  resetResponse(rest::ResponseCode::OK);
                  response()->setPayload(std::move(payload));
                  return;
                }
              }

              generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
            } else {
              generateError(result.asResult());
            }
          })
          .thenError<VPackException>([this](VPackException const& e) {
            generateError(Result{e.errorCode(), e.what()});
          })
          .thenError<std::exception>([this](std::exception const& e) {
            generateError(rest::ResponseCode::SERVER_ERROR,
                          TRI_ERROR_HTTP_SERVER_ERROR, e.what());
          }));
}

RestStatus RestAdminClusterHandler::handleSingleServerJob(std::string const& job) {
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (!ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on coordinators");
    return RestStatus::DONE;
  }

  if (request()->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  bool parseSuccess;
  VPackSlice body = parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return RestStatus::DONE;
  }

  if (body.isObject()) {
    VPackSlice server = body.get("server");
    if (server.isString()) {
      std::string serverId = resolveServerNameID(server.copyString());
      return handleCreateSingleServerJob(job, serverId);
    }
  }

  generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                "object with key `server`");
  return RestStatus::DONE;
}

RestStatus RestAdminClusterHandler::handleCreateSingleServerJob(std::string const& job,
                                                                std::string const& serverId) {
  std::string jobId =
      std::to_string(server().getFeature<ClusterFeature>().clusterInfo().uniqid());
  auto jobToDoPath =
      arangodb::cluster::paths::root()->arango()->target()->toDo()->job(jobId);

  VPackBuilder builder;
  {
    VPackObjectBuilder ob(&builder);
    builder.add("type", VPackValue(job));
    builder.add("server", VPackValue(serverId));
    builder.add("jobId", VPackValue(jobId));
    builder.add("creator", VPackValue(ServerState::instance()->getId()));
    builder.add("timeCreated",
                VPackValue(timepointToString(std::chrono::system_clock::now())));
  }

  return waitForFuture(
      AsyncAgencyComm()
          .setValue(20s, jobToDoPath, builder.slice())
          .thenValue([this, jobId = std::move(jobId)](AsyncAgencyCommResult&& result) {
            if (result.ok() && result.statusCode() == 200) {
              VPackBuffer<uint8_t> payload;
              {
                VPackBuilder builder(payload);
                VPackObjectBuilder ob(&builder);
                builder.add("error", VPackValue(false));
                builder.add("code", VPackValue(int(ResponseCode::ACCEPTED)));
                builder.add("id", VPackValue(jobId));
              }

              resetResponse(rest::ResponseCode::ACCEPTED);
              response()->setPayload(std::move(payload));
            } else {
              generateError(result.asResult());
            }
          })
          .thenError<VPackException>([this](VPackException const& e) {
            generateError(Result{e.errorCode(), e.what()});
          })
          .thenError<std::exception>([this](std::exception const& e) {
            generateError(rest::ResponseCode::SERVER_ERROR,
                          TRI_ERROR_HTTP_SERVER_ERROR, e.what());
          }));
}

RestStatus RestAdminClusterHandler::handleProxyGetRequest(std::string const& url,
                                                          std::string const& serverFromParameter) {
  if (!ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on coordinators");
    return RestStatus::DONE;
  }

  if (request()->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  std::string const& serverId = request()->value(serverFromParameter);
  if (serverId.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  std::string("missing parameter `") + serverFromParameter +
                      "`");
    return RestStatus::DONE;
  }

  auto* pool = server().getFeature<NetworkFeature>().pool();

  network::RequestOptions opt;
  opt.timeout = 10s;
  auto frequest =
      network::sendRequestRetry(pool, "server:" + serverId, fuerte::RestVerb::Get,
                                url, VPackBuffer<uint8_t>(), opt);

  return waitForFuture(
      std::move(frequest)
          .thenValue([this](network::Response&& result) {
            if (result.ok()) {
              resetResponse(ResponseCode(result.statusCode()));  // I quit if the values are not the HTTP Status Codes
              auto payload = result.response->stealPayload();
              response()->setPayload(std::move(*payload));
            } else {
              switch (result.error) {
                case fuerte::Error::ConnectionCanceled:
                  generateError(rest::ResponseCode::BAD,
                                TRI_ERROR_HTTP_BAD_PARAMETER, "unknown server");
                  break;
                case fuerte::Error::CouldNotConnect:
                case fuerte::Error::RequestTimeout:
                  generateError(rest::ResponseCode::REQUEST_TIMEOUT, TRI_ERROR_HTTP_GATEWAY_TIMEOUT,
                                "server did not answer");
                  break;
                default:
                  generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR);
              }
            }
          })
          .thenError<VPackException>([this](VPackException const& e) {
            generateError(Result{e.errorCode(), e.what()});
          })
          .thenError<std::exception>([this](std::exception const& e) {
            generateError(rest::ResponseCode::SERVER_ERROR,
                          TRI_ERROR_HTTP_SERVER_ERROR, e.what());
          }));
}

RestStatus RestAdminClusterHandler::handleNodeVersion() {
  return handleProxyGetRequest("/_api/version", "ServerID");
}

RestStatus RestAdminClusterHandler::handleNodeStatistics() {
  return handleProxyGetRequest("/_admin/statistics", "ServerID");
}

RestStatus RestAdminClusterHandler::handleNodeEngine() {
  return handleProxyGetRequest("/_api/engine", "ServerID");
}

RestStatus RestAdminClusterHandler::handleStatistics() {
  return handleProxyGetRequest("/_admin/statistics", "DBserver");
}

RestStatus RestAdminClusterHandler::handleShardDistribution() {
  if (!ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on coordinators");
    return RestStatus::DONE;
  }

  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (request()->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  auto reporter = cluster::ShardDistributionReporter::instance(server());
  VPackBuffer<uint8_t> resultBody;
  {
    VPackBuilder result(resultBody);
    VPackObjectBuilder body(&result);
    result.add(VPackValue("results"));
    reporter->getDistributionForDatabase(_vocbase.name(), result);
    result.add(StaticStrings::Error, VPackValue(false));
    result.add(StaticStrings::Code, VPackValue(200));
  }
  resetResponse(rest::ResponseCode::OK);
  response()->setPayload(std::move(resultBody));
  return RestStatus::DONE;
}

RestStatus RestAdminClusterHandler::handleGetCollectionShardDistribution(std::string const& collection) {
  if (collection.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expected nonempty `collection` parameter");
    return RestStatus::DONE;
  }

  auto reporter = cluster::ShardDistributionReporter::instance(server());
  VPackBuffer<uint8_t> resultBody;
  {
    VPackBuilder result(resultBody);
    VPackObjectBuilder body(&result);
    result.add(VPackValue("results"));
    reporter->getCollectionDistributionForDatabase(_vocbase.name(), collection, result);
    result.add(StaticStrings::Error, VPackValue(false));
    result.add(StaticStrings::Code, VPackValue(200));
  }
  resetResponse(rest::ResponseCode::OK);
  response()->setPayload(std::move(resultBody));
  return RestStatus::DONE;
}

RestStatus RestAdminClusterHandler::handleCollectionShardDistribution() {
  if (!ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on coordinators");
    return RestStatus::DONE;
  }

  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  switch (request()->requestType()) {
    case rest::RequestType::GET:
      return handleGetCollectionShardDistribution(
          request()->value("collection"));
    case rest::RequestType::PUT:
      break;
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
      return RestStatus::DONE;
  }

  bool parseSuccess;
  VPackSlice body = parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return RestStatus::DONE;
  }

  if (body.isObject()) {
    VPackSlice collection = body.get("collection");
    if (collection.isString()) {
      return handleGetCollectionShardDistribution(collection.copyString());
    }
  }

  generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                "object with key `collection`");
  return RestStatus::DONE;
}

RestStatus RestAdminClusterHandler::handleGetMaintenance() {
  if (AsyncAgencyCommManager::INSTANCE == nullptr) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on single server with active failover");
    return RestStatus::DONE;
  }

  auto maintenancePath =
      arangodb::cluster::paths::root()->arango()->supervision()->state()->mode();

  return waitForFuture(
      AsyncAgencyComm()
          .getValues(maintenancePath)
          .thenValue([this](AgencyReadResult&& result) {
            if (result.ok() && result.statusCode() == fuerte::StatusOK) {
              VPackBuffer<uint8_t> body;
              {
                VPackBuilder bodyBuilder(body);
                VPackObjectBuilder ob(&bodyBuilder);
                bodyBuilder.add(StaticStrings::Error, VPackValue(false));
                bodyBuilder.add("result", result.value());
              }  // use generateOk instead

              resetResponse(rest::ResponseCode::OK);
              response()->setPayload(std::move(body));
            } else {
              generateError(result.asResult());
            }
          })
          .thenError<VPackException>([this](VPackException const& e) {
            generateError(Result{e.errorCode(), e.what()});
          })
          .thenError<std::exception>([this](std::exception const& e) {
            generateError(rest::ResponseCode::SERVER_ERROR,
                          TRI_ERROR_HTTP_SERVER_ERROR, e.what());
          }));
}

RestAdminClusterHandler::FutureVoid RestAdminClusterHandler::waitForSupervisionState(
    bool state, clock::time_point startTime) {
  if (startTime == clock::time_point()) {
    startTime = clock::now();
  }

  return SchedulerFeature::SCHEDULER->delay(1s)
      .thenValue([](auto) {
        return AsyncAgencyComm().getValues(
            arangodb::cluster::paths::root()->arango()->supervision()->state()->mode());
      })
      .thenValue([this, state, startTime](AgencyReadResult&& result) {
        auto waitFor = state ? "Maintenance" : "Normal";
        if (result.ok() && result.statusCode() == fuerte::StatusOK) {
          if (!result.value().isEqualString(waitFor)) {
            if ((clock::now() - startTime) < 120.0s) {
              // wait again
              return waitForSupervisionState(state, startTime);
            }

            generateError(rest::ResponseCode::REQUEST_TIMEOUT,
                          TRI_ERROR_HTTP_GATEWAY_TIMEOUT, std::string{"timed out while waiting for supervision to go into maintenance mode"});
          } else {
            auto msg = state ? "Cluster supervision deactivated. It will be "
                               "reactivated automatically in 60 minutes unless "
                               "this call is repeated until then."
                             : "Cluster supervision reactivated.";
            VPackBuffer<uint8_t> body;
            {
              VPackBuilder bodyBuilder(body);
              VPackObjectBuilder ob(&bodyBuilder);
              bodyBuilder.add(StaticStrings::Error, VPackValue(false));
              bodyBuilder.add("warning", VPackValue(msg));
            }

            resetResponse(rest::ResponseCode::OK);
            response()->setPayload(std::move(body));
          }
        } else {
          generateError(result.asResult());
        }

        return futures::makeFuture();
      });
}

RestStatus RestAdminClusterHandler::setMaintenance(bool wantToActive) {
  auto maintenancePath =
      arangodb::cluster::paths::root()->arango()->supervision()->maintenance();

  auto sendTransaction = [&] {
    if (wantToActive) {
      return AsyncAgencyComm().setValue(60s, maintenancePath, VPackValue(true), 3600);
    } else {
      return AsyncAgencyComm().deleteKey(60s, maintenancePath);
    }
  };

  auto self(shared_from_this());

  return waitForFuture(sendTransaction()
                           .thenValue([this, wantToActive](AsyncAgencyCommResult&& result) {
                             if (result.ok() && result.statusCode() == 200) {
                               return waitForSupervisionState(wantToActive);
                             } else {
                               generateError(result.asResult());
                             }
                             return futures::makeFuture();
                           })
                           .thenError<VPackException>([this](VPackException const& e) {
                             generateError(Result{e.errorCode(), e.what()});
                           })
                           .thenError<std::exception>([this](std::exception const& e) {
                             generateError(rest::ResponseCode::SERVER_ERROR,
                                           TRI_ERROR_HTTP_SERVER_ERROR, e.what());
                           }));
}

RestStatus RestAdminClusterHandler::handlePutMaintenance() {
  if (AsyncAgencyCommManager::INSTANCE == nullptr) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on single server with active failover");
    return RestStatus::DONE;
  }

  bool parseSuccess;
  VPackSlice body = parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return RestStatus::DONE;
  }

  if (body.isString()) {
    if (body.isEqualString("on")) {
      return setMaintenance(true);
    } else if (body.isEqualString("off")) {
      return setMaintenance(false);
    }
  }

  generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                "string expected with value `on` or `off`");
  return RestStatus::DONE;
}

RestStatus RestAdminClusterHandler::handleMaintenance() {
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (!ServerState::instance()->isCoordinator() &&
      !ServerState::instance()->isSingleServer()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on single server and coordinators");
    return RestStatus::DONE;
  }

  if (AsyncAgencyCommManager::INSTANCE == nullptr) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on single server with active failover");
    return RestStatus::DONE;
  }

  switch (request()->requestType()) {
    case rest::RequestType::GET:
      return handleGetMaintenance();
    case rest::RequestType::PUT:
      return handlePutMaintenance();
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
      return RestStatus::DONE;
  }
}

RestStatus RestAdminClusterHandler::handleGetNumberOfServers() {
  if (AsyncAgencyCommManager::INSTANCE == nullptr) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on single server with active failover");
    return RestStatus::DONE;
  }

  auto targetPath = arangodb::cluster::paths::root()->arango()->target();

  VPackBuffer<uint8_t> trx;
  {
    VPackBuilder builder(trx);
    arangodb::agency::envelope<VPackBuilder>::create(builder)
        .read()
        .key(targetPath->numberOfDBServers()->str())
        .key(targetPath->numberOfCoordinators()->str())
        .key(targetPath->cleanedServers()->str())
        .done()
        .done();
  }

  auto self(shared_from_this());
  return waitForFuture(
      AsyncAgencyComm()
          .sendReadTransaction(10.0s, std::move(trx))
          .thenValue([this](AsyncAgencyCommResult&& result) {
            auto targetPath = arangodb::cluster::paths::root()->arango()->target();

            if (result.ok() && result.statusCode() == fuerte::StatusOK) {
              VPackBuffer<uint8_t> body;
              {
                VPackBuilder builder(body);
                VPackObjectBuilder ob(&builder);
                builder.add("numberOfDBServers",
                            result.slice().at(0).get(
                                targetPath->numberOfDBServers()->vec()));
                builder.add("numberOfCoordinators",
                            result.slice().at(0).get(
                                targetPath->numberOfCoordinators()->vec()));
                builder.add("cleanedServers", result.slice().at(0).get(
                                                  targetPath->cleanedServers()->vec()));
                builder.add(StaticStrings::Error, VPackValue(false));
                builder.add(StaticStrings::Code, VPackValue(200));
              }

              resetResponse(rest::ResponseCode::OK);
              response()->setPayload(std::move(body));
            } else {
              generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR,
                            "agency communication failed");
            }
          })
          .thenError<VPackException>([this](VPackException const& e) {
            generateError(Result{e.errorCode(), e.what()});
          })
          .thenError<std::exception>([this](std::exception const& e) {
            generateError(rest::ResponseCode::SERVER_ERROR,
                          TRI_ERROR_HTTP_SERVER_ERROR, e.what());
          }));
}

RestStatus RestAdminClusterHandler::handlePutNumberOfServers() {
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (AsyncAgencyCommManager::INSTANCE == nullptr) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on single server with active failover");
    return RestStatus::DONE;
  }

  bool parseSuccess;
  VPackSlice body = parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return RestStatus::DONE;
  }

  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "object expected");
    return RestStatus::DONE;
  }

  std::vector<AgencyOperation> ops;
  auto targetPath = arangodb::cluster::paths::root()->arango()->target();
  bool hasThingsToDo = false;

  VPackBuffer<uint8_t> trx;
  {
    VPackBuilder builder(trx);
    auto write = arangodb::agency::envelope<VPackBuilder>::create(builder).write();

    VPackSlice numberOfCoordinators = body.get("numberOfCoordinators");
    if (numberOfCoordinators.isNumber() || numberOfCoordinators.isNull()) {
      write = std::move(write).set(targetPath->numberOfCoordinators()->str(),
                                   numberOfCoordinators);
      hasThingsToDo = true;
    } else if (!numberOfCoordinators.isNone()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "numberOfCoordinators: number expected");
      return RestStatus::DONE;
    }

    VPackSlice numberOfDBServers = body.get("numberOfDBServers");
    if (numberOfDBServers.isNumber() || numberOfDBServers.isNull()) {
      write = std::move(write).set(targetPath->numberOfDBServers()->str(), numberOfDBServers);
      hasThingsToDo = true;
    } else if (!numberOfDBServers.isNone()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "numberOfDBServers: number expected");
      return RestStatus::DONE;
    }

    VPackSlice cleanedServers = body.get("cleanedServers");
    if (cleanedServers.isArray()) {
      bool allStrings = true;
      for (auto server : VPackArrayIterator(cleanedServers)) {
        if (!server.isString()) {
          allStrings = false;
          break;
        }
      }

      if (allStrings) {
        write = std::move(write).set(targetPath->cleanedServers()->str(), cleanedServers);
        hasThingsToDo = true;
      } else {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                      "cleanedServers: array of strings expected");
        return RestStatus::DONE;
      }
    } else if (!cleanedServers.isNone()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "cleanedServers: array expected");
      return RestStatus::DONE;
    }

    std::move(write).done().done();
  }

  if (!hasThingsToDo) {
    generateOk(rest::ResponseCode::OK, velocypack::Slice::noneSlice());
    // TODO: the appropriate response would rather be
    // generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
    //               "missing fields");
    // but that would break API compatibility. Introduce this behavior
    // in 4.0!!
    return RestStatus::DONE;
  }

  auto self(shared_from_this());
  return waitForFuture(
      AsyncAgencyComm()
          .sendWriteTransaction(20s, std::move(trx))
          .thenValue([this](AsyncAgencyCommResult&& result) {
            if (result.ok() && result.statusCode() == fuerte::StatusOK) {
              VPackBuffer<uint8_t> responseBody;
              {
                VPackBuilder builder(responseBody);
                VPackObjectBuilder ob(&builder);
                builder.add(StaticStrings::Error, VPackValue(false));
                builder.add("code", VPackValue(200));
              }
              response()->setPayload(std::move(responseBody));
              resetResponse(rest::ResponseCode::OK);
            } else {
              generateError(result.asResult());
            }
          })
          .thenError<VPackException>([this](VPackException const& e) {
            generateError(Result{e.errorCode(), e.what()});
          })
          .thenError<std::exception>([this](std::exception const& e) {
            generateError(rest::ResponseCode::SERVER_ERROR,
                          TRI_ERROR_HTTP_SERVER_ERROR, e.what());
          }));
}

RestStatus RestAdminClusterHandler::handleNumberOfServers() {
  if (!ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on coordinators");
    return RestStatus::DONE;
  }
 
  // GET requests are allowed for everyone, unless --server.harden is used.
  // in this case admin privileges are required.
  // PUT requests always require admin privileges
  ServerSecurityFeature& security = server().getFeature<ServerSecurityFeature>();
  bool const needsAdminPrivileges = 
      (request()->requestType() != rest::RequestType::GET || security.isRestApiHardened());

  if (needsAdminPrivileges &&
      !ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  switch (request()->requestType()) {
    case rest::RequestType::GET:
      return handleGetNumberOfServers();
    case rest::RequestType::PUT:
      return handlePutNumberOfServers();
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
      return RestStatus::DONE;
  }
}

RestStatus RestAdminClusterHandler::handleHealth() {
  // We allow this API whenever one is authenticated in some way. There used
  // to be a check for isAdminUser here. However, we want that the UI with
  // the cluster health dashboard works for every authenticated user.
  if (request()->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  if (!ServerState::instance()->isCoordinator() &&
      !ServerState::instance()->isSingleServer()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on single server and coordinators");
    return RestStatus::DONE;
  }

  if (AsyncAgencyCommManager::INSTANCE == nullptr) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on single server with active failover");
    return RestStatus::DONE;
  }

  // TODO handle timeout parameter

  auto self(shared_from_this());

  // query the agency config
  auto fConfig =
      AsyncAgencyComm()
          .sendWithFailover(fuerte::RestVerb::Get, "/_api/agency/config", 60.0s,
                            AsyncAgencyComm::RequestType::READ, VPackBuffer<uint8_t>())
          .thenValue([self](AsyncAgencyCommResult&& result) {
            // this lambda has to capture self since collect returns early on an
            // exception and the RestHandle might be freed to early otherwise

            if (result.fail() || result.statusCode() != fuerte::StatusOK) {
              THROW_ARANGO_EXCEPTION(result.asResult());
            }

            // now connect to all the members and ask for their engine and version
            std::vector<futures::Future<::agentConfigHealthResult>> fs;

            auto* pool = self->server().getFeature<NetworkFeature>().pool();
            for (auto member : VPackObjectIterator(result.slice().get(
                     std::vector<std::string>{"configuration", "pool"}))) {
              std::string endpoint = member.value.copyString();
              std::string memberName = member.key.copyString();

              auto future =
                  network::sendRequest(pool, endpoint, fuerte::RestVerb::Get,
                                       "/_api/agency/config", VPackBuffer<uint8_t>())
                      .then([endpoint = std::move(endpoint), memberName = std::move(memberName)](
                                futures::Try<network::Response>&& resp) mutable {
                        return futures::makeFuture(
                            ::agentConfigHealthResult{std::move(endpoint), std::move(memberName),
                                                      std::move(resp)});
                      });

              fs.emplace_back(std::move(future));
            }

            return futures::collectAll(fs);
          });

  // query information from the store
  auto rootPath = arangodb::cluster::paths::root()->arango();
  VPackBuffer<uint8_t> trx;
  {
    VPackBuilder builder(trx);
    arangodb::agency::envelope<VPackBuilder>::create(builder)
        .read()
        .key(rootPath->cluster()->str())
        .key(rootPath->supervision()->health()->str())
        .key(rootPath->plan()->str())
        .key(rootPath->current()->str())
        .done()
        .done();
  }
  auto fStore = AsyncAgencyComm().sendReadTransaction(60.0s, std::move(trx));

  return waitForFuture(
      futures::collect(std::move(fConfig), std::move(fStore))
          .thenValue([this](auto&& result) {
            auto rootPath = arangodb::cluster::paths::root()->arango();
            auto& [configResult, storeResult] = result;
            if (storeResult.ok() && storeResult.statusCode() == fuerte::StatusOK) {
              VPackBuffer<uint8_t> responseBody;
              {
                VPackBuilder builder(responseBody);
                VPackObjectBuilder ob(&builder);
                ::buildHealthResult(builder, configResult, storeResult.slice().at(0));
                builder.add(StaticStrings::Error, VPackValue(false));
                builder.add(StaticStrings::Code, VPackValue(200));
              }
              resetResponse(rest::ResponseCode::OK);
              response()->setPayload(std::move(responseBody));
            } else {
              generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR,
                            "agency communication failed");
            }
          })
          .thenError<VPackException>([this](VPackException const& e) {
            generateError(Result{e.errorCode(), e.what()});
          })
          .thenError<std::exception>([this](std::exception const& e) {
            generateError(rest::ResponseCode::SERVER_ERROR,
                          TRI_ERROR_HTTP_SERVER_ERROR, e.what());
          }));
}

std::string RestAdminClusterHandler::resolveServerNameID(std::string const& serverName) {
  auto servers = server().getFeature<ClusterFeature>().clusterInfo().getServerAliases();

  for (auto const& pair : servers) {
    if (pair.second == serverName) {
      return pair.first;
    }
  }

  return serverName;
}

namespace std {
template <>
struct hash<RestAdminClusterHandler::CollectionShardPair> {
  std::size_t operator()(RestAdminClusterHandler::CollectionShardPair const& a) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, std::hash<std::string>()(a.collection));
    boost::hash_combine(seed, std::hash<std::string>()(a.shard));
    return seed;
  }
};
}  // namespace std

void RestAdminClusterHandler::getShardDistribution(
    std::map<std::string, std::unordered_set<CollectionShardPair>>& distr) {
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();

  for (auto const& server : ci.getCurrentDBServers()) {
    distr[server].clear();
  }

  for (auto const& collection : ci.getCollections(_vocbase.name())) {
    if (!collection->distributeShardsLike().empty()) {
      continue;
    }
    // base64-encode collection id with RevisionId
    std::string collectionID = RevisionId{collection->planId().id()}.toString();
    auto shardIds = collection->shardIds();
    for (auto const& shard : *shardIds) {
      for (size_t i = 0; i < shard.second.size(); i++) {
        std::string const& server = shard.second.at(i);
        distr[server].emplace(CollectionShardPair{collectionID, shard.first, i == 0});
      }
    }
  }
}

namespace {

using CollectionShardPair = RestAdminClusterHandler::CollectionShardPair;
using MoveShardDescription = RestAdminClusterHandler::MoveShardDescription;

void theSimpleStupidOne(std::map<std::string, std::unordered_set<CollectionShardPair>>& shardMap,
                        std::vector<MoveShardDescription>& moves) {
  // If you dislike this algorithm feel free to add a new one.
  // shardMap is a map from dbserver to a set of shards located on that server.
  // your algorithm has to fill `moves` with the move shard operations that it wants to execute.
  // Please fill in all values of the `MoveShardDescription` struct.

  std::unordered_set<std::string> movedShards;
  while (moves.size() < 10) {
    auto [emptiest, fullest] =
        std::minmax_element(shardMap.begin(), shardMap.end(),
                            [](auto const& a, auto const& b) {
                              return a.second.size() < b.second.size();
                            });

    if (emptiest->second.size() + 1 >= fullest->second.size()) {
      break;
    }

    auto pair = fullest->second.begin();
    for (; pair != fullest->second.end(); pair++) {
      if (movedShards.count(pair->shard) != 0) {
        continue;
      }

      auto i = std::find_if(emptiest->second.begin(), emptiest->second.end(),
                            [&](CollectionShardPair const& p) {
                              return p.shard == pair->shard;
                            });
      if (i == emptiest->second.end()) {
        break;
      }
    }

    if (pair == fullest->second.end()) {
      break;
    }

    // move the shard
    moves.push_back(MoveShardDescription{pair->collection, pair->shard, fullest->first,
                                         emptiest->first, pair->isLeader});
    movedShards.insert(pair->shard);
    emptiest->second.emplace(*pair);
    fullest->second.erase(pair);
  }
}
}  // namespace

RestAdminClusterHandler::FutureVoid RestAdminClusterHandler::handlePostRebalanceShards(
    const ReshardAlgorithm& algorithm) {
  // dbserver -> shards
  std::vector<MoveShardDescription> moves;
  std::map<std::string, std::unordered_set<CollectionShardPair>> shardMap;
  getShardDistribution(shardMap);

  algorithm(shardMap, moves);

  if (moves.empty()) {
    resetResponse(rest::ResponseCode::OK);
    return futures::makeFuture();
  }

  VPackBuffer<uint8_t> trx;
  {
    VPackBuilder builder(trx);
    auto write = arangodb::agency::envelope<VPackBuilder>::create(builder).write();

    auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
    std::string timestamp = timepointToString(std::chrono::system_clock::now());
    for (auto const& move : moves) {
      std::string jobId = std::to_string(ci.uniqid());
      auto jobToDoPath =
          arangodb::cluster::paths::root()->arango()->target()->toDo()->job(jobId);
      write = std::move(write).emplace(jobToDoPath->str(), [&](VPackBuilder& builder) {
        builder.add("type", VPackValue("moveShard"));
        builder.add("database", VPackValue(_vocbase.name()));
        builder.add("collection", VPackValue(move.collection));
        builder.add("jobId", VPackValue(jobId));
        builder.add("shard", VPackValue(move.shard));
        builder.add("fromServer", VPackValue(move.from));
        builder.add("toServer", VPackValue(move.to));
        builder.add("isLeader", VPackValue(move.isLeader));
        builder.add("remainsFollower", VPackValue(move.isLeader));
        builder.add("creator", VPackValue(ServerState::instance()->getId()));
        builder.add("timeCreated", VPackValue(timestamp));
      });
    }
    std::move(write).done().done();
  }

  return AsyncAgencyComm().sendWriteTransaction(20s, std::move(trx)).thenValue([this](AsyncAgencyCommResult&& result) {
    if (result.ok() && result.statusCode() == 200) {
      VPackBuffer<uint8_t> responseBody;
      {
        VPackBuilder builder(responseBody);
        VPackObjectBuilder ob(&builder);
        builder.add(StaticStrings::Error, VPackValue(false));
        builder.add("code", VPackValue(202));
      }
      resetResponse(rest::ResponseCode::ACCEPTED);
      response()->setPayload(std::move(responseBody));

    } else {
      generateError(result.asResult());
    }
  });
}

RestStatus RestAdminClusterHandler::handleRebalanceShards() {
  if (request()->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  if (!ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on coordinators");
    return RestStatus::DONE;
  }

  ExecContext const& exec = ExecContext::current();
  if (!exec.canUseDatabase(auth::Level::RW)) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "insufficent permissions");
    return RestStatus::DONE;
  }

  // ADD YOUR ALGORITHM HERE!!!

  std::string algorithmName = request()->value("algorithm");
  ReshardAlgorithm algorithm;
  if (algorithmName == "simple" || algorithmName.empty()) {
    algorithm = theSimpleStupidOne;
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "unknown algorithm");
    return RestStatus::DONE;
  }

  return waitForFuture(handlePostRebalanceShards(algorithm)
                           .thenError<VPackException>([this](VPackException const& e) {
                             generateError(Result{e.errorCode(), e.what()});
                           })
                           .thenError<std::exception>([this](std::exception const& e) {
                             generateError(rest::ResponseCode::SERVER_ERROR,
                                           TRI_ERROR_HTTP_SERVER_ERROR, e.what());
                           }));
}
