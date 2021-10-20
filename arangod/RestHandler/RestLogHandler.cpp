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
#include "Replication2/ReplicatedLog/LogStatus.h"
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
    return Result(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto deleteReplicatedLog(LogId id) const -> futures::Future<Result> {
    return Result(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto getReplicatedLogs() const
      -> futures::Future<std::unordered_map<arangodb::replication2::LogId, arangodb::replication2::replicated_log::LogStatus>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto getLogStatus(LogId) const
      -> futures::Future<replication2::replicated_log::LogStatus> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto getLogEntryByIndex(LogId, LogIndex) const
      -> futures::Future<std::optional<PersistingLogEntry>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto appendEntries(LogId, replicated_log::AppendEntriesRequest const&) const
      -> futures::Future<replicated_log::AppendEntriesResult> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto tailEntries(LogId, LogIndex, std::size_t limit) const
      -> futures::Future<std::unique_ptr<LogIterator>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto insert(LogId, LogPayload) const
      -> futures::Future<std::pair<LogIndex, replicated_log::WaitForResult>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto releaseIndex(LogId, LogIndex) const -> futures::Future<Result> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual auto updateTermSpecification(LogId, replication2::agency::LogPlanTermSpecification const& term) const
      -> futures::Future<Result> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
};

namespace {

auto sendInsertRequest(network::ConnectionPool* pool, std::string const& server,
                       std::string const& database, LogId id, LogPayload payload)
    -> futures::Future<std::pair<LogIndex, replicated_log::WaitForResult>> {
  auto path = basics::StringUtils::joinT("/", "_api/log", id, "insert");

  network::RequestOptions opts;
  opts.database = database;
  return network::sendRequest(pool, "server:" + server, fuerte::RestVerb::Post,
                              path, payload.dummy, opts)
      .thenValue([](network::Response&& resp) {
        if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
          THROW_ARANGO_EXCEPTION(resp.combinedResult());
        }
        auto result = resp.slice().get("result");
        auto waitResult = result.get("result");

        auto quorum = std::make_shared<replication2::replicated_log::QuorumData const>(
            waitResult.get("quorum"));
        auto commitIndex = waitResult.get("commitIndex").extract<LogIndex>();
        auto index = result.get("index").extract<LogIndex>();
        return std::make_pair(index, replicated_log::WaitForResult(commitIndex,
                                                                   std::move(quorum)));
      });
}

auto sendLogStatusRequest(network::ConnectionPool* pool, std::string const& server,
                          std::string const& database, LogId id)
    -> futures::Future<replication2::replicated_log::LogStatus> {
  auto path = basics::StringUtils::joinT("/", "_api/log", id);

  network::RequestOptions opts;
  opts.database = database;
  return network::sendRequest(pool, "server:" + server, fuerte::RestVerb::Get, path)
      .thenValue([](network::Response&& resp) {
        if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
          THROW_ARANGO_EXCEPTION(resp.combinedResult());
        }
        return replication2::replicated_log::LogStatus::fromVelocyPack(
            resp.slice().get("result"));
      });
}

auto sendReadEntryRequest(network::ConnectionPool* pool, std::string const& server,
                          std::string const& database, LogId id, LogIndex index)
    -> futures::Future<std::optional<PersistingLogEntry>> {
  auto path = basics::StringUtils::joinT("/", "_api/log", id, "readEntry", index.value);

  network::RequestOptions opts;
  opts.database = database;
  return network::sendRequest(pool, "server:" + server, fuerte::RestVerb::Get, path, {}, opts)
      .thenValue([](network::Response&& resp) {
        if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
          THROW_ARANGO_EXCEPTION(resp.combinedResult());
        }
        auto entry =
            PersistingLogEntry::fromVelocyPack(resp.slice().get("result"));
        return std::optional<PersistingLogEntry>(std::move(entry));
      });
}

