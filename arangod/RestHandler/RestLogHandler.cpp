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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "RestLogHandler.h"

#include <velocypack/Iterator.h>

#include <Cluster/ServerState.h>
#include <Network/ConnectionPool.h>
#include <Network/Methods.h>
#include <Network/NetworkFeature.h>

#include <Cluster/ClusterFeature.h>
#include <Cluster/AgencyCache.h>

#include "Agency/AgencyPaths.h"
#include "Inspection/VPack.h"
#include "Replication2/AgencyMethods.h"
#include "Replication2/Methods.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/ReplicatedLogIterator.h"
#include "Replication2/ReplicatedLog/Utilities.h"
#include "Rest/PathMatch.h"
#include "ApplicationFeatures/ApplicationServer.h"

using namespace arangodb;
using namespace arangodb::replication2;

RestStatus RestLogHandler::execute() {
  // for now required admin access to the database
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  auto methods = ReplicatedLogMethods::createInstance(_vocbase);
  return executeByMethod(*methods);
}

RestStatus RestLogHandler::executeByMethod(
    ReplicatedLogMethods const& methods) {
  switch (_request->requestType()) {
    case rest::RequestType::GET:
      return handleGetRequest(methods);
    case rest::RequestType::POST:
      return handlePostRequest(methods);
    case rest::RequestType::DELETE_REQ:
      return handleDeleteRequest(methods);
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
  return RestStatus::DONE;
}

RestStatus RestLogHandler::handlePostRequest(
    ReplicatedLogMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  if (suffixes.empty()) {
    return handlePost(methods, body);
  } else if (std::string_view logIdStr, toRemoveStr, toAddStr;
             rest::Match(suffixes).against(&logIdStr, "participant",
                                           &toRemoveStr, "replace-with",
                                           &toAddStr)) {
    auto const logId = replication2::LogId::fromString(logIdStr);
    auto const toRemove = replication2::ParticipantId(toRemoveStr);
    auto const toAdd = replication2::ParticipantId(toAddStr);
    if (!logId) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT("Not a log id: ", logIdStr));
      return RestStatus::DONE;
    }

    // If this wasn't a temporary API, it would be nice to be able to pass a
    // minimum raft index to wait for here.
    namespace paths = ::arangodb::cluster::paths;
    auto& agencyCache =
        _vocbase.server().getFeature<ClusterFeature>().agencyCache();
    if (auto result = agencyCache.waitForLatestCommitIndex().waitAndGet();
        result.fail()) {
      THROW_ARANGO_EXCEPTION(result);
    }
    auto path = paths::aliases::target()
                    ->replicatedLogs()
                    ->database(_vocbase.name())
                    ->log(*logId);
    auto&& [res, raftIdx] =
        agencyCache.get(path->str(paths::SkipComponents{1}));
    auto stateTarget =
        velocypack::deserialize<replication2::agency::LogTarget>(res->slice());

    return waitForFuture(
        methods.replaceParticipant(*logId, toRemove, toAdd, stateTarget.leader)
            .thenValue([this](auto&& result) {
              if (result.ok()) {
                generateOk(rest::ResponseCode::OK,
                           VPackSlice::emptyObjectSlice());
              } else {
                generateError(result);
              }
            }));
  } else if (std::string_view logIdStr, newLeaderStr;
             rest::Match(suffixes).against(&logIdStr, "leader",
                                           &newLeaderStr)) {
    auto const logId = replication2::LogId::fromString(logIdStr);
    auto const newLeader = replication2::ParticipantId(newLeaderStr);
    if (!logId) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT("Not a log id: ", logIdStr));
      return RestStatus::DONE;
    }
    return waitForFuture(
        methods.setLeader(*logId, newLeader).thenValue([this](auto&& result) {
          if (result.ok()) {
            generateOk(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
          } else {
            generateError(result);
          }
        }));
  }

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect POST /_api/log/<log-id>/[verb]");
    return RestStatus::DONE;
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  if (auto& verb = suffixes[1]; verb == "insert") {
    return handlePostInsert(methods, logId, body);
  } else if (verb == "release") {
    return handlePostRelease(methods, logId);
  } else if (verb == "ping") {
    return handlePostPing(methods, logId, body);
  } else if (verb == "compact") {
    return handlePostCompact(methods, logId);
  } else if (verb == "multi-insert") {
    return handlePostInsertMulti(methods, logId, body);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expecting one of the resources 'insert', 'release', "
                  "'multi-insert', 'compact', 'ping'");
  }
  return RestStatus::DONE;
}

