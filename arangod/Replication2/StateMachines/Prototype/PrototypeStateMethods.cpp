////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include <Basics/ResultT.h>
#include <Basics/Exceptions.h>
#include <Basics/Exceptions.tpp>
#include <Basics/voc-errors.h>
#include <Futures/Future.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedState.tpp"
#include "Replication2/Methods.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "VocBase/vocbase.h"
#include "Random/RandomGenerator.h"

#include "PrototypeFollowerState.h"
#include "PrototypeLeaderState.h"
#include "PrototypeStateMachine.h"
#include "PrototypeStateMethods.h"
#include "Inspection/VPack.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;

struct PrototypeStateMethodsDBServer final : PrototypeStateMethods {
  explicit PrototypeStateMethodsDBServer(TRI_vocbase_t& vocbase)
      : _vocbase(vocbase) {}

  [[nodiscard]] auto compareExchange(LogId id, std::string key,
                                     std::string oldValue, std::string newValue,
                                     PrototypeWriteOptions options) const
      -> futures::Future<ResultT<LogIndex>> override {
    auto leader = getPrototypeStateLeaderById(id);
    return leader->compareExchange(std::move(key), std::move(oldValue),
                                   std::move(newValue), options);
  }

  [[nodiscard]] auto insert(
      LogId id, std::unordered_map<std::string, std::string> const& entries,
      PrototypeWriteOptions options) const
      -> futures::Future<LogIndex> override {
    auto leader = getPrototypeStateLeaderById(id);
    return leader->set(entries, options);
  };

  auto get(LogId id, std::vector<std::string> keys,
           ReadOptions const& readOptions) const
      -> futures::Future<
          ResultT<std::unordered_map<std::string, std::string>>> override {
    auto stateMachine =
        std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
            _vocbase.getReplicatedStateById(id).get());
    if (stateMachine == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, basics::StringUtils::concatT(
                                  "Failed to get ProtoypeState with id ", id));
    }

    if (readOptions.readFrom.has_value()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "reading on followers is not implemented on dbservers");
    }

    auto leader = stateMachine->getLeader();
    if (leader != nullptr) {
      return leader->get(std::move(keys), readOptions.waitForApplied);
    }

    if (!readOptions.allowDirtyRead) {
      THROW_ARANGO_EXCEPTION(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER);
    }

    auto follower = stateMachine->getFollower();
    if (follower != nullptr) {
      return follower->get(std::move(keys), readOptions.waitForApplied);
    }

    THROW_ARANGO_EXCEPTION(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_PARTICIPANT_GONE);
  }

  virtual auto waitForApplied(LogId id, LogIndex waitForIndex) const
      -> futures::Future<Result> override {
    auto stateMachine =
        std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
            _vocbase.getReplicatedStateById(id).get());
    if (stateMachine == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, basics::StringUtils::concatT(
                                  "Failed to get ProtoypeState with id ", id));
    }
    auto leader = stateMachine->getLeader();
    if (leader != nullptr) {
      return leader->waitForApplied(waitForIndex).thenValue([](auto&&) {
        return Result{};
      });
    }
    auto follower = stateMachine->getFollower();
    if (follower != nullptr) {
      return follower->waitForApplied(waitForIndex).thenValue([](auto&&) {
        return Result{};
      });
    }
    THROW_ARANGO_EXCEPTION(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_PARTICIPANT_GONE);
  }

  [[nodiscard]] auto getSnapshot(LogId id, LogIndex waitForIndex) const
      -> futures::Future<
          ResultT<std::unordered_map<std::string, std::string>>> override {
    auto leader = getPrototypeStateLeaderById(id);
    return leader->getSnapshot(waitForIndex);
  }

  [[nodiscard]] auto remove(LogId id, std::string key,
                            PrototypeWriteOptions options) const
      -> futures::Future<LogIndex> override {
    auto leader = getPrototypeStateLeaderById(id);
    return leader->remove(std::move(key), options);
  }

  [[nodiscard]] auto remove(LogId id, std::vector<std::string> keys,
                            PrototypeWriteOptions options) const
      -> futures::Future<LogIndex> override {
    auto leader = getPrototypeStateLeaderById(id);
    return leader->remove(std::move(keys), options);
  }

  auto status(LogId id) const
      -> futures::Future<ResultT<PrototypeStatus>> override {
    std::ignore = getPrototypeStateLeaderById(id);
    return PrototypeStatus{id};  // TODO
  }

  auto createState(CreateOptions options) const
      -> futures::Future<ResultT<CreateResult>> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto drop(LogId id) const -> futures::Future<Result> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

 private:
  [[nodiscard]] auto getPrototypeStateLeaderById(LogId id) const
      -> std::shared_ptr<PrototypeLeaderState> {
    auto stateMachine =
        std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
            _vocbase.getReplicatedStateById(id).get());
    if (stateMachine == nullptr) {
      using namespace fmt::literals;
      throw basics::Exception::fmt(
          ADB_HERE, TRI_ERROR_REPLICATION_REPLICATED_STATE_NOT_FOUND,
          "id"_a = id, "type"_a = "PrototypeState");
    }
    auto leader = stateMachine->getLeader();
    if (leader == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_CLUSTER_NOT_LEADER,
          basics::StringUtils::concatT(
              "Failed to get leader of ProtoypeState with id ", id));
    }
    return leader;
  }

 private:
  TRI_vocbase_t& _vocbase;
};

