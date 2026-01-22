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

#include <Async/async.h>
#include <Cluster/AgencyCache.h>
#include <Cluster/ClusterFeature.h>
#include <Network/Methods.h>
#include <Network/NetworkFeature.h>

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

auto RestLogHandler::executeAsync() -> futures::Future<futures::Unit> {
  // for now required admin access to the database
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    co_return;
  }

  auto methods = ReplicatedLogMethods::createInstance(_vocbase);
  co_await executeByMethod(*methods);
  co_return;
}

auto RestLogHandler::executeByMethod(ReplicatedLogMethods const& methods)
    -> async<void> {
  switch (_request->requestType()) {
    case rest::RequestType::GET:
      co_return co_await handleGetRequest(methods);
    case rest::RequestType::POST:
      co_return co_await handlePostRequest(methods);
    case rest::RequestType::DELETE_REQ:
      co_return co_await handleDeleteRequest(methods);
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
  co_return;
}

auto RestLogHandler::handlePostRequest(ReplicatedLogMethods const& methods)
    -> async<void> {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    co_return;
  }

  if (suffixes.empty()) {
    co_await handlePost(methods, body);
    co_return;
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
      co_return;
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

    auto result = co_await methods.replaceParticipant(*logId, toRemove, toAdd,
                                                      stateTarget.leader);
    if (result.ok()) {
      generateOk(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
    } else {
      generateError(result);
    }
    co_return;
  } else if (std::string_view logIdStr, newLeaderStr;
             rest::Match(suffixes).against(&logIdStr, "leader",
                                           &newLeaderStr)) {
    auto const logId = replication2::LogId::fromString(logIdStr);
    auto const newLeader = replication2::ParticipantId(newLeaderStr);
    if (!logId) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT("Not a log id: ", logIdStr));
      co_return;
    }
    auto result = co_await methods.setLeader(*logId, newLeader);

    if (result.ok()) {
      generateOk(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
    } else {
      generateError(result);
    }
    co_return;
  }

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect POST /_api/log/<log-id>/[verb]");
    co_return;
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  if (auto& verb = suffixes[1]; verb == "insert") {
    co_return co_await handlePostInsert(methods, logId, body);
  } else if (verb == "release") {
    co_return co_await handlePostRelease(methods, logId);
  } else if (verb == "ping") {
    co_return co_await handlePostPing(methods, logId, body);
  } else if (verb == "compact") {
    co_return co_await handlePostCompact(methods, logId);
  } else if (verb == "multi-insert") {
    co_return co_await handlePostInsertMulti(methods, logId, body);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expecting one of the resources 'insert', 'release', "
                  "'multi-insert', 'compact', 'ping'");
  }
  co_return;
}

auto RestLogHandler::handlePostInsert(ReplicatedLogMethods const& methods,
                                      replication2::LogId logId,
                                      velocypack::Slice payload)
    -> async<void> {
  bool const waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool const dontWaitForCommit =
      _request->parsedValue(StaticStrings::DontWaitForCommit, false);
  if (dontWaitForCommit) {
    auto idx = co_await methods.insertWithoutCommit(
        logId, LogPayload::createFromSlice(payload), waitForSync);

    VPackBuilder response;
    {
      VPackObjectBuilder result(&response);
      response.add("index", VPackValue(idx));
    }
    generateOk(rest::ResponseCode::ACCEPTED, response.slice());
  } else {
    auto waitForResult = co_await methods.insert(
        logId, LogPayload::createFromSlice(payload), waitForSync);
    VPackBuilder response;
    {
      VPackObjectBuilder result(&response);
      response.add("index", VPackValue(waitForResult.first));
      response.add(VPackValue("result"));
      waitForResult.second.toVelocyPack(response);
    }
    generateOk(rest::ResponseCode::CREATED, response.slice());
  }
}