RestStatus RestLogHandler::handlePostInsert(ReplicatedLogMethods const& methods,
                                            replication2::LogId logId,
                                            velocypack::Slice payload) {
  bool const waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool const dontWaitForCommit =
      _request->parsedValue(StaticStrings::DontWaitForCommit, false);
  if (dontWaitForCommit) {
    return waitForFuture(
        methods
            .insertWithoutCommit(logId, LogPayload::createFromSlice(payload),
                                 waitForSync)
            .thenValue([this](LogIndex idx) {
              VPackBuilder response;
              {
                VPackObjectBuilder result(&response);
                response.add("index", VPackValue(idx));
              }
              generateOk(rest::ResponseCode::ACCEPTED, response.slice());
            }));

  } else {
    return waitForFuture(
        methods.insert(logId, LogPayload::createFromSlice(payload), waitForSync)
            .thenValue([this](auto&& waitForResult) {
              VPackBuilder response;
              {
                VPackObjectBuilder result(&response);
                response.add("index", VPackValue(waitForResult.first));
                response.add(VPackValue("result"));
                waitForResult.second.toVelocyPack(response);
              }
              generateOk(rest::ResponseCode::CREATED, response.slice());
            }));
  }
}

RestStatus RestLogHandler::handlePostPing(ReplicatedLogMethods const& methods,
                                          replication2::LogId logId,
                                          velocypack::Slice payload) {
  std::optional<std::string> message;
  if (payload.isObject()) {
    if (auto messageSlice = payload.get("message"); not messageSlice.isNone()) {
      message = messageSlice.copyString();
    }
  }
  return waitForFuture(
      methods.ping(logId, std::move(message))
          .thenValue([this](auto&& waitForResult) {
            VPackBuilder response;
            {
              VPackObjectBuilder result(&response);
              response.add("index", VPackValue(waitForResult.first));
              response.add(VPackValue("result"));
              waitForResult.second.toVelocyPack(response);
            }
            generateOk(rest::ResponseCode::CREATED, response.slice());
          }));
}

RestStatus RestLogHandler::handlePostInsertMulti(
    ReplicatedLogMethods const& methods, replication2::LogId logId,
    velocypack::Slice payload) {
  if (!payload.isArray()) {
    generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "array expected at top-level");
    return RestStatus::DONE;
  }
  bool const waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool const dontWaitForCommit =
      _request->parsedValue(StaticStrings::DontWaitForCommit, false);
  if (dontWaitForCommit) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_NOT_IMPLEMENTED,
                  "dontWaitForCommit is not implemented for multiple inserts");
    return RestStatus::DONE;
  }
  auto iter = replicated_log::VPackArrayToLogPayloadIterator{payload};
  auto f = methods.insert(logId, iter, waitForSync)
               .thenValue([this](auto&& waitForResult) {
                 VPackBuilder response;
                 {
                   VPackObjectBuilder result(&response);
                   {
                     VPackArrayBuilder a(&response, "indexes");
                     for (auto index : waitForResult.first) {
                       response.add(VPackValue(index));
                     }
                   }
                   response.add(VPackValue("result"));
                   waitForResult.second.toVelocyPack(response);
                 }
                 generateOk(rest::ResponseCode::CREATED, response.slice());
               });
  return waitForFuture(std::move(f));
}