struct PrototypeStateMethodsCoordinator final
    : PrototypeStateMethods,
      std::enable_shared_from_this<PrototypeStateMethodsCoordinator> {
  [[nodiscard]] auto compareExchange(LogId id, std::string key,
                                     std::string oldValue, std::string newValue,
                                     PrototypeWriteOptions options) const
      -> futures::Future<ResultT<LogIndex>> override {
    auto path =
        basics::StringUtils::joinT("/", "_api/prototype-state", id, "cmp-ex");
    network::RequestOptions opts;
    opts.database = _vocbase.name();
    opts.param("waitForApplied", std::to_string(options.waitForApplied));
    opts.param("waitForSync", std::to_string(options.waitForSync));
    opts.param("waitForCommit", std::to_string(options.waitForCommit));

    VPackBuilder builder{};
    {
      VPackObjectBuilder ob{&builder};
      builder.add(VPackValue(key));
      {
        VPackObjectBuilder ob2{&builder};
        builder.add("oldValue", oldValue);
        builder.add("newValue", newValue);
      }
    }

    return network::sendRequest(_pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Put, path,
                                builder.bufferRef(), opts)
        .thenValue([](network::Response&& resp) -> ResultT<LogIndex> {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            auto r = resp.combinedResult();
            r = r.mapError([&](result::Error error) {
              error.appendErrorMessage(" while contacting server " +
                                       resp.serverId());
              return error;
            });
            return r;
          } else {
            auto slice = resp.slice();
            if (auto result = slice.get("result");
                result.isObject() && result.length() == 1) {
              return result.get("index").extract<LogIndex>();
            }
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_INTERNAL,
                basics::StringUtils::concatT(
                    "expected result containing index in leader response: ",
                    slice.toJson()));
          }
        });
  }

  [[nodiscard]] auto insert(
      LogId id, std::unordered_map<std::string, std::string> const& entries,
      PrototypeWriteOptions options) const
      -> futures::Future<LogIndex> override {
    auto path =
        basics::StringUtils::joinT("/", "_api/prototype-state", id, "insert");
    network::RequestOptions opts;
    opts.database = _vocbase.name();
    opts.param("waitForApplied", std::to_string(options.waitForApplied));
    opts.param("waitForSync", std::to_string(options.waitForSync));
    opts.param("waitForCommit", std::to_string(options.waitForCommit));

    VPackBuilder builder{};
    {
      VPackObjectBuilder ob{&builder};
      for (auto const& [key, value] : entries) {
        builder.add(key, VPackValue(value));
      }
    }
    return network::sendRequest(_pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Post, path,
                                builder.bufferRef(), opts)
        .thenValue([](network::Response&& resp) -> LogIndex {
          return processLogIndexResponse(std::move(resp));
        });
  }

  auto get(LogId id, std::vector<std::string> keys,
           ReadOptions const& readOptions) const
      -> futures::Future<
          ResultT<std::unordered_map<std::string, std::string>>> override {
    auto path = basics::StringUtils::joinT("/", "_api/prototype-state", id,
                                           "multi-get");
    network::RequestOptions opts;
    opts.database = _vocbase.name();
    opts.param("waitForApplied",
               std::to_string(readOptions.waitForApplied.value));
    opts.param("allowDirtyRead", std::to_string(readOptions.allowDirtyRead));

    VPackBuilder builder{};
    {
      VPackArrayBuilder ab{&builder};
      for (auto const& key : keys) {
        builder.add(VPackValue(key));
      }
    }

    auto server = std::invoke([&]() -> ParticipantId {
      if (readOptions.readFrom) {
        return *readOptions.readFrom;
      }
      return getLogLeader(id);
    });

    return network::sendRequest(_pool, "server:" + server,
                                fuerte::RestVerb::Post, path,
                                builder.bufferRef(), opts)
        .thenValue(
            [](network::Response&& resp)
                -> ResultT<std::unordered_map<std::string, std::string>> {
              if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
                auto r = resp.combinedResult();
                r = r.mapError([&](result::Error error) {
                  error.appendErrorMessage(" while contacting server " +
                                           resp.serverId());
                  return error;
                });
                return r;
              } else {
                auto slice = resp.slice();
                if (auto result = slice.get("result"); result.isObject()) {
                  std::unordered_map<std::string, std::string> map;
                  for (auto it : VPackObjectIterator{result}) {
                    map.emplace(it.key.copyString(), it.value.copyString());
                  }
                  return {map};
                }
                THROW_ARANGO_EXCEPTION_MESSAGE(
                    TRI_ERROR_INTERNAL, basics::StringUtils::concatT(
                                            "expected result containing map "
                                            "in leader response: ",
                                            slice.toJson()));
              }
            });
  }

  [[nodiscard]] auto getSnapshot(LogId id, LogIndex waitForIndex) const
      -> futures::Future<
          ResultT<std::unordered_map<std::string, std::string>>> override {
    auto path =
        basics::StringUtils::joinT("/", "_api/prototype-state", id, "snapshot");
    network::RequestOptions opts;
    opts.database = _vocbase.name();
    opts.param("waitForIndex", std::to_string(waitForIndex.value));

    return network::sendRequest(_pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue(
            [](network::Response&& resp)
                -> ResultT<std::unordered_map<std::string, std::string>> {
              if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
                THROW_ARANGO_EXCEPTION(resp.combinedResult());
              } else {
                auto slice = resp.slice();
                if (auto result = slice.get("result"); result.isObject()) {
                  std::unordered_map<std::string, std::string> map;
                  for (auto it : VPackObjectIterator{result}) {
                    map.emplace(it.key.copyString(), it.value.copyString());
                  }
                  return map;
                }
                THROW_ARANGO_EXCEPTION_MESSAGE(
                    TRI_ERROR_INTERNAL, basics::StringUtils::concatT(
                                            "expected result containing map "
                                            "in leader response: ",
                                            slice.toJson()));
              }
            });
  }

  [[nodiscard]] auto remove(LogId id, std::string key,
                            PrototypeWriteOptions options) const
      -> futures::Future<LogIndex> override {
    auto path = basics::StringUtils::joinT("/", "_api/prototype-state", id,
                                           "entry", key);
    network::RequestOptions opts;
    opts.database = _vocbase.name();
    opts.param("waitForApplied", std::to_string(options.waitForApplied));
    opts.param("waitForSync", std::to_string(options.waitForSync));
    opts.param("waitForCommit", std::to_string(options.waitForCommit));

    return network::sendRequest(_pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Delete, path, {}, opts)
        .thenValue([](network::Response&& resp) -> LogIndex {
          return processLogIndexResponse(std::move(resp));
        });
  }

  [[nodiscard]] auto remove(LogId id, std::vector<std::string> keys,
                            PrototypeWriteOptions options) const
      -> futures::Future<LogIndex> override {
    auto path = basics::StringUtils::joinT("/", "_api/prototype-state", id,
                                           "multi-remove");
    network::RequestOptions opts;
    opts.database = _vocbase.name();
    opts.param("waitForApplied", std::to_string(options.waitForApplied));
    opts.param("waitForSync", std::to_string(options.waitForSync));
    opts.param("waitForCommit", std::to_string(options.waitForCommit));

    VPackBuilder builder{};
    {
      VPackArrayBuilder ab{&builder};
      for (auto const& key : keys) {
        builder.add(VPackValue(key));
      }
    }
    return network::sendRequest(_pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Delete, path,
                                builder.bufferRef(), opts)
        .thenValue([](network::Response&& resp) -> LogIndex {
          return processLogIndexResponse(std::move(resp));
        });
  }

  auto drop(LogId id) const -> futures::Future<Result> override {
    auto methods = replication2::ReplicatedLogMethods::createInstance(_vocbase);
    return methods->deleteReplicatedLog(id);
  }

  auto waitForApplied(LogId id, LogIndex waitForIndex) const
      -> futures::Future<Result> override {
    auto path = basics::StringUtils::joinT("/", "_api/prototype-state", id,
                                           "wait-for-applied", waitForIndex);
    network::RequestOptions opts;
    opts.database = _vocbase.name();

    return network::sendRequest(_pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue([](network::Response&& resp) -> Result {
          return resp.combinedResult();
        });
  }

  auto status(LogId id) const
      -> futures::Future<ResultT<PrototypeStatus>> override {
    auto path = basics::StringUtils::joinT("/", "_api/prototype-state", id);
    network::RequestOptions opts;
    opts.database = _vocbase.name();

    return network::sendRequest(_pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue([](network::Response&& resp) -> ResultT<PrototypeStatus> {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          } else {
            return velocypack::deserialize<PrototypeStatus>(
                resp.slice().get("result"));
          }
        });
  }

  auto createState(CreateOptions options) const
      -> futures::Future<ResultT<CreateResult>> override {
    using LogMethods = replication2::ReplicatedLogMethods;
    auto methods = LogMethods::createInstance(_vocbase);

    auto forwardOptions = replication2::ReplicatedLogMethods::CreateOptions{};
    forwardOptions.waitForReady = options.waitForReady;
    forwardOptions.config = options.config;
    forwardOptions.id = options.id;
    forwardOptions.servers = options.servers;
    forwardOptions.numberOfServers = options.numberOfServers;
    forwardOptions.spec.type = "prototype";

    return methods->createReplicatedLog(std::move(forwardOptions))
        .thenValue([options = std::move(options), methods,
                    self = shared_from_this()](
                       ResultT<LogMethods::CreateResult>&& result) mutable
                   -> futures::Future<ResultT<CreateResult>> {
          if (result.fail()) {
            return result.result();
          }
          auto& res = result.get();
          return CreateResult{.id = res.id, .servers = std::move(res.servers)};
        });
  }

  explicit PrototypeStateMethodsCoordinator(TRI_vocbase_t& vocbase)
      : _vocbase(vocbase),
        _clusterInfo(
            vocbase.server().getFeature<ClusterFeature>().clusterInfo()),
        _pool(vocbase.server().getFeature<NetworkFeature>().pool()) {}

 private:
  [[nodiscard]] auto getLogLeader(LogId id) const -> ServerID {
    auto leader = _clusterInfo.getReplicatedLogLeader(id);
    if (leader.fail()) {
      if (leader.is(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED)) {
        throw ParticipantResignedException(leader.result(), ADB_HERE);
      } else {
        THROW_ARANGO_EXCEPTION(leader.result());
      }
    }
    return *leader;
  }

  static auto processLogIndexResponse(network::Response&& resp) -> LogIndex {
    if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
      auto r = resp.combinedResult();
      r = r.mapError([&](result::Error error) {
        error.appendErrorMessage(" while contacting server " + resp.serverId());
        return error;
      });
      THROW_ARANGO_EXCEPTION(r);
    } else {
      auto slice = resp.slice();
      if (auto result = slice.get("result");
          result.isObject() && result.length() == 1) {
        return result.get("index").extract<LogIndex>();
      }
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          basics::StringUtils::concatT(
              "expected result containing index in leader response: ",
              slice.toJson()));
    }
  }

 public:
  TRI_vocbase_t& _vocbase;
  ClusterInfo& _clusterInfo;
  network::ConnectionPool* _pool;
};

