//////////////////////////////////////////////////////////////////////////////
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include <Basics/ResultT.h>
#include <Basics/Exceptions.h>
#include <Basics/voc-errors.h>
#include <Futures/Future.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/Methods.h"
#include "Network/Methods.h"
#include "VocBase/vocbase.h"
#include "Random/RandomGenerator.h"

#include "PrototypeFollowerState.h"
#include "PrototypeLeaderState.h"
#include "PrototypeStateMachine.h"
#include "PrototypeStateMethods.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Inspection/VPack.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;

struct PrototypeStateMethodsDBServer final : PrototypeStateMethods {
  explicit PrototypeStateMethodsDBServer(TRI_vocbase_t& vocbase)
      : _vocbase(vocbase) {}

  [[nodiscard]] auto insert(
      LogId id, std::unordered_map<std::string, std::string> const& entries,
      PrototypeWriteOptions options) const
      -> futures::Future<LogIndex> override {
    auto leader = getPrototypeStateLeaderById(id);
    return leader->set(entries, options);
  };

  [[nodiscard]] auto get(LogId id, std::string key,
                         LogIndex waitForApplied) const
      -> futures::Future<ResultT<std::optional<std::string>>> override {
    auto stateMachine =
        std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
            _vocbase.getReplicatedStateById(id));
    if (stateMachine == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, basics::StringUtils::concatT(
                                  "Failed to get ProtoypeState with id ", id));
    }

    auto leader = stateMachine->getLeader();
    if (leader != nullptr) {
      return leader->get(std::move(key), waitForApplied);
    }
    auto follower = stateMachine->getFollower();
    if (follower != nullptr) {
      return follower->get(std::move(key), waitForApplied);
    }

    THROW_ARANGO_EXCEPTION(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_PARTICIPANT_GONE);
  }

  [[nodiscard]] auto get(LogId id, std::vector<std::string> keys,
                         LogIndex waitForApplied) const
      -> futures::Future<
          ResultT<std::unordered_map<std::string, std::string>>> override {
    auto stateMachine =
        std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
            _vocbase.getReplicatedStateById(id));
    if (stateMachine == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, basics::StringUtils::concatT(
                                  "Failed to get ProtoypeState with id ", id));
    }

    auto leader = stateMachine->getLeader();
    if (leader != nullptr) {
      return leader->get(std::move(keys), waitForApplied);
    }
    auto follower = stateMachine->getFollower();
    if (follower != nullptr) {
      return follower->get(std::move(keys), waitForApplied);
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

 private:
  [[nodiscard]] auto getPrototypeStateLeaderById(LogId id) const
      -> std::shared_ptr<PrototypeLeaderState> {
    auto stateMachine =
        std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
            _vocbase.getReplicatedStateById(id));
    if (stateMachine == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND,
          basics::StringUtils::concatT("Failed to get ProtoypeState with id ",
                                       id));
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

  [[nodiscard]] auto get(LogId id, std::string key, LogIndex waitForIndex) const
      -> futures::Future<ResultT<std::optional<std::string>>> override {
    auto path = basics::StringUtils::joinT("/", "_api/prototype-state", id,
                                           "entry", key);
    network::RequestOptions opts;
    opts.database = _vocbase.name();
    opts.param("waitForIndex", std::to_string(waitForIndex.value));

    return network::sendRequest(_pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue([](network::Response&& resp)
                       -> ResultT<std::optional<std::string>> {
          if (resp.statusCode() == fuerte::StatusNotFound) {
            return {std::nullopt};
          } else if (resp.fail() ||
                     !fuerte::statusIsSuccess(resp.statusCode())) {
            auto r = resp.combinedResult();
            r = r.mapError([&](result::Error error) {
              error.appendErrorMessage(" while contacting server " +
                                       resp.serverId());
              return error;
            });
            THROW_ARANGO_EXCEPTION(r);
          } else {
            auto slice = resp.slice();
            if (auto result = slice.get("result");
                result.isObject() && result.length() == 1) {
              return {result.valueAt(0).copyString()};
            }
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_INTERNAL, basics::StringUtils::concatT(
                                        "expected result containing key-value "
                                        "pair in leader response: ",
                                        slice.toJson()));
          }
        });
  }

  [[nodiscard]] auto get(LogId id, std::vector<std::string> keys,
                         LogIndex waitForIndex) const
      -> futures::Future<
          ResultT<std::unordered_map<std::string, std::string>>> override {
    auto path = basics::StringUtils::joinT("/", "_api/prototype-state", id,
                                           "multi-get");
    network::RequestOptions opts;
    opts.database = _vocbase.name();
    opts.param("waitForIndex", std::to_string(waitForIndex.value));

    VPackBuilder builder{};
    {
      VPackArrayBuilder ab{&builder};
      for (auto const& key : keys) {
        builder.add(VPackValue(key));
      }
    }

    return network::sendRequest(_pool, "server:" + getLogLeader(id),
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
                THROW_ARANGO_EXCEPTION(r);
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

  void fillCreateOptions(CreateOptions& options) const {
    if (!options.id.has_value()) {
      options.id = LogId{_clusterInfo.uniqid()};
    }

    auto dbservers = _clusterInfo.getCurrentDBServers();
    std::size_t expectedNumberOfServers =
        std::min(dbservers.size(), std::size_t{3});
    if (options.config.has_value()) {
      expectedNumberOfServers = options.config->replicationFactor;
    } else if (!options.servers.empty()) {
      expectedNumberOfServers = options.servers.size();
    }

    if (!options.config.has_value()) {
      options.config =
          LogConfig{2, expectedNumberOfServers, expectedNumberOfServers, false};
    }

    if (expectedNumberOfServers > dbservers.size()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS);
    }

    if (options.servers.size() < expectedNumberOfServers) {
      auto newEnd = dbservers.end();
      if (!options.servers.empty()) {
        newEnd = std::remove_if(
            dbservers.begin(), dbservers.end(),
            [&](ParticipantId const& server) {
              return std::find(options.servers.begin(), options.servers.end(),
                               server) != options.servers.end();
            });
      }

      std::shuffle(dbservers.begin(), newEnd,
                   RandomGenerator::UniformRandomGenerator<std::uint32_t>{});
      std::copy_n(dbservers.begin(),
                  expectedNumberOfServers - options.servers.size(),
                  std::back_inserter(options.servers));
    }
  }

  static auto stateTargetFromCreateOptions(CreateOptions const& opts)
      -> replication2::replicated_state::agency::Target {
    auto target = replicated_state::agency::Target{};
    target.id = opts.id.value();
    target.properties.implementation.type = "prototype";
    target.config = opts.config.value();
    target.version = 1;
    for (auto const& server : opts.servers) {
      target.participants[server];
    }
    return target;
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
    fillCreateOptions(options);
    TRI_ASSERT(options.id.has_value());
    auto target = stateTargetFromCreateOptions(options);
    auto methods =
        replication2::ReplicatedStateMethods::createInstance(_vocbase);

    return methods->createReplicatedState(std::move(target))
        .thenValue([options = std::move(options), methods,
                    self = shared_from_this()](auto&& result) mutable
                   -> futures::Future<ResultT<CreateResult>> {
          auto response = CreateResult{*options.id, std::move(options.servers)};
          if (!result.ok()) {
            return {result};
          }

          if (options.waitForReady) {
            // wait for the state to be ready
            return methods->waitForStateReady(*options.id, 1)
                .thenValue([self,
                            resp = std::move(response)](auto&& result) mutable
                           -> futures::Future<ResultT<CreateResult>> {
                  if (result.fail()) {
                    return {result.result()};
                  }
                  return self->_clusterInfo.waitForPlan(result.get())
                      .thenValue([resp = std::move(resp)](auto&& result) mutable
                                 -> ResultT<CreateResult> {
                        if (result.fail()) {
                          return {result};
                        }
                        return std::move(resp);
                      });
                });
          }
          return response;
        });
  }

  explicit PrototypeStateMethodsCoordinator(TRI_vocbase_t& vocbase)
      : _vocbase(vocbase),
        _clusterInfo(
            vocbase.server().getFeature<ClusterFeature>().clusterInfo()),
        _pool(vocbase.server().getFeature<NetworkFeature>().pool()) {}

 private:
  [[nodiscard]] auto getLogLeader(LogId id) const -> ServerID {
    auto leader = _clusterInfo.getReplicatedLogLeader(_vocbase.name(), id);
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