RestStatus RestLogHandler::handlePostRelease(
    ReplicatedLogMethods const& methods, replication2::LogId logId) {
  auto idx = LogIndex{basics::StringUtils::uint64(_request->value("index"))};
  return waitForFuture(
      methods.release(logId, idx).thenValue([this](Result&& res) {
        if (res.fail()) {
          generateError(res);
        } else {
          generateOk(rest::ResponseCode::ACCEPTED, VPackSlice::noneSlice());
        }
      }));
}

RestStatus RestLogHandler::handlePostCompact(
    ReplicatedLogMethods const& methods, replication2::LogId logId) {
  return waitForFuture(methods.compact(logId).thenValue([this](auto&& res) {
    VPackBuilder builder;
    velocypack::serialize(builder, res);
    generateOk(rest::ResponseCode::ACCEPTED, builder.slice());
  }));
}

RestStatus RestLogHandler::handlePost(ReplicatedLogMethods const& methods,
                                      velocypack::Slice specSlice) {
  if (_vocbase.replicationVersion() != replication::Version::TWO) {
    generateError(
        Result{TRI_ERROR_HTTP_FORBIDDEN,
               "Replicated logs available only in replication2 databases!"});
    return RestStatus::DONE;
  }

  // create a new log
  auto spec =
      velocypack::deserialize<ReplicatedLogMethods::CreateOptions>(specSlice);
  return waitForFuture(
      methods.createReplicatedLog(std::move(spec))
          .thenValue(
              [this](ResultT<ReplicatedLogMethods::CreateResult>&& result) {
                if (result.ok()) {
                  VPackBuilder builder;
                  velocypack::serialize(builder, result.get());
                  generateOk(rest::ResponseCode::OK, builder.slice());
                } else {
                  generateError(result.result());
                }
              }));
}

RestStatus RestLogHandler::handleGetRequest(
    ReplicatedLogMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.empty()) {
    return handleGet(methods);
  }

  if (suffixes.size() == 2) {
    if (suffixes[0] == "collection-status") {
      CollectionID cid{suffixes[1]};
      return handleGetCollectionStatus(methods, std::move(cid));
    }
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};

  if (suffixes.size() == 1) {
    return handleGetLog(methods, logId);
  }

  auto const& verb = suffixes[1];
  if (verb == "poll") {
    return handleGetPoll(methods, logId);
  } else if (verb == "head") {
    return handleGetHead(methods, logId);
  } else if (verb == "tail") {
    return handleGetTail(methods, logId);
  } else if (verb == "entry") {
    return handleGetEntry(methods, logId);
  } else if (verb == "slice") {
    return handleGetSlice(methods, logId);
  } else if (verb == "local-status") {
    return handleGetLocalStatus(methods, logId);
  } else if (verb == "global-status") {
    return handleGetGlobalStatus(methods, logId);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expecting one of the resources 'poll', 'head', 'tail', "
                  "'entry', 'slice', 'local-status', 'global-status'");
  }
  return RestStatus::DONE;
}

RestStatus RestLogHandler::handleDeleteRequest(
    ReplicatedLogMethods const& methods) {
  auto const& suffixes = _request->suffixes();
  if (std::string_view logIdStr; rest::Match(suffixes).against(&logIdStr)) {
    auto const logId = replication2::LogId::fromString(logIdStr);
    if (!logId) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT("Not a log id: ", logIdStr));
      return RestStatus::DONE;
    }
    return waitForFuture(
        methods.deleteReplicatedLog(*logId).thenValue([this](auto&& result) {
          if (result.ok()) {
            generateOk(rest::ResponseCode::OK, VPackSlice::noneSlice());
          } else {
            generateError(result);
          }
        }));

  } else if (std::string_view logIdStr;
             rest::Match(suffixes).against(&logIdStr, "leader")) {
    auto const logId = replication2::LogId::fromString(logIdStr);
    if (!logId) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT("Not a log id: ", logIdStr));
      return RestStatus::DONE;
    }
    return waitForFuture(methods.setLeader(*logId, std::nullopt)
                             .thenValue([this](auto&& result) {
                               if (result.ok()) {
                                 generateOk(rest::ResponseCode::OK,
                                            VPackSlice::emptyObjectSlice());
                               } else {
                                 generateError(result);
                               }
                             }));
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return RestStatus::DONE;
  }
}

