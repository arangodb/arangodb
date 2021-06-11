////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
#include <velocypack/velocypack-aliases.h>

#include <Cluster/ServerState.h>
#include <Network/ConnectionPool.h>
#include <Network/Methods.h>
#include <Network/NetworkFeature.h>

#include <Cluster/ClusterFeature.h>

#include "Replication2/AgencyMethods.h"

#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/NetworkAttachedFollower.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/types.h"

using namespace arangodb;
using namespace arangodb::replication2;
namespace paths = arangodb::cluster::paths::aliases;

struct arangodb::ReplicatedLogMethods {
  virtual ~ReplicatedLogMethods() = default;
  virtual auto createReplicatedLog(replication2::agency::LogPlanSpecification const& spec) const
      -> futures::Future<Result> {
    return Result(TRI_ERROR_BAD_PARAMETER);
  }

  virtual auto deleteReplicatedLog(LogId id) const -> futures::Future<Result> {
    return Result(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto getReplicatedLogs() const
  -> futures::Future<std::unordered_map<arangodb::replication2::LogId,
      arangodb::replication2::replicated_log::LogStatus>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto getLogStatus(LogId) const
      -> futures::Future<replication2::replicated_log::LogStatus> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto getLogEntryByIndex(LogId, LogIndex) const -> futures::Future<std::optional<LogEntry>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto appendEntries(LogId, replicated_log::AppendEntriesRequest const&) const
      -> futures::Future<replicated_log::AppendEntriesResult> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto tailEntries(LogId, LogIndex) const
  -> futures::Future<std::unique_ptr<LogIterator>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto insert(LogId, LogPayload) const
  -> futures::Future<std::shared_ptr<replicated_log::QuorumData const>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto updateTermSpecification(LogId, replication2::agency::LogPlanTermSpecification const& term) const
      -> futures::Future<Result> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
};

namespace {

auto sendInsertRequest(network::ConnectionPool *pool, std::string const& server, std::string const& database,
                       LogId id, LogPayload payload)
    -> futures::Future<std::shared_ptr<replicated_log::QuorumData const>> {

  auto path = basics::StringUtils::joinT("/", "_api/log", id, "insert");

  network::RequestOptions opts;
  opts.database = database;
  return network::sendRequest(pool, "server:" + server, fuerte::RestVerb::Post, path,
                              payload.dummy, opts)
      .thenValue([](network::Response&& resp) {
        if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
          THROW_ARANGO_EXCEPTION(resp.combinedResult());
        }
        return std::make_shared<replication2::replicated_log::QuorumData const>(resp.slice().get("result"));
      });
}


auto sendLogStatusRequest(network::ConnectionPool *pool, std::string const& server, std::string const& database,
                       LogId id)
-> futures::Future<replication2::replicated_log::LogStatus> {

  auto path = basics::StringUtils::joinT("/", "_api/log", id);

  network::RequestOptions opts;
  opts.database = database;
  return network::sendRequest(pool, "server:" + server, fuerte::RestVerb::Get, path)
      .thenValue([](network::Response&& resp) {
        if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
          THROW_ARANGO_EXCEPTION(resp.combinedResult());
        }
        return replication2::replicated_log::statusFromVelocyPack(resp.slice().get("result"));
      });
}

auto sendReadEntryRequest(network::ConnectionPool *pool, std::string const& server, std::string const& database,
                       LogId id, LogIndex index)
-> futures::Future<std::optional<LogEntry>> {

  auto path = basics::StringUtils::joinT("/", "_api/log", id, "readEntry", index.value);

  network::RequestOptions opts;
  opts.database = database;
  return network::sendRequest(pool, "server:" + server, fuerte::RestVerb::Get, path)
      .thenValue([](network::Response&& resp) {
        if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
          THROW_ARANGO_EXCEPTION(resp.combinedResult());
        }
        auto entry = LogEntry::fromVelocyPack(resp.slice().get("result"));
        return std::optional<LogEntry>(std::move(entry));
      });
}