auto PrototypeStateMethods::createInstance(TRI_vocbase_t& vocbase)
    -> std::shared_ptr<PrototypeStateMethods> {
  switch (ServerState::instance()->getRole()) {
    case ServerState::ROLE_COORDINATOR:
      return std::make_shared<PrototypeStateMethodsCoordinator>(vocbase);
    case ServerState::ROLE_DBSERVER:
      return std::make_shared<PrototypeStateMethodsDBServer>(vocbase);
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "api only on available coordinators or dbservers");
  }
}

[[nodiscard]] auto PrototypeStateMethods::get(LogId id, std::string key,
                                              LogIndex waitForApplied) const
    -> futures::Future<ResultT<std::optional<std::string>>> {
  return get(id, std::move(key), {waitForApplied, false, std::nullopt});
}

[[nodiscard]] auto PrototypeStateMethods::get(LogId id,
                                              std::vector<std::string> keys,
                                              LogIndex waitForApplied) const
    -> futures::Future<ResultT<std::unordered_map<std::string, std::string>>> {
  return get(id, std::move(keys), {waitForApplied, false, std::nullopt});
}

[[nodiscard]] auto PrototypeStateMethods::get(
    LogId id, std::string key, ReadOptions const& readOptions) const
    -> futures::Future<ResultT<std::optional<std::string>>> {
  return get(id, std::vector{key}, readOptions)
      .thenValue(
          [key](ResultT<std::unordered_map<std::string, std::string>>&& result)
              -> ResultT<std::optional<std::string>> {
            if (result.ok()) {
              auto& map = result.get();
              if (auto iter = map.find(key); iter != std::end(map)) {
                return {std::move(iter->second)};
              } else {
                return {std::nullopt};
              }
            }
            return result.result();
          });
}