auto sendTailRequest(network::ConnectionPool* pool, std::string const& server,
                     std::string const& database, LogId id, LogIndex first,
                     std::size_t limit) -> futures::Future<std::unique_ptr<LogIterator>> {
  auto path = basics::StringUtils::joinT("/", "_api/log", id, "tail");

  network::RequestOptions opts;
  opts.database = database;
  opts.parameters["first"] = to_string(first);
  opts.parameters["limit"] = std::to_string(limit);
  return network::sendRequest(pool, "server:" + server, fuerte::RestVerb::Get, path, {}, opts)
      .thenValue([](network::Response&& resp) -> std::unique_ptr<LogIterator> {
        if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
          THROW_ARANGO_EXCEPTION(resp.combinedResult());
        }

        struct VPackLogIterator final : LogIterator {
          explicit VPackLogIterator(std::shared_ptr<velocypack::Buffer<uint8_t>> buffer_ptr)
              : buffer(std::move(buffer_ptr)),
                iter(VPackSlice(buffer->data()).get("result")),
                end(iter.end()) {}

          auto next() -> std::optional<LogEntryView> override {
            while (iter != end) {
              return LogEntryView::fromVelocyPack(*iter++);
            }
            return std::nullopt;
          }

         private:
          std::shared_ptr<velocypack::Buffer<uint8_t>> buffer;
          VPackArrayIterator iter;
          VPackArrayIterator end;
        };

        return std::make_unique<VPackLogIterator>(resp.response().stealPayload());
      });
}
}  // namespace

struct ReplicatedLogMethodsCoord final : ReplicatedLogMethods {
  auto getLogLeader(LogId id) const {
    auto leader = clusterInfo.getReplicatedLogLeader(vocbase.name(), id);
    if (!leader) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }

    return *leader;
  }

  auto insert(LogId id, LogPayload payload) const
      -> futures::Future<std::pair<LogIndex, replicated_log::WaitForResult>> override {
    return sendInsertRequest(pool, getLogLeader(id), vocbase.name(), id, std::move(payload));
  }

  auto getLogEntryByIndex(LogId id, LogIndex index) const
      -> futures::Future<std::optional<PersistingLogEntry>> override {
    return sendReadEntryRequest(pool, getLogLeader(id), vocbase.name(), id, index);
  }

  auto getLogStatus(LogId id) const
      -> futures::Future<replication2::replicated_log::LogStatus> override {
    return sendLogStatusRequest(pool, getLogLeader(id), vocbase.name(), id);
  }

  auto tailEntries(LogId id, LogIndex first, std::size_t limit) const
      -> futures::Future<std::unique_ptr<LogIterator>> override {
    return sendTailRequest(pool, getLogLeader(id), vocbase.name(), id, first, limit);
  }

  auto createReplicatedLog(replication2::agency::LogPlanSpecification const& spec) const
      -> futures::Future<Result> override {
    return replication2::agency::methods::createReplicatedLog(vocbase.name(), spec);
  }

  auto deleteReplicatedLog(LogId id) const -> futures::Future<Result> override {
    return replication2::agency::methods::deleteReplicatedLog(vocbase.name(), id);
  }

  auto updateTermSpecification(LogId id,
                               replication2::agency::LogPlanTermSpecification const& term) const
      -> futures::Future<Result> override {
    return replication2::agency::methods::updateTermSpecification(vocbase.name(), id, term);
  }

  explicit ReplicatedLogMethodsCoord(TRI_vocbase_t& vocbase, ClusterInfo& clusterInfo,
                                     network::ConnectionPool* pool)
      : vocbase(vocbase), clusterInfo(clusterInfo), pool(pool) {}

 private:
  TRI_vocbase_t& vocbase;
  ClusterInfo& clusterInfo;
  network::ConnectionPool* pool;
};

struct ReplicatedLogMethodsDBServ final : ReplicatedLogMethods {
  auto createReplicatedLog(const replication2::agency::LogPlanSpecification& spec) const
      -> futures::Future<Result> override {
    return vocbase.createReplicatedLog(spec.id, std::nullopt).result();
  }

  auto deleteReplicatedLog(LogId id) const -> futures::Future<Result> override {
    return vocbase.dropReplicatedLog(id);
  }

  auto getReplicatedLogs() const
      -> futures::Future<std::unordered_map<arangodb::replication2::LogId, arangodb::replication2::replicated_log::LogStatus>> override {
    return vocbase.getReplicatedLogs();
  }