auto sendTailRequest(network::ConnectionPool* pool, std::string const& server,
                     std::string const& database, LogId id, LogIndex first)
    -> futures::Future<std::unique_ptr<LogIterator>> {
  auto path = basics::StringUtils::joinT("/", "_api/log", id, "tail") + "?first=" + to_string(first);

  network::RequestOptions opts;
  opts.database = database;
  return network::sendRequest(pool, "server:" + server, fuerte::RestVerb::Get, path)
      .thenValue([](network::Response&& resp) -> std::unique_ptr<LogIterator> {
        if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
          THROW_ARANGO_EXCEPTION(resp.combinedResult());
        }

        struct VPackLogIterator final : LogIterator {
          explicit VPackLogIterator(std::shared_ptr<velocypack::Buffer<uint8_t>> buffer_ptr)
              : buffer(std::move(buffer_ptr)), iter(VPackSlice(buffer->data())), end(iter.end()) {}

          auto next() -> std::optional<LogEntry> override {
            if (iter != end) {
              return LogEntry::fromVelocyPack(*iter++);
            }
            return std::nullopt;
          }

         private:
          std::shared_ptr<velocypack::Buffer<uint8_t>> buffer;
          VPackArrayIterator iter;
          VPackArrayIterator end;
        };

        return std::make_unique<VPackLogIterator>(resp.response().copyPayload());
      });
}
}

struct ReplicatedLogMethodsCoord final : ReplicatedLogMethods {
  auto insert(LogId id, LogPayload payload) const
      -> futures::Future<std::shared_ptr<replicated_log::QuorumData const>> override {
    auto& ci = server.getFeature<ClusterFeature>().clusterInfo();

    auto leader = ci.getReplicatedLogLeader(vocbase.name(), id);
    if (!leader) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }

    auto pool = server.getFeature<NetworkFeature>().pool();
    return sendInsertRequest(pool, *leader, vocbase.name(), id, std::move(payload));
  }

  auto getLogEntryByIndex(LogId id, LogIndex index) const
      -> futures::Future<std::optional<LogEntry>> override {
    auto& ci = server.getFeature<ClusterFeature>().clusterInfo();

    auto leader = ci.getReplicatedLogLeader(vocbase.name(), id);
    if (!leader) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }

    auto pool = server.getFeature<NetworkFeature>().pool();
    return sendReadEntryRequest(pool, *leader, vocbase.name(), id, index);
  }

  auto getLogStatus(LogId id) const -> futures::Future<replication2::replicated_log::LogStatus> override {
    auto& ci = server.getFeature<ClusterFeature>().clusterInfo();

    auto leader = ci.getReplicatedLogLeader(vocbase.name(), id);
    if (!leader) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }

    auto pool = server.getFeature<NetworkFeature>().pool();
    return sendLogStatusRequest(pool, *leader, vocbase.name(), id);
  }

  auto tailEntries(LogId id, LogIndex first) const
      -> futures::Future<std::unique_ptr<LogIterator>> override {
    auto& ci = server.getFeature<ClusterFeature>().clusterInfo();

    auto leader = ci.getReplicatedLogLeader(vocbase.name(), id);
    if (!leader) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }

    auto pool = server.getFeature<NetworkFeature>().pool();
    return sendTailRequest(pool, *leader, vocbase.name(), id, first);
  }

  auto createReplicatedLog(replication2::agency::LogPlanSpecification const& spec) const
      -> futures::Future<Result> override {
    return replication2::agency::methods::createReplicatedLog(vocbase.name(), spec);
  }

  auto deleteReplicatedLog(LogId id) const -> futures::Future<Result> override {
    return replication2::agency::methods::deleteReplicatedLog(vocbase.name(), id);
  }

  auto updateTermSpecification(LogId id, replication2::agency::LogPlanTermSpecification const& term) const
      -> futures::Future<Result> override {
    return replication2::agency::methods::updateTermSpecification(vocbase.name(), id, term);
  }

  explicit ReplicatedLogMethodsCoord(TRI_vocbase_t& vocbase,
                                     application_features::ApplicationServer& server)
      : vocbase(vocbase), server(server) {}

 private:
  TRI_vocbase_t& vocbase;
  application_features::ApplicationServer& server;
};

struct ReplicatedLogMethodsDBServ final : ReplicatedLogMethods {
  auto createReplicatedLog(const replication2::agency::LogPlanSpecification& spec) const
      -> futures::Future<Result> override {
    return vocbase.createReplicatedLog(spec.id).result();
  }

  auto deleteReplicatedLog(LogId id) const -> futures::Future<Result> override {
    return vocbase.dropReplicatedLog(id);
  }

  auto getReplicatedLogs() const
  -> futures::Future<std::unordered_map<arangodb::replication2::LogId,
      arangodb::replication2::replicated_log::LogStatus>> override {
    return vocbase.getReplicatedLogs();
  }

  auto getLogStatus(LogId id) const
      -> futures::Future<replication2::replicated_log::LogStatus> override {
    return vocbase.getReplicatedLogById(id).getParticipant()->getStatus();
  }