auto RestLogHandler::handlePostPing(ReplicatedLogMethods const& methods,
                                    replication2::LogId logId,
                                    velocypack::Slice payload) -> async<void> {
  std::optional<std::string> message;
  if (payload.isObject()) {
    if (auto messageSlice = payload.get("message"); not messageSlice.isNone()) {
      message = messageSlice.copyString();
    }
  }
  auto waitForResult = co_await methods.ping(logId, std::move(message));

  VPackBuilder response;
  {
    VPackObjectBuilder result(&response);
    response.add("index", VPackValue(waitForResult.first));
    response.add(VPackValue("result"));
    waitForResult.second.toVelocyPack(response);
  }
  generateOk(rest::ResponseCode::CREATED, response.slice());
}

auto RestLogHandler::handlePostInsertMulti(ReplicatedLogMethods const& methods,
                                           replication2::LogId logId,
                                           velocypack::Slice payload)
    -> async<void> {
  if (!payload.isArray()) {
    generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "array expected at top-level");
    co_return;
  }
  bool const waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool const dontWaitForCommit =
      _request->parsedValue(StaticStrings::DontWaitForCommit, false);
  if (dontWaitForCommit) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_NOT_IMPLEMENTED,
                  "dontWaitForCommit is not implemented for multiple inserts");
    co_return;
  }
  auto iter = replicated_log::VPackArrayToLogPayloadIterator{payload};
  auto waitForResult = co_await methods.insert(logId, iter, waitForSync);

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
}

auto RestLogHandler::handlePostRelease(ReplicatedLogMethods const& methods,
                                       replication2::LogId logId)
    -> async<void> {
  auto idx = LogIndex{basics::StringUtils::uint64(_request->value("index"))};

  auto res = co_await methods.release(logId, idx);
  if (res.fail()) {
    generateError(res);
  } else {
    generateOk(rest::ResponseCode::ACCEPTED, VPackSlice::noneSlice());
  }
}

auto RestLogHandler::handlePostCompact(ReplicatedLogMethods const& methods,
                                       replication2::LogId logId)
    -> async<void> {
  auto res = co_await methods.compact(logId);

  VPackBuilder builder;
  velocypack::serialize(builder, res);
  generateOk(rest::ResponseCode::ACCEPTED, builder.slice());
}

auto RestLogHandler::handlePost(ReplicatedLogMethods const& methods,
                                velocypack::Slice specSlice) -> async<void> {
  if (_vocbase.replicationVersion() != replication::Version::TWO) {
    generateError(
        Result{TRI_ERROR_HTTP_FORBIDDEN,
               "Replicated logs available only in replication2 databases!"});
    co_return;
  }

  // create a new log
  auto spec =
      velocypack::deserialize<ReplicatedLogMethods::CreateOptions>(specSlice);
  auto result = co_await methods.createReplicatedLog(std::move(spec));
  if (result.ok()) {
    VPackBuilder builder;
    velocypack::serialize(builder, result.get());
    generateOk(rest::ResponseCode::OK, builder.slice());
  } else {
    generateError(result.result());
  }
}

auto RestLogHandler::handleGetRequest(ReplicatedLogMethods const& methods)
    -> async<void> {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.empty()) {
    co_return co_await handleGet(methods);
  }

  if (suffixes.size() == 2) {
    if (suffixes[0] == "collection-status") {
      CollectionID cid{suffixes[1]};
      co_return co_await handleGetCollectionStatus(methods, std::move(cid));
    }
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};

  if (suffixes.size() == 1) {
    co_return co_await handleGetLog(methods, logId);
  }

  auto const& verb = suffixes[1];
  if (verb == "poll") {
    co_return co_await handleGetPoll(methods, logId);
  } else if (verb == "head") {
    co_return co_await handleGetHead(methods, logId);
  } else if (verb == "tail") {
    co_return co_await handleGetTail(methods, logId);
  } else if (verb == "entry") {
    co_return co_await handleGetEntry(methods, logId);
  } else if (verb == "slice") {
    co_return co_await handleGetSlice(methods, logId);
  } else if (verb == "local-status") {
    co_return co_await handleGetLocalStatus(methods, logId);
  } else if (verb == "global-status") {
    co_return co_await handleGetGlobalStatus(methods, logId);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expecting one of the resources 'poll', 'head', 'tail', "
                  "'entry', 'slice', 'local-status', 'global-status'");
  }
}