  auto getLogStatus(LogId id) const
      -> futures::Future<replication2::replicated_log::LogStatus> override {
    return vocbase.getReplicatedLogById(id)->getParticipant()->getStatus();
  }

  auto getLogEntryByIndex(LogId id, LogIndex idx) const
      -> futures::Future<std::optional<PersistingLogEntry>> override {
    return vocbase.getReplicatedLogLeaderById(id)->readReplicatedEntryByIndex(idx);
  }

  auto tailEntries(LogId id, LogIndex idx, std::size_t limit) const
      -> futures::Future<std::unique_ptr<LogIterator>> override {
    struct LimitingIterator : LogIterator {
      LimitingIterator(size_t limit, std::unique_ptr<LogIterator> source)
          : _limit(limit), _source(std::move(source)) {}
      auto next() -> std::optional<LogEntryView> override {
        if (_limit == 0) {
          return std::nullopt;
        }

        _limit -= 1;
        return _source->next();
      }

      std::size_t _limit;
      std::unique_ptr<LogIterator> _source;
    };

    return vocbase.getReplicatedLogLeaderById(id)->waitForIterator(idx).thenValue(
        [limit](std::unique_ptr<LogIterator> iter) -> std::unique_ptr<LogIterator> {
          return std::make_unique<LimitingIterator>(limit, std::move(iter));
        });
  }

  auto appendEntries(LogId id, replicated_log::AppendEntriesRequest const& req) const
      -> futures::Future<replicated_log::AppendEntriesResult> override {
    return vocbase.getReplicatedLogFollowerById(id)->appendEntries(req);
  }

  auto insert(LogId logId, LogPayload payload) const
      -> futures::Future<std::pair<LogIndex, replicated_log::WaitForResult>> override {
    auto log = vocbase.getReplicatedLogLeaderById(logId);
    auto idx = log->insert(std::move(payload));
    return log->waitFor(idx).thenValue(
        [idx](auto&& result) { return std::make_pair(idx, std::move(result)); });
  }

  auto releaseIndex(LogId id, LogIndex idx) const -> futures::Future<Result> override {
    auto log = vocbase.getReplicatedLogById(id);
    return log->getParticipant()->release(idx);
  }

  explicit ReplicatedLogMethodsDBServ(TRI_vocbase_t& vocbase)
      : vocbase(vocbase) {}

 private:
  TRI_vocbase_t& vocbase;
};