  auto getLogEntryByIndex(LogId id, LogIndex idx) const -> futures::Future<std::optional<LogEntry>> override {
    return vocbase.getReplicatedLogLeaderById(id)->readReplicatedEntryByIndex(idx);
  }

  auto tailEntries(LogId id, LogIndex idx) const -> futures::Future<std::unique_ptr<LogIterator>> override {
    return vocbase.getReplicatedLogLeaderById(id)->waitForIterator(idx);
  }

  auto appendEntries(LogId id, replicated_log::AppendEntriesRequest const& req) const
      -> futures::Future<replicated_log::AppendEntriesResult> override {
    return vocbase.getReplicatedLogFollowerById(id)->appendEntries(req);
  }

  auto insert(LogId logId, LogPayload payload) const
      -> futures::Future<std::shared_ptr<replicated_log::QuorumData const>> override {
    auto log = vocbase.getReplicatedLogLeaderById(logId);
    auto idx = log->insert(std::move(payload));
    auto f = log->waitFor(idx);
    log->runAsyncStep();  // TODO
    return f;
  }

  explicit ReplicatedLogMethodsDBServ(TRI_vocbase_t& vocbase)
      : vocbase(vocbase) {}

 private:
  TRI_vocbase_t& vocbase;
};

RestStatus RestLogHandler::execute() {
  switch (ServerState::instance()->getRole()) {
    case ServerState::ROLE_DBSERVER:
      return executeByMethod(ReplicatedLogMethodsDBServ(_vocbase));
    case ServerState::ROLE_COORDINATOR:
      return executeByMethod(ReplicatedLogMethodsCoord(_vocbase, server()));
    case ServerState::ROLE_SINGLE:
    case ServerState::ROLE_UNDEFINED:
    case ServerState::ROLE_AGENT:
    default:
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "api only on coordinators or dbservers");
      return RestStatus::DONE;
  }
}

