////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "RestLogHandler.h"

#include <velocypack/Iterator.h>

#include <Cluster/ServerState.h>
#include <Network/ConnectionPool.h>
#include <Network/Methods.h>
#include <Network/NetworkFeature.h>

#include <Cluster/ClusterFeature.h>

#include "Replication2/AgencyMethods.h"
#include "Replication2/Methods.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogEntries.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/ReplicatedLogIterator.h"
#include "Replication2/ReplicatedLog/Utilities.h"

using namespace arangodb;
using namespace arangodb::replication2;

RestStatus RestLogHandler::execute() {
  // for now required admin access to the database
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  auto const& suffixes = _request->decodedSuffixes();
  if (suffixes.size() >= 2) {
    if (suffixes[1] == "target") {
      return handleTarget();
    }
  }

  // execute local methods
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
  std::vector<std::string> const& suffixes = _request->suffixes();

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  if (suffixes.empty()) {
    return handlePost(methods, body);
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
  } else if (verb == "multi-insert") {
    return handlePostInsertMulti(methods, logId, body);
  } else {
    generateError(
        rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
        "expecting one of the resources 'insert', 'release', 'multi-insert'");
  }
  return RestStatus::DONE;
}

RestStatus RestLogHandler::handlePostInsert(ReplicatedLogMethods const& methods,
                                            replication2::LogId logId,
                                            velocypack::Slice payload) {
  return waitForFuture(
      methods.insert(logId, LogPayload::createFromSlice(payload))
          .thenValue([this](auto&& waitForResult) {
            VPackBuilder response;
            {
              VPackObjectBuilder result(&response);
              response.add("index", VPackValue(waitForResult.first));
              response.add(VPackValue("result"));
              waitForResult.second.toVelocyPack(response);
            }
            generateOk(rest::ResponseCode::ACCEPTED, response.slice());
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

  auto iter = replicated_log::VPackArrayToLogPayloadIterator{payload};
  auto f = methods.insert(logId, iter).thenValue([this](auto&& waitForResult) {
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
    generateOk(rest::ResponseCode::ACCEPTED, response.slice());
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

RestStatus RestLogHandler::handlePost(ReplicatedLogMethods const& methods,
                                      velocypack::Slice specSlice) {
  // create a new log
  replication2::agency::LogTarget spec(replication2::agency::from_velocypack,
                                       specSlice);
  return waitForFuture(
      methods.createReplicatedLog(spec).thenValue([this](Result&& result) {
        if (result.ok()) {
          generateOk(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
        } else {
          generateError(result);
        }
      }));
}

RestStatus RestLogHandler::handleGetRequest(
    ReplicatedLogMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.empty()) {
    return handleGet(methods);
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
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect DELETE /_api/log/<log-id>");
    return RestStatus::DONE;
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  return waitForFuture(
      methods.deleteReplicatedLog(logId).thenValue([this](Result&& result) {
        if (!result.ok()) {
          generateError(result);
        } else {
          generateOk(rest::ResponseCode::ACCEPTED,
                     VPackSlice::emptyObjectSlice());
        }
      }));
}

RestLogHandler::RestLogHandler(ArangodServer& server, GeneralRequest* req,
                               GeneralResponse* resp)
    : RestVocbaseBaseHandler(server, req, resp) {}

RestStatus RestLogHandler::handleGet(ReplicatedLogMethods const& methods) {
  return waitForFuture(
      methods.getReplicatedLogs().thenValue([this](auto&& logs) {
        VPackBuilder builder;
        {
          VPackObjectBuilder ob(&builder);

          for (auto const& [idx, status] : logs) {
            builder.add(VPackValue(std::to_string(idx.id())));
            status.toVelocyPack(builder);
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
                 .thenValue([&](std::unique_ptr<PersistedLogIterator> iter) {
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
                 .thenValue([&](std::unique_ptr<PersistedLogIterator> iter) {
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
                 .thenValue([&](std::unique_ptr<PersistedLogIterator> iter) {
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
                 .thenValue([&](std::unique_ptr<PersistedLogIterator> iter) {
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

  return waitForFuture(
      methods.getGlobalStatus(logId).thenValue([this](auto&& status) {
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

  return waitForFuture(
      methods.getLogEntryByIndex(logId, logIdx)
          .thenValue([this](std::optional<PersistingLogEntry>&& entry) {
            if (entry) {
              VPackBuilder result;
              entry->toVelocyPack(result);
              generateOk(rest::ResponseCode::OK, result.slice());
            } else {
              generateError(rest::ResponseCode::NOT_FOUND,
                            TRI_ERROR_HTTP_NOT_FOUND, "log index not found");
            }
          }));
}

RestStatus RestLogHandler::handleTarget() {
  auto const& suffixes = _request->suffixes();
  TRI_ASSERT(suffixes.size() >= 2);
  TRI_ASSERT(suffixes[1] == "target");

  auto methods =
      replication2::agency::methods::TargetMethods::construct(_vocbase);

  LogId logId{basics::StringUtils::uint64(suffixes[0])};

  auto const generateLogTargetResponse = [this](rest::ResponseCode code) {
    return
        [this, code](ResultT<replication2::agency::LogTarget> const& result) {
          if (result.ok()) {
            velocypack::Builder builder;
            result->toVelocyPack(builder);
            generateOk(code, builder.slice());
          } else {
            generateError(result.result());
          }
        };
  };

  if (suffixes.size() == 2) {
    // simple target API
    // PUT - creates the replicated log in target
    // PATCH - does a velocypack merge with the given body and the old target
    //  null means remove
    // GET - load the current target
    // DELETE - delete target entry (effectively deleting the replicated log)
    if (_request->requestType() == rest::RequestType::GET) {
      auto f = methods->getTarget(logId).thenValue(
          generateLogTargetResponse(rest::ResponseCode::OK));

      return waitForFuture(std::move(f));
    }

    bool parseSuccess = false;
    VPackSlice body = this->parseVPackBody(parseSuccess);
    if (!parseSuccess) {  // error message generated in parseVPackBody
      return RestStatus::DONE;
    }

    switch (_request->requestType()) {
      case RequestType::DELETE_REQ:
        return waitForFuture(methods->deleteTarget(logId).thenValue(
            [this](Result const& result) {
              if (result.ok()) {
                generateOk(rest::ResponseCode::OK,
                           velocypack::Slice::noneSlice());
              } else {
                generateError(result);
              }
            }));
      case RequestType::POST:
      case RequestType::PUT:
        return waitForFuture(
            methods
                ->putTarget(
                    logId,
                    replication2::agency::LogTarget::fromVelocyPack(body))
                .thenValue(
                    generateLogTargetResponse(rest::ResponseCode::ACCEPTED)));
      case RequestType::PATCH:
        return waitForFuture(methods->patchTarget(logId, body)
                                 .thenValue(generateLogTargetResponse(
                                     rest::ResponseCode::ACCEPTED)));
      case RequestType::GET:  // handled above
        TRI_ASSERT(false);
        [[fallthrough]];
      default:
        generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                      TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    }

  } else {
    bool parseSuccess = false;
    VPackSlice body = this->parseVPackBody(parseSuccess);
    if (!parseSuccess) {  // error message generated in parseVPackBody
      return RestStatus::DONE;
    }

    if (auto const& verb = suffixes[2]; verb == "leader") {
      // First handle the get request. Simply return the leader or null if
      // no leader is set.
      if (_request->requestType() == rest::RequestType::GET) {
        auto f = methods->getLeader(logId).thenValue([this](auto&& result) {
          if (result.fail()) {
            generateError(result.result());
          } else {
            velocypack::Builder builder;
            auto const& leader = result.get();
            if (leader.has_value()) {
              builder.add(velocypack::Value(*leader));
            } else {
              builder.add(velocypack::Slice::nullSlice());
            }
            generateOk(rest::ResponseCode::OK, builder.slice());
          }
        });
        return waitForFuture(std::move(f));
      }

      if (_request->requestType() != rest::RequestType::PUT) {
        generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                      TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
        return RestStatus::DONE;
      }

      auto newLeader =
          std::invoke([&]() -> ResultT<std::optional<ParticipantId>> {
            if (body.isString()) {
              return {body.copyString()};
            } else if (body.isNull()) {
              return {std::nullopt};
            }
            return Result{
                TRI_ERROR_BAD_PARAMETER,
                "invalid new leader: expected string or null as body"};
          });
      if (newLeader.fail()) {
        generateError(newLeader.result());
        return RestStatus::DONE;
      } else {
        return waitForFuture(methods->updateLeader(logId, *newLeader)
                                 .thenValue(generateLogTargetResponse(
                                     rest::ResponseCode::ACCEPTED)));
      }

    } else if (verb == "participants") {
      // participants api
    }
  }

  return RestStatus::DONE;
}