RestStatus RestLogHandler::execute() {
  // for now required admin access to the database
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  switch (ServerState::instance()->getRole()) {
    case ServerState::ROLE_DBSERVER:
      return executeByMethod(ReplicatedLogMethodsDBServ(_vocbase));
    case ServerState::ROLE_COORDINATOR: {
      auto pool = server().getFeature<NetworkFeature>().pool();
      auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
      return executeByMethod(ReplicatedLogMethodsCoord(_vocbase, ci, pool));
    }
    case ServerState::ROLE_SINGLE:
    case ServerState::ROLE_UNDEFINED:
    case ServerState::ROLE_AGENT:
    default:
      generateError(ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED,
                    "api only on available coordinators or dbservers");
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
        methods.insert(logId, LogPayload::createFromSlice(body)).thenValue([this](auto&& waitForResult) {
          VPackBuilder response;
          {
            VPackObjectBuilder result(&response);
            response.add("index", VPackValue(waitForResult.first));
            response.add(VPackValue("result"));
            waitForResult.second.toVelocyPack(response);
          }
          generateOk(rest::ResponseCode::ACCEPTED, response.slice());
        }));
  } else if (verb == "release") {
    auto idx = LogIndex{basics::StringUtils::uint64(_request->value("index"))};
    return waitForFuture(methods.releaseIndex(logId, idx).thenValue([this](Result&& res) {
      if (res.fail()) {
        generateError(res);
      } else {
        generateOk(rest::ResponseCode::ACCEPTED, VPackSlice::noneSlice());
      }
    }));
  } else if (verb == "updateTermSpecification") {
    auto term = replication2::agency::LogPlanTermSpecification(replication2::agency::from_velocypack,
                                                               body);
    return waitForFuture(
        methods.updateTermSpecification(logId, term).thenValue([this](auto&& result) {
          if (result.ok()) {
            generateOk(ResponseCode::ACCEPTED, VPackSlice::emptyObjectSlice());
          } else {
            generateError(result);
          }
        }));
  } else if (verb == "becomeLeader") {
    if (!ServerState::instance()->isDBServer()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
    auto log = _vocbase.getReplicatedLogById(logId);

    auto term = LogTerm{body.get(StaticStrings::Term).getNumericValue<uint64_t>()};
    auto writeConcern =
        body.get(StaticStrings::WriteConcern).getNumericValue<std::size_t>();
    auto waitForSync = body.get(StaticStrings::WaitForSyncString).isTrue();

    std::vector<std::shared_ptr<replicated_log::AbstractFollower>> follower;
    for (auto const& part : VPackArrayIterator(body.get(StaticStrings::Follower))) {
      auto partId = part.copyString();
      follower.emplace_back(std::make_shared<replicated_log::NetworkAttachedFollower>(
          server().getFeature<NetworkFeature>().pool(), partId, _vocbase.name(), logId));
    }

    replication2::LogConfig config;
    config.waitForSync = waitForSync;
    config.writeConcern = writeConcern;
    log->becomeLeader(config, ServerState::instance()->getId(), term, follower);
    generateOk(rest::ResponseCode::ACCEPTED, VPackSlice::emptyObjectSlice());
  } else if (verb == "becomeFollower") {
    if (!ServerState::instance()->isDBServer()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
    auto log = _vocbase.getReplicatedLogById(logId);
    auto term = LogTerm{body.get(StaticStrings::Term).getNumericValue<uint64_t>()};
    auto leaderId = body.get(StaticStrings::Leader).copyString();
    log->becomeFollower(ServerState::instance()->getId(), term, leaderId);
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
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expecting one of the resources 'insert', ");
  }
  return RestStatus::DONE;
}

RestStatus RestLogHandler::handleGetRequest(ReplicatedLogMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.empty()) {
    return handleGet(methods);
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};

  if (suffixes.size() == 1) {
    return handleGetLog(methods, logId);
  }

  auto const& verb = suffixes[1];
  if (verb == "tail") {
    return handleGetTail(methods, logId);
  } else if (verb == "readEntry") {
    return handleGetReadEntry(methods, logId);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expecting one of the resources 'tail', 'readEntry'");
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

RestStatus RestLogHandler::handleGet(ReplicatedLogMethods const& methods) {
  return waitForFuture(methods.getReplicatedLogs().thenValue([this](auto&& logs) {
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
  return waitForFuture(methods.getLogStatus(logId).thenValue([this](auto&& status) {
    VPackBuilder buffer;
    status.toVelocyPack(buffer);
    generateOk(rest::ResponseCode::OK, buffer.slice());
  }));
}

RestStatus RestLogHandler::handleGetTail(const ReplicatedLogMethods& methods,
                                         replication2::LogId logId) {
  if (_request->suffixes().size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/tail");
    return RestStatus::DONE;
  }
  LogIndex logIdx{basics::StringUtils::uint64(_request->value("first"))};
  std::size_t limit{basics::StringUtils::uint64(_request->value("limit"))};

  auto fut =
      methods.tailEntries(logId, logIdx, limit).thenValue([&](std::unique_ptr<LogIterator> iter) {
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

RestStatus RestLogHandler::handleGetReadEntry(const ReplicatedLogMethods& methods,
                                              replication2::LogId logId) {
  auto const& suffixes = _request->suffixes();
  if (suffixes.size() != 3) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>/readEntry/<id>");
    return RestStatus::DONE;
  }
  LogIndex logIdx{basics::StringUtils::uint64(suffixes[2])};

  return waitForFuture(
      methods.getLogEntryByIndex(logId, logIdx).thenValue([this](std::optional<PersistingLogEntry>&& entry) {
        if (entry) {
          VPackBuilder result;
          entry->toVelocyPack(result);
          generateOk(rest::ResponseCode::OK, result.slice());
        } else {
          generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                        "log index not found");
        }
      }));
}