RestStatus RestLogHandler::executeByMethod(ReplicatedLogMethods const& methods) {
  switch (_request->requestType()) {
    case rest::RequestType::GET:
      return handleGetRequest(methods);
    case rest::RequestType::POST:
      return handlePostRequest(methods);
    case rest::RequestType::DELETE_REQ:
      return handleDeleteRequest(methods);
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
  return RestStatus::DONE;
}


RestStatus RestLogHandler::handlePostRequest(ReplicatedLogMethods const& methods) {

  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  if (suffixes.empty()) {
    // create a new log
    replication2::agency::LogPlanSpecification spec(replication2::agency::from_velocypack, body);

    if (spec.currentTerm.has_value()) {
      spec.currentTerm.reset();
    }

    return waitForFuture(methods.createReplicatedLog(spec).thenValue([this](Result&& result) {
      if (result.ok()) {
        generateOk(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
      } else {
        generateError(result);
      }
    }));
  }

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>");
    return RestStatus::DONE;
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};

  if (auto& verb = suffixes[1]; verb == "insert") {
    return waitForFuture(
        methods.insert(logId, LogPayload{body}).thenValue([this](auto&& quorum) {
          VPackBuilder response;
          quorum->toVelocyPack(response);
          generateOk(rest::ResponseCode::ACCEPTED, response.slice());
        }));
  } else if(verb == "updateTermSpecification") {
    auto term = replication2::agency::LogPlanTermSpecification(replication2::agency::from_velocypack, body);
    return waitForFuture(
        methods.updateTermSpecification(logId, term).thenValue([this](auto&& result) {
      if (result.ok()) {
        generateOk(ResponseCode::ACCEPTED, VPackSlice::emptyObjectSlice());
      } else {
        generateError(result);
      }
    }));
  } else if(verb == "becomeLeader") {
    if (!ServerState::instance()->isDBServer()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
    auto& log = _vocbase.getReplicatedLogById(logId);

    auto term = LogTerm{body.get(StaticStrings::Term).getNumericValue<uint64_t>()};
    auto writeConcern = body.get(StaticStrings::WriteConcern).getNumericValue<std::size_t>();
    auto waitForSync = body.get(StaticStrings::WaitForSyncString).isTrue();

    std::vector<std::shared_ptr<replicated_log::AbstractFollower>> follower;
    for (auto const& part : VPackArrayIterator(body.get(StaticStrings::Follower))) {
      auto partId = part.copyString();
      follower.emplace_back(std::make_shared<replicated_log::NetworkAttachedFollower>(server().getFeature<NetworkFeature>().pool(), partId, _vocbase.name(), logId));
    }

    replicated_log::LogLeader::TermData termData;
    termData.id = ServerState::instance()->getId();
    termData.term = term;
    termData.writeConcern = writeConcern;
    termData.waitForSync = waitForSync;
    log.becomeLeader(termData, follower);
    generateOk(rest::ResponseCode::ACCEPTED, VPackSlice::emptyObjectSlice());
  } else if (verb == "becomeFollower") {
    if (!ServerState::instance()->isDBServer()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
    auto& log = _vocbase.getReplicatedLogById(logId);
    auto term = LogTerm{body.get(StaticStrings::Term).getNumericValue<uint64_t>()};
    auto leaderId = body.get(StaticStrings::Leader).copyString();
    log.becomeFollower(ServerState::instance()->getId(), term, leaderId);
    generateOk(rest::ResponseCode::ACCEPTED, VPackSlice::emptyObjectSlice());

  } else if (verb == "appendEntries") {
    auto request = replicated_log::AppendEntriesRequest::fromVelocyPack(body);
    auto f = methods.appendEntries(logId, request).thenValue([this](replicated_log::AppendEntriesResult&& res) {
      VPackBuilder builder;
      res.toVelocyPack(builder);
      // TODO fix the result type here. Currently we always return the error under the
      //      `result` field. Maybe we want to change the HTTP status code as well?
      //      Don't forget to update the deserializer that reads the response!
      generateOk(rest::ResponseCode::ACCEPTED, builder.slice());
    });

    return waitForFuture(std::move(f));
  } else {
    generateError(
        rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
        "expecting one of the resources 'insert', ");

  }
  return RestStatus::DONE;
}

RestStatus RestLogHandler::handleGetRequest(ReplicatedLogMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.empty()) {
    return waitForFuture(methods.getReplicatedLogs().thenValue([this](auto&& logs) {
      VPackBuilder builder;
      {
        VPackObjectBuilder ob(&builder);

        for (auto const& [idx, status] : logs) {
          builder.add(VPackValue(std::to_string(idx.id())));
          std::visit([&](auto const& status) { status.toVelocyPack(builder); }, status);
        }
      }

      generateOk(rest::ResponseCode::OK, builder.slice());
    }));
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};

  if (suffixes.size() == 1) {
    return waitForFuture(methods.getLogStatus(logId).thenValue([this](auto&& status) {
      VPackBuilder buffer;
      std::visit([&](auto const& status) { status.toVelocyPack(buffer); }, status);
      generateOk(rest::ResponseCode::OK, buffer.slice());
    }));
  }

  auto const& verb = suffixes[1];

  if (verb == "tail") {
    if (suffixes.size() != 2) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expect GET /_api/log/<log-id>/tail");
      return RestStatus::DONE;
    }
    LogIndex logIdx{basics::StringUtils::uint64(_request->value("first"))};
    auto fut = methods.tailEntries(logId, logIdx).thenValue([&](std::unique_ptr<LogIterator> iter) {
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
  } else if (verb == "readEntry") {
    if (suffixes.size() != 3) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expect GET /_api/log/<log-id>/readEntry/<id>");
      return RestStatus::DONE;
    }
    LogIndex logIdx{basics::StringUtils::uint64(suffixes[2])};

    return waitForFuture(
        methods.getLogEntryByIndex(logId, logIdx).thenValue([this](std::optional<LogEntry>&& entry) {
          if (entry) {
            VPackBuilder result;
            entry->toVelocyPack(result);
            generateOk(rest::ResponseCode::OK, result.slice());
          } else {
            generateError(rest::ResponseCode::NOT_FOUND,
                          TRI_ERROR_HTTP_NOT_FOUND, "log index not found");
          }
        }));
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expecting one of the resources 'dump', 'readEntry'");
  }
  return RestStatus::DONE;
}

RestStatus RestLogHandler::handleDeleteRequest(ReplicatedLogMethods const& methods) {

  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect DELETE /_api/log/<log-id>");
    return RestStatus::DONE;
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  return waitForFuture(methods.deleteReplicatedLog(logId).thenValue([this](Result&& result) {
    if (!result.ok()) {
      generateError(result);
    } else {
      generateOk(rest::ResponseCode::ACCEPTED, VPackSlice::emptyObjectSlice());
    }
  }));
}

RestLogHandler::RestLogHandler(application_features::ApplicationServer& server,
                               GeneralRequest* req, GeneralResponse* resp)
    : RestVocbaseBaseHandler(server, req, resp) {}
RestLogHandler::~RestLogHandler() = default;