auto RestLogHandler::handleDeleteRequest(ReplicatedLogMethods const& methods)
    -> async<void> {
  auto const& suffixes = _request->suffixes();
  if (std::string_view logIdStr; rest::Match(suffixes).against(&logIdStr)) {
    auto const logId = replication2::LogId::fromString(logIdStr);
    if (!logId) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT("Not a log id: ", logIdStr));
      co_return;
    }
    auto result = co_await methods.deleteReplicatedLog(*logId);
    if (result.ok()) {
      generateOk(rest::ResponseCode::OK, VPackSlice::noneSlice());
    } else {
      generateError(result);
    }
  } else if (std::string_view logIdStr;
             rest::Match(suffixes).against(&logIdStr, "leader")) {
    auto const logId = replication2::LogId::fromString(logIdStr);
    if (!logId) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT("Not a log id: ", logIdStr));
      co_return;
    }
    auto result = co_await methods.setLeader(*logId, std::nullopt);
    if (result.ok()) {
      generateOk(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
    } else {
      generateError(result);
    }
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
  }
}

RestLogHandler::RestLogHandler(ArangodServer& server, GeneralRequest* req,
                               GeneralResponse* resp)
    : RestVocbaseBaseHandler(server, req, resp) {}

auto RestLogHandler::handleGet(ReplicatedLogMethods const& methods)
    -> async<void> {
  auto logs = co_await methods.getReplicatedLogs();
  VPackBuilder builder;
  {
    VPackObjectBuilder ob(&builder);

    for (auto const& [idx, logInfo] : logs) {
      builder.add(VPackValue(std::to_string(idx.id())));
      std::visit(
          overload{
              [&](replicated_log::LogStatus const& status) {
                status.toVelocyPack(builder);
              },
              [&](ReplicatedLogMethods::ParticipantsList const& participants) {
                VPackArrayBuilder ab(&builder);
                for (auto&& pid : participants) {
                  builder.add(VPackValue(pid));
                }
              }},
          logInfo);
    }
  }

  generateOk(rest::ResponseCode::OK, builder.slice());
}

auto RestLogHandler::handleGetLog(ReplicatedLogMethods const& methods,
                                  replication2::LogId logId) -> async<void> {
  auto status = co_await methods.getStatus(logId);
  std::visit(
      [this](auto&& status) {
        VPackBuilder buffer;
        status.toVelocyPack(buffer);
        generateOk(rest::ResponseCode::OK, buffer.slice());
      },
      status);
}

auto RestLogHandler::handleGetPoll(ReplicatedLogMethods const& methods,
                                   replication2::LogId logId) -> async<void> {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/poll");
    co_return;
  }
  bool limitFound;
  LogIndex logIdx{basics::StringUtils::uint64(_request->value("first"))};
  std::size_t limit{
      basics::StringUtils::uint64(_request->value("limit", limitFound))};
  limit = limitFound ? limit : ReplicatedLogMethods::kDefaultLimit;

  auto iter = co_await methods.poll(logId, logIdx, limit);
  VPackBuilder builder;
  {
    VPackArrayBuilder ab(&builder);
    while (auto entry = iter->next()) {
      entry->toVelocyPack(builder);
    }
  }

  generateOk(rest::ResponseCode::OK, builder.slice());
}

auto RestLogHandler::handleGetTail(ReplicatedLogMethods const& methods,
                                   replication2::LogId logId) -> async<void> {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/tail");
    co_return;
  }
  bool limitFound;
  std::size_t limit{
      basics::StringUtils::uint64(_request->value("limit", limitFound))};
  limit = limitFound ? limit : ReplicatedLogMethods::kDefaultLimit;

  auto iter = co_await methods.tail(logId, limit);

  VPackBuilder builder;
  {
    VPackArrayBuilder ab(&builder);
    while (auto entry = iter->next()) {
      entry->toVelocyPack(builder);
    }
  }

  generateOk(rest::ResponseCode::OK, builder.slice());
}

