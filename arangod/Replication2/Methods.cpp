////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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

#include <Basics/Exceptions.h>
#include <Basics/voc-errors.h>
#include <Futures/Future.h>

#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Network/Methods.h"
#include "Replication2/AgencyMethods.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "VocBase/vocbase.h"

#include "Methods.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

struct ReplicatedLogMethodsDBServer final
    : ReplicatedLogMethods,
      std::enable_shared_from_this<ReplicatedLogMethodsDBServer> {
  explicit ReplicatedLogMethodsDBServer(TRI_vocbase_t& vocbase)
      : vocbase(vocbase) {}
  auto createReplicatedLog(replication2::agency::LogPlanSpecification const& spec) const
      -> futures::Future<Result> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto deleteReplicatedLog(LogId id) const -> futures::Future<Result> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto getReplicatedLogs() const
      -> futures::Future<std::unordered_map<arangodb::replication2::LogId, replicated_log::LogStatus>> override {
    return vocbase.getReplicatedLogs();
  }

  auto getLogStatus(LogId id) const
      -> futures::Future<replication2::replicated_log::LogStatus> override {
    return vocbase.getReplicatedLogById(id)->getParticipant()->getStatus();
  }

  auto getLogEntryByIndex(LogId id, LogIndex index) const
      -> futures::Future<std::optional<PersistingLogEntry>> override {
    return vocbase.getReplicatedLogLeaderById(id)->readReplicatedEntryByIndex(index);
  }

  auto slice(LogId id, LogIndex start, LogIndex stop) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    return vocbase.getReplicatedLogLeaderById(id)->copyInMemoryLog().getInternalIteratorRange(start, stop);
  }

  auto poll(LogId id, LogIndex index, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto leader = vocbase.getReplicatedLogLeaderById(id);
    return vocbase.getReplicatedLogLeaderById(id)->waitFor(index).thenValue(
        [index, limit, leader = std::move(leader),
         self = shared_from_this()](auto&&) -> std::unique_ptr<PersistedLogIterator> {
          auto log = leader->copyInMemoryLog();
          return log.getInternalIteratorRange(index, index + limit);
        });
  }

  auto tail(LogId id, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto log = vocbase.getReplicatedLogLeaderById(id)->copyInMemoryLog();
    auto stop = log.getNextIndex();
    auto start = stop.saturatedDecrement(limit);
    return log.getInternalIteratorRange(start, stop);
  }

  auto head(LogId id, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto log = vocbase.getReplicatedLogLeaderById(id)->copyInMemoryLog();
    auto start = log.getFirstIndex();
    return log.getInternalIteratorRange(start, start + limit);
  }

  auto insert(LogId id, LogPayload payload) const
      -> futures::Future<std::pair<LogIndex, replicated_log::WaitForResult>> override {
    auto log = vocbase.getReplicatedLogLeaderById(id);
    auto idx = log->insert(std::move(payload));
    return log->waitFor(idx).thenValue([idx](auto&& result) {
      return std::make_pair(idx, std::forward<decltype(result)>(result));
    });
  }

  auto insert(LogId id, TypedLogIterator<LogPayload>& iter) const
      -> futures::Future<std::pair<std::vector<LogIndex>, replicated_log::WaitForResult>> override {
    auto log = vocbase.getReplicatedLogLeaderById(id);
    auto indexes = std::vector<LogIndex>{};
    while (auto payload = iter.next()) {
      auto idx = log->insert(std::move(*payload));
      indexes.push_back(idx);
    }
    if (indexes.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "multi insert list must not be empty");
    }

    return log->waitFor(indexes.back()).thenValue([indexes = std::move(indexes)](auto&& result) mutable {
      return std::make_pair(std::move(indexes), std::forward<decltype(result)>(result));
    });
  }

  auto release(LogId id, LogIndex index) const -> futures::Future<Result> override {
    auto log = vocbase.getReplicatedLogById(id);
    return log->getParticipant()->release(index);
  }
  TRI_vocbase_t& vocbase;
};