RestLogHandler::RestLogHandler(ArangodServer& server, GeneralRequest* req,
                               GeneralResponse* resp)
    : RestVocbaseBaseHandler(server, req, resp) {}

RestStatus RestLogHandler::handleGet(ReplicatedLogMethods const& methods) {
  return waitForFuture(methods.getReplicatedLogs().thenValue([this](
                                                                 auto&& logs) {
    VPackBuilder builder;
    {
      VPackObjectBuilder ob(&builder);

      for (auto const& [idx, logInfo] : logs) {
        builder.add(VPackValue(std::to_string(idx.id())));
        std::visit(overload{[&](replicated_log::LogStatus const& status) {
                              status.toVelocyPack(builder);
                            },
                            [&](ReplicatedLogMethods::ParticipantsList const&
                                    participants) {
                              VPackArrayBuilder ab(&builder);
                              for (auto&& pid : participants) {
                                builder.add(VPackValue(pid));
                              }
                            }},
                   logInfo);
      }
    }

    generateOk(rest::ResponseCode::OK, builder.slice());
  }));
}

RestStatus RestLogHandler::handleGetLog(const ReplicatedLogMethods& methods,
                                        replication2::LogId logId) {
  return waitForFuture(
      methods.getStatus(logId).thenValue([this](auto&& status) {
        std::visit(
            [this](auto&& status) {
              VPackBuilder buffer;
              status.toVelocyPack(buffer);
              generateOk(rest::ResponseCode::OK, buffer.slice());
            },
            status);
      }));
}

RestStatus RestLogHandler::handleGetPoll(ReplicatedLogMethods const& methods,
                                         replication2::LogId logId) {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/poll");
    return RestStatus::DONE;
  }
  bool limitFound;
  LogIndex logIdx{basics::StringUtils::uint64(_request->value("first"))};
  std::size_t limit{
      basics::StringUtils::uint64(_request->value("limit", limitFound))};
  limit = limitFound ? limit : ReplicatedLogMethods::kDefaultLimit;

  auto fut = methods.poll(logId, logIdx, limit)
                 .thenValue([&](std::unique_ptr<LogIterator> iter) {
                   VPackBuilder builder;
                   {
                     VPackArrayBuilder ab(&builder);
                     while (auto entry = iter->next()) {
                       entry->toVelocyPack(builder);
                     }
                   }

                   generateOk(rest::ResponseCode::OK, builder.slice());
                 });

  return waitForFuture(std::move(fut));
}

RestStatus RestLogHandler::handleGetTail(ReplicatedLogMethods const& methods,
                                         replication2::LogId logId) {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/tail");
    return RestStatus::DONE;
  }
  bool limitFound;
  std::size_t limit{
      basics::StringUtils::uint64(_request->value("limit", limitFound))};
  limit = limitFound ? limit : ReplicatedLogMethods::kDefaultLimit;

  auto fut = methods.tail(logId, limit)
                 .thenValue([&](std::unique_ptr<LogIterator> iter) {
                   VPackBuilder builder;
                   {
                     VPackArrayBuilder ab(&builder);
                     while (auto entry = iter->next()) {
                       entry->toVelocyPack(builder);
                     }
                   }

                   generateOk(rest::ResponseCode::OK, builder.slice());
                 });

  return waitForFuture(std::move(fut));
}