auto RestLogHandler::handleGetHead(ReplicatedLogMethods const& methods,
                                   replication2::LogId logId) -> async<void> {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/head");
    co_return;
  }
  bool limitFound;
  std::size_t limit{
      basics::StringUtils::uint64(_request->value("limit", limitFound))};
  limit = limitFound ? limit : ReplicatedLogMethods::kDefaultLimit;

  auto iter = co_await methods.head(logId, limit);
  VPackBuilder builder;
  {
    VPackArrayBuilder ab(&builder);
    while (auto entry = iter->next()) {
      entry->toVelocyPack(builder);
    }
  }

  generateOk(rest::ResponseCode::OK, builder.slice());
}

auto RestLogHandler::handleGetSlice(ReplicatedLogMethods const& methods,
                                    replication2::LogId logId) -> async<void> {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/slice");
    co_return;
  }
  bool stopFound;
  LogIndex start{basics::StringUtils::uint64(_request->value("start"))};
  LogIndex stop{
      basics::StringUtils::uint64(_request->value("stop", stopFound))};
  stop = stopFound ? stop : start + ReplicatedLogMethods::kDefaultLimit + 1;

  auto iter = co_await methods.slice(logId, start, stop);

  VPackBuilder builder;
  {
    VPackArrayBuilder ab(&builder);
    while (auto entry = iter->next()) {
      entry->toVelocyPack(builder);
    }
  }

  generateOk(rest::ResponseCode::OK, builder.slice());
}

auto RestLogHandler::handleGetLocalStatus(ReplicatedLogMethods const& methods,
                                          replication2::LogId logId)
    -> async<void> {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/local-status");
    co_return;
  }

  auto status = co_await methods.getLocalStatus(logId);

  VPackBuilder buffer;
  status.toVelocyPack(buffer);
  generateOk(rest::ResponseCode::OK, buffer.slice());
}

auto RestLogHandler::handleGetGlobalStatus(ReplicatedLogMethods const& methods,
                                           replication2::LogId logId)
    -> async<void> {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/global-status");
    co_return;
  }

  auto specSource = std::invoke([&] {
    auto isLocal = _request->parsedValue<bool>("useLocalCache").value_or(false);
    if (isLocal) {
      return replicated_log::GlobalStatus::SpecificationSource::kLocalCache;
    }
    return replicated_log::GlobalStatus::SpecificationSource::kRemoteAgency;
  });
  auto status = co_await methods.getGlobalStatus(logId, specSource);

  VPackBuilder buffer;
  status.toVelocyPack(buffer);
  generateOk(rest::ResponseCode::OK, buffer.slice());
}

auto RestLogHandler::handleGetCollectionStatus(
    replication2::ReplicatedLogMethods const& methods, CollectionID cid)
    -> async<void> {
  auto status = co_await methods.getCollectionStatus(std::move(cid));
  VPackBuilder buffer;
  status.toVelocyPack(buffer);
  generateOk(rest::ResponseCode::OK, buffer.slice());
}

auto RestLogHandler::handleGetEntry(ReplicatedLogMethods const& methods,
                                    replication2::LogId logId) -> async<void> {
  auto const& suffixes = _request->suffixes();
  if (suffixes.size() != 3) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/entry/<id>");
    co_return;
  }
  LogIndex logIdx{basics::StringUtils::uint64(suffixes[2])};

  auto iter = co_await methods.slice(logId, logIdx, logIdx + 1);
  if (auto entry = iter->next(); entry) {
    VPackBuilder result;
    entry->toVelocyPack(result);
    generateOk(rest::ResponseCode::OK, result.slice());
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "log index not found");
  }
}