namespace {
struct VPackLogIterator final : PersistedLogIterator {
  explicit VPackLogIterator(std::shared_ptr<velocypack::Buffer<uint8_t>> buffer_ptr)
      : buffer(std::move(buffer_ptr)),
        iter(VPackSlice(buffer->data()).get("result")),
        end(iter.end()) {}

  auto next() -> std::optional<PersistingLogEntry> override {
    while (iter != end) {
      return PersistingLogEntry::fromVelocyPack(*iter++);
    }
    return std::nullopt;
  }

 private:
  std::shared_ptr<velocypack::Buffer<uint8_t>> buffer;
  VPackArrayIterator iter;
  VPackArrayIterator end;
};
}  // namespace

struct ReplicatedLogMethodsCoordinator final
    : ReplicatedLogMethods,
      std::enable_shared_from_this<ReplicatedLogMethodsCoordinator> {
  auto createReplicatedLog(replication2::agency::LogPlanSpecification const& spec) const
      -> futures::Future<Result> override {
    return replication2::agency::methods::createReplicatedLog(vocbase.name(), spec)
        .thenValue([self = shared_from_this()](ResultT<uint64_t>&& res) -> futures::Future<Result> {
          if (res.fail()) {
            return futures::Future<Result>{std::in_place, res.result()};
          }

          return self->clusterInfo.waitForPlan(res.get());
        });
  }

  auto deleteReplicatedLog(LogId id) const -> futures::Future<Result> override {
    return replication2::agency::methods::deleteReplicatedLog(vocbase.name(), id)
        .thenValue([self = shared_from_this()](ResultT<uint64_t>&& res) -> futures::Future<Result> {
          if (res.fail()) {
            return futures::Future<Result>{std::in_place, res.result()};
          }

          return self->clusterInfo.waitForPlan(res.get());
        });
  }

  auto getReplicatedLogs() const
      -> futures::Future<std::unordered_map<arangodb::replication2::LogId, replicated_log::LogStatus>> override {
    return vocbase.getReplicatedLogs();
  }

  auto getLogStatus(LogId id) const
      -> futures::Future<replication2::replicated_log::LogStatus> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id);
    network::RequestOptions opts;
    opts.database = vocbase.name();
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path)
        .thenValue([](network::Response&& resp) {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }

          return replication2::replicated_log::LogStatus::fromVelocyPack(
              resp.slice().get("result"));
        });
  }

  auto getLogEntryByIndex(LogId id, LogIndex index) const
      -> futures::Future<std::optional<PersistingLogEntry>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "entry", index.value);
    network::RequestOptions opts;
    opts.database = vocbase.name();
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue([](network::Response&& resp) {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }
          auto entry =
              PersistingLogEntry::fromVelocyPack(resp.slice().get("result"));
          return std::optional<PersistingLogEntry>(std::move(entry));
        });
  }

  auto slice(LogId id, LogIndex start, LogIndex stop) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "slice");

    network::RequestOptions opts;
    opts.database = vocbase.name();
    opts.parameters["start"] = to_string(start);
    opts.parameters["stop"] = to_string(stop);
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue([](network::Response&& resp) -> std::unique_ptr<PersistedLogIterator> {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }

          return std::make_unique<VPackLogIterator>(resp.response().stealPayload());
        });
  }

  auto poll(LogId id, LogIndex index, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "poll");

    network::RequestOptions opts;
    opts.database = vocbase.name();
    opts.parameters["first"] = to_string(index);
    opts.parameters["limit"] = std::to_string(limit);
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue([](network::Response&& resp) -> std::unique_ptr<PersistedLogIterator> {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }

          return std::make_unique<VPackLogIterator>(resp.response().stealPayload());
        });
  }

  auto tail(LogId id, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "tail");

    network::RequestOptions opts;
    opts.database = vocbase.name();
    opts.parameters["limit"] = std::to_string(limit);
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue([](network::Response&& resp) -> std::unique_ptr<PersistedLogIterator> {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }

          return std::make_unique<VPackLogIterator>(resp.response().stealPayload());
        });
  }

  auto head(LogId id, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "head");

    network::RequestOptions opts;
    opts.database = vocbase.name();
    opts.parameters["limit"] = std::to_string(limit);
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue([](network::Response&& resp) -> std::unique_ptr<PersistedLogIterator> {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }

          return std::make_unique<VPackLogIterator>(resp.response().stealPayload());
        });
  }

  auto insert(LogId id, LogPayload payload) const
      -> futures::Future<std::pair<LogIndex, replicated_log::WaitForResult>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "insert");

    network::RequestOptions opts;
    opts.database = vocbase.name();
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Post, path, payload.dummy, opts)
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

  auto insert(LogId id, TypedLogIterator<LogPayload>& iter) const
      -> futures::Future<std::pair<std::vector<LogIndex>, replicated_log::WaitForResult>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "multi-insert");

    std::size_t payloadSize{0};
    VPackBuilder builder{};
    {
      VPackArrayBuilder arrayBuilder{&builder};
      while (const auto payload = iter.next()) {
        if (payload.has_value()) {
          builder.add(payload->slice());
        } else {
          builder.add(VPackSlice::emptyObjectSlice());
        }
        ++payloadSize;
      }
    }

    network::RequestOptions opts;
    opts.database = vocbase.name();
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Post, path, builder.bufferRef(), opts)
        .thenValue([payloadSize](network::Response&& resp) {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }
          auto result = resp.slice().get("result");
          auto waitResult = result.get("result");

          auto quorum = std::make_shared<replication2::replicated_log::QuorumData const>(
              waitResult.get("quorum"));
          auto commitIndex = waitResult.get("commitIndex").extract<LogIndex>();

          std::vector<LogIndex> indexes;
          indexes.reserve(payloadSize);
          auto indexIter = velocypack::ArrayIterator(result.get("indexes"));
          std::transform(indexIter.begin(), indexIter.end(),
                        std::back_inserter(indexes), [](auto const& it) {
                           return it.template extract<LogIndex>();
                         });
          return std::make_pair(
              std::move(indexes),
              replicated_log::WaitForResult(commitIndex, std::move(quorum)));
        });
  }

  auto release(LogId id, LogIndex index) const -> futures::Future<Result> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "release");

    VPackBufferUInt8 body;
    {
      VPackBuilder builder(body);
      builder.add(VPackSlice::emptyObjectSlice());
    }

    network::RequestOptions opts;
    opts.database = vocbase.name();
    opts.parameters["index"] = to_string(index);
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Post, path, std::move(body), opts)
        .thenValue([](network::Response&& resp) { return resp.combinedResult(); });
  }

  explicit ReplicatedLogMethodsCoordinator(TRI_vocbase_t& vocbase)
      : vocbase(vocbase),
        clusterInfo(vocbase.server().getFeature<ClusterFeature>().clusterInfo()),
        pool(vocbase.server().getFeature<NetworkFeature>().pool()) {}

 private:
  auto getLogLeader(LogId id) const -> ServerID {
    auto leader = clusterInfo.getReplicatedLogLeader(vocbase.name(), id);
    if (leader.fail()) {
      THROW_ARANGO_EXCEPTION(leader.result());
    }

    return *leader;
  }

  TRI_vocbase_t& vocbase;
  ClusterInfo& clusterInfo;
  network::ConnectionPool* pool;
};

auto ReplicatedLogMethods::createInstance(TRI_vocbase_t& vocbase)
    -> std::shared_ptr<ReplicatedLogMethods> {
  switch (ServerState::instance()->getRole()) {
    case ServerState::ROLE_COORDINATOR:
      return std::make_shared<ReplicatedLogMethodsCoordinator>(vocbase);
    case ServerState::ROLE_DBSERVER:
      return std::make_shared<ReplicatedLogMethodsDBServer>(vocbase);
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "api only on available coordinators or dbservers");
  }
}