RestStatus RestLogHandler::handleGetHead(ReplicatedLogMethods const& methods,
                                         replication2::LogId logId) {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/head");
    return RestStatus::DONE;
  }
  bool limitFound;
  std::size_t limit{
      basics::StringUtils::uint64(_request->value("limit", limitFound))};
  limit = limitFound ? limit : ReplicatedLogMethods::kDefaultLimit;

  auto fut = methods.head(logId, limit)
                 .thenValue([&](std::unique_ptr<LogIterator> iter) {
                   VPackBuilder builder;
                   {
                     VPackArrayBuilder ab(&builder);
                     while (auto entry = iter->next()) {
                       entry->toVelocyPack(builder);
                     }
                   }

                   generateOk(rest::ResponseCode::OK, builder.slice());
                 });

  return waitForFuture(std::move(fut));
}

RestStatus RestLogHandler::handleGetSlice(ReplicatedLogMethods const& methods,
                                          replication2::LogId logId) {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/slice");
    return RestStatus::DONE;
  }
  bool stopFound;
  LogIndex start{basics::StringUtils::uint64(_request->value("start"))};
  LogIndex stop{
      basics::StringUtils::uint64(_request->value("stop", stopFound))};
  stop = stopFound ? stop : start + ReplicatedLogMethods::kDefaultLimit + 1;

  auto fut = methods.slice(logId, start, stop)
                 .thenValue([&](std::unique_ptr<LogIterator> iter) {
                   VPackBuilder builder;
                   {
                     VPackArrayBuilder ab(&builder);
                     while (auto entry = iter->next()) {
                       entry->toVelocyPack(builder);
                     }
                   }

                   generateOk(rest::ResponseCode::OK, builder.slice());
                 });

  return waitForFuture(std::move(fut));
}

RestStatus RestLogHandler::handleGetLocalStatus(
    ReplicatedLogMethods const& methods, replication2::LogId logId) {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/local-status");
    return RestStatus::DONE;
  }

  return waitForFuture(
      methods.getLocalStatus(logId).thenValue([this](auto&& status) {
        VPackBuilder buffer;
        status.toVelocyPack(buffer);
        generateOk(rest::ResponseCode::OK, buffer.slice());
      }));
}

RestStatus RestLogHandler::handleGetGlobalStatus(
    ReplicatedLogMethods const& methods, replication2::LogId logId) {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/global-status");
    return RestStatus::DONE;
  }

  auto specSource = std::invoke([&] {
    auto isLocal = _request->parsedValue<bool>("useLocalCache").value_or(false);
    if (isLocal) {
      return replicated_log::GlobalStatus::SpecificationSource::kLocalCache;
    }
    return replicated_log::GlobalStatus::SpecificationSource::kRemoteAgency;
  });

  return waitForFuture(methods.getGlobalStatus(logId, specSource)
                           .thenValue([this](auto&& status) {
                             VPackBuilder buffer;
                             status.toVelocyPack(buffer);
                             generateOk(rest::ResponseCode::OK, buffer.slice());
                           }));
}

RestStatus RestLogHandler::handleGetCollectionStatus(
    replication2::ReplicatedLogMethods const& methods, CollectionID cid) {
  return waitForFuture(methods.getCollectionStatus(std::move(cid))
                           .thenValue([this](auto&& status) {
                             VPackBuilder buffer;
                             status.toVelocyPack(buffer);
                             generateOk(rest::ResponseCode::OK, buffer.slice());
                           }));
}

RestStatus RestLogHandler::handleGetEntry(ReplicatedLogMethods const& methods,
                                          replication2::LogId logId) {
  auto const& suffixes = _request->suffixes();
  if (suffixes.size() != 3) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/entry/<id>");
    return RestStatus::DONE;
  }
  LogIndex logIdx{basics::StringUtils::uint64(suffixes[2])};

  auto fut =
      methods.slice(logId, logIdx, logIdx + 1)
          .thenValue([this](std::unique_ptr<LogIterator>&& iter) {
            if (auto entry = iter->next(); entry) {
              VPackBuilder result;
              entry->toVelocyPack(result);
              generateOk(rest::ResponseCode::OK, result.slice());
            } else {
              generateError(rest::ResponseCode::NOT_FOUND,
                            TRI_ERROR_HTTP_NOT_FOUND, "log index not found");
            }
          });

  return waitForFuture(std::move(fut));
}
