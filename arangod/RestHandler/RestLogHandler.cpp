//
// Created by lars on 16/04/2021.
//

#include <velocypack/Iterator.h>

#include <Cluster/ServerState.h>
#include <Network/ConnectionPool.h>
#include <Network/Methods.h>
#include <Network/NetworkFeature.h>
#include <Replication2/InMemoryLog.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/overload.h"
#include "RestLogHandler.h"

using namespace arangodb;
using namespace arangodb::replication2;

RestStatus RestLogHandler::execute() {
  switch (_request->requestType()) {
    case rest::RequestType::GET:
      return handleGetRequest();
    case rest::RequestType::POST:
      return handlePostRequest();
      break;
    case rest::RequestType::DELETE_REQ:
      return handleDeleteRequest();
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
  return RestStatus::DONE;
}

struct FakePersistedLog : PersistedLog {
  explicit FakePersistedLog(LogId id) : PersistedLog(id) {};
  ~FakePersistedLog() override = default;
  auto insert(std::shared_ptr<LogIterator> iter) -> Result override {
    LOG_DEVEL << "Fake log insert";
    return Result();
  }
  auto read(LogIndex start) -> std::shared_ptr<LogIterator> override {
    struct FakeLogIterator : LogIterator {
      auto next() -> std::optional<LogEntry> override {
        return std::nullopt;
      }
    };

    return std::shared_ptr<FakeLogIterator>();
  }
  auto removeFront(LogIndex stop) -> Result override { return Result(); }
  auto removeBack(LogIndex start) -> Result override { return Result(); }
  auto drop() -> Result override { return Result(); }
};

struct FakeLogFollower : LogFollower {
  explicit FakeLogFollower(network::ConnectionPool* pool, ParticipantId id,
                           std::string database, LogId logId)
      :  pool(pool), id(std::move(id)), database(database), logId(logId) {}
  auto participantId() const noexcept -> ParticipantId override { return id; }
  auto appendEntries(AppendEntriesRequest request)
      -> arangodb::futures::Future<AppendEntriesResult> override {
    VPackBufferUInt8  buffer;
    {
      VPackBuilder builder(buffer);
      request.toVelocyPack(builder);
    }

    auto path = "_api/log/" + std::to_string(logId.id()) + "/appendEntries";

    network::RequestOptions opts;
    opts.database = database;
    LOG_DEVEL << "sending append entries to " << id << " with payload " << VPackSlice(buffer.data()).toJson();
    auto f = network::sendRequest(pool, "server:" + id, fuerte::RestVerb::Post, path, std::move(buffer), opts);

    return std::move(f).thenValue([this](network::Response result) -> AppendEntriesResult {
      LOG_DEVEL << "Append entries for " << id << " returned, fuerte ok = " << result.ok();
      if (result.fail()) {
        return AppendEntriesResult{false, LogTerm(0)};
      }
      LOG_DEVEL << "Result for " << id << " is " << result.slice().toJson();
      TRI_ASSERT(result.slice().get("error").isFalse()); // TODO
      return AppendEntriesResult::fromVelocyPack(result.slice().get("result"));
    });

  }

  network::ConnectionPool *pool;
  ParticipantId id;
  std::string database;
  LogId logId;
};

RestStatus RestLogHandler::handlePostRequest() {

  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  if (suffixes.empty()) {
    // create a new log
    LogId id{body.get("id").getNumericValue<uint64_t>()};

    if (auto it = _vocbase._logs.find(id); it != _vocbase._logs.end()) {
      generateError(rest::ResponseCode::CONFLICT, TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER, "log with id already exists");
      return RestStatus::DONE;
    }

    auto participantId = ServerState::instance()->getId();
    auto state = std::make_shared<InMemoryState>();
    auto persistedLog = std::make_shared<FakePersistedLog>(id);

    auto log = std::make_shared<InMemoryLog>(participantId, state, persistedLog);
    _vocbase._logs.emplace(id, log);
    generateOk(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
    return RestStatus::DONE;
  }

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>");
    return RestStatus::DONE;
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  std::shared_ptr<InMemoryLog> log;
  {
    if (auto it = _vocbase._logs.find(logId); it != _vocbase._logs.end()) {
      log = it->second;
    } else {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, "unknown log");
      return RestStatus::DONE;
    }
  }

  if (auto& verb = suffixes[1]; verb == "insert") {
    auto idx = log->insert(LogPayload{body.toJson()});

    auto f = log->waitFor(idx).thenValue([this, idx](std::shared_ptr<QuorumData>&& quorum) {
      VPackBuilder response;
      {
        VPackObjectBuilder ob(&response);
        response.add("index", VPackValue(quorum->index.value));
        response.add("term", VPackValue(quorum->term.value));
        VPackArrayBuilder ab(&response, "quorum");
        for (auto& part : quorum->quorum) {
          response.add(VPackValue(part));
        }
      }
      LOG_DEVEL << "insert completed idx = " << idx.value;
      generateOk(rest::ResponseCode::ACCEPTED, response.slice());
    });

    log->runAsyncStep(); // TODO
    return waitForFuture(std::move(f));

  } else if(verb == "becomeLeader") {

    auto term = LogTerm{body.get("term").getNumericValue<uint64_t>()};
    auto writeConcern = body.get("writeConcern").getNumericValue<std::size_t>();

    std::vector<std::shared_ptr<LogFollower>> follower;
    for (auto const& part : VPackArrayIterator(body.get("follower"))) {
      auto partId = part.copyString();
      follower.emplace_back(std::make_shared<FakeLogFollower>(server().getFeature<NetworkFeature>().pool(), partId, _vocbase.name(), logId));
    }

    log->becomeLeader(term, follower, writeConcern);
    generateOk(rest::ResponseCode::ACCEPTED, VPackSlice::emptyObjectSlice());
  } else if (verb == "becomeFollower") {
    auto term = LogTerm{body.get("term").getNumericValue<uint64_t>()};
    auto leaderId = body.get("leader").copyString();
    log->becomeFollower(term, leaderId);
    generateOk(rest::ResponseCode::ACCEPTED, VPackSlice::emptyObjectSlice());

  } else if (verb == "appendEntries") {
    auto request = AppendEntriesRequest::fromVelocyPack(body);
    auto f = log->appendEntries(std::move(request)).thenValue([this](AppendEntriesResult&& res) {
      VPackBuilder builder;
      res.toVelocyPack(builder);
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

RestStatus RestLogHandler::handleGetRequest() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.empty()) {
    VPackBuilder response;

    {
      VPackObjectBuilder arrayBuilder(&response);
      for (auto const& [id, log] : _vocbase._logs) {
        auto stats = log->getLocalStatistics();

        auto idString = std::to_string(id.id());
        response.add(VPackValue(idString));
        stats.toVelocyPack(response);
      }
    }

    generateOk(rest::ResponseCode::OK, response.slice());
    return RestStatus::DONE;
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  std::shared_ptr<InMemoryLog> log;
  {
    if (auto it = _vocbase._logs.find(logId); it != _vocbase._logs.end()) {
      log = it->second;
    } else {
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, "unknown log");
      return RestStatus::DONE;
    }
  }

  if (suffixes.size() == 1) {
    VPackBuilder buffer;
    std::visit([&](auto const& status) { status.toVelocyPack(buffer); }, log->getStatus());
    generateOk(rest::ResponseCode::OK, buffer.slice());
    return RestStatus::DONE;
  } else if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/log/<log-id>");
    return RestStatus::DONE;
  }

  auto const& verb = suffixes[1];

  if (verb == "dump") {
    // dump log
    auto [idx, snapshot] = log->createSnapshot();
    VPackBuilder result;

    {
      VPackObjectBuilder ob(&result);
      result.add("logId", VPackValue(logId.id()));
      result.add("index", VPackValue(idx.value));
      {
        VPackObjectBuilder state(&result, "state");
        for (auto const& kv : snapshot->_state) {
          result.add(kv.first, kv.second.slice());
        }
      }
    }

    generateOk(rest::ResponseCode::OK, result.slice());
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expecting one of the resources 'dump', ");
  }
  return RestStatus::DONE;
}

RestLogHandler::RestLogHandler(application_features::ApplicationServer& server,
                               GeneralRequest* req, GeneralResponse* resp)
    : RestVocbaseBaseHandler(server, req, resp) {}

RestStatus RestLogHandler::handleDeleteRequest() {

  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect DELETE /_api/log/<log-id>");

  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  if (auto it = _vocbase._logs.find(logId); it == _vocbase._logs.end()) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, "log not found");
  } else {
    _vocbase._logs.erase(it);
    generateOk(rest::ResponseCode::ACCEPTED, VPackSlice::emptyObjectSlice());
  }

  return RestStatus::DONE;
}

RestLogHandler::~RestLogHandler() = default;
