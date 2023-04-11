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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <Basics/Exceptions.h>
#include <Basics/voc-errors.h>
#include <Futures/Future.h>
#include <absl/strings/str_cat.h>

#include <iterator>
#include <utility>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/AgencyCallback.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "Inspection/VPack.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Replication2/AgencyMethods.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "VocBase/vocbase.h"

#include "Agency/AgencyPaths.h"
#include "Agency/AsyncAgencyComm.h"
#include "Methods.h"
#include "Random/RandomGenerator.h"

#include "Basics/Result.h"
#include "Basics/Result.tpp"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

namespace {
struct ReplicatedLogMethodsDBServer final
    : ReplicatedLogMethods,
      std::enable_shared_from_this<ReplicatedLogMethodsDBServer> {
  explicit ReplicatedLogMethodsDBServer(TRI_vocbase_t& vocbase)
      : vocbase(vocbase) {}
  auto waitForLogReady(LogId id, std::uint64_t version) const
      -> futures::Future<ResultT<consensus::index_t>> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto createReplicatedLog(CreateOptions spec) const
      -> futures::Future<ResultT<CreateResult>> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto getReplicatedLogs() const -> futures::Future<std::unordered_map<
      arangodb::replication2::LogId,
      std::variant<replicated_log::LogStatus, ParticipantsList>>> override {
    auto result = std::unordered_map<
        arangodb::replication2::LogId,
        std::variant<replicated_log::LogStatus, ParticipantsList>>{};
    for (auto& replicatedLog : vocbase.getReplicatedStatesStatus()) {
      result[replicatedLog.first] = std::move(replicatedLog.second);
    }
    return result;
  }

  auto createReplicatedLog(replication2::agency::LogTarget spec) const
      -> futures::Future<Result> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto deleteReplicatedLog(LogId id) const -> futures::Future<Result> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto getLocalStatus(LogId id) const
      -> futures::Future<replication2::replicated_log::LogStatus> override {
    return vocbase.getReplicatedLogById(id)->getParticipant()->getStatus();
  }

  [[noreturn]] auto getGlobalStatus(
      LogId id, replicated_log::GlobalStatus::SpecificationSource) const
      -> futures::Future<replication2::replicated_log::GlobalStatus> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto getStatus(LogId id) const -> futures::Future<GenericLogStatus> override {
    return getLocalStatus(id).thenValue(
        [](LogStatus&& status) { return GenericLogStatus(std::move(status)); });
  }

  auto slice(LogId id, LogIndex start, LogIndex stop) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto iter = vocbase.getReplicatedLogById(id)
                    ->getParticipant()
                    ->getInternalLogIterator(LogRange(start, stop));
    return {std::move(iter)};
  }

  auto poll(LogId id, LogIndex index, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto leader = vocbase.getReplicatedLogLeaderById(id);
    return vocbase.getReplicatedLogById(id)
        ->getParticipant()
        ->waitFor(index)
        .thenValue([index, limit, leader = std::move(leader),
                    self = shared_from_this()](
                       auto&&) -> std::unique_ptr<PersistedLogIterator> {
          return leader->getInternalLogIterator(LogRange(index, index + limit));
        });
  }

  auto tail(LogId id, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto participant = vocbase.getReplicatedLogById(id)->getParticipant();
    auto status = participant->getQuickStatus();
    auto logStats = status.local.value();
    auto stop = logStats.spearHead.index + 1;
    auto start = stop.saturatedDecrement(limit);
    return participant->getInternalLogIterator(LogRange(start, stop));
  }

  auto head(LogId id, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto participant = vocbase.getReplicatedLogById(id)->getParticipant();
    auto status = participant->getQuickStatus();
    auto logStats = status.local.value();
    auto start = logStats.firstIndex;
    auto bounds = LogRange(start, start + limit);
    return participant->getInternalLogIterator(bounds);
  }

  auto ping(LogId id, std::optional<std::string> message) const
      -> futures::Future<
          std::pair<LogIndex, replicated_log::WaitForResult>> override {
    auto log = vocbase.getReplicatedLogLeaderById(id);
    auto idx = log->ping(std::move(message));
    return log->waitFor(idx).thenValue([idx](auto&& result) {
      return std::make_pair(idx, std::forward<decltype(result)>(result));
    });
  }

  auto insert(LogId id, LogPayload payload, bool waitForSync) const
      -> futures::Future<
          std::pair<LogIndex, replicated_log::WaitForResult>> override {
    auto log = std::dynamic_pointer_cast<replicated_log::LogLeader>(
        vocbase.getReplicatedLogLeaderById(id));
    auto idx = log->insert(std::move(payload), waitForSync);
    return log->waitFor(idx).thenValue([idx](auto&& result) {
      return std::make_pair(idx, std::forward<decltype(result)>(result));
    });
  }

  auto insert(LogId id, TypedLogIterator<LogPayload>& iter,
              bool waitForSync) const
      -> futures::Future<std::pair<std::vector<LogIndex>,
                                   replicated_log::WaitForResult>> override {
    auto log = std::dynamic_pointer_cast<replicated_log::LogLeader>(
        vocbase.getReplicatedLogLeaderById(id));
    auto indexes = std::vector<LogIndex>{};
    while (auto payload = iter.next()) {
      auto idx = log->insert(std::move(*payload));
      indexes.push_back(idx);
    }
    if (indexes.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "multi insert list must not be empty");
    }

    return log->waitFor(indexes.back())
        .thenValue([indexes = std::move(indexes)](auto&& result) mutable {
          return std::make_pair(std::move(indexes),
                                std::forward<decltype(result)>(result));
        });
  }

  auto insertWithoutCommit(LogId id, LogPayload payload, bool waitForSync) const
      -> futures::Future<LogIndex> override {
    auto log = std::dynamic_pointer_cast<replicated_log::LogLeader>(
        vocbase.getReplicatedLogLeaderById(id));
    auto idx = log->insert(std::move(payload), waitForSync);
    return {idx};
  }

  auto compact(LogId id) const
      -> futures::Future<CompactionResultMap> override {
    auto log = vocbase.getReplicatedLogById(id);
    auto result = log->getParticipant()->compact();

    CompactionResultMap map;
    map.emplace(ServerState::instance()->getId(),
                CompactionResponse::fromResult(result));
    return map;
  }

  auto release(LogId id, LogIndex index) const
      -> futures::Future<Result> override {
    auto log = vocbase.getReplicatedLogById(id);
    return log->getParticipant()->release(index);
  }

  auto replaceParticipant(LogId id, ParticipantId const& participantToRemove,
                          ParticipantId const& participantToAdd,
                          std::optional<ParticipantId> const& currentLeader)
      const -> futures::Future<Result> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto setLeader(LogId id, std::optional<ParticipantId> const& leaderId) const
      -> futures::Future<Result> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  TRI_vocbase_t& vocbase;
};

namespace {
struct VPackLogIterator final : PersistedLogIterator {
  explicit VPackLogIterator(
      std::shared_ptr<velocypack::Buffer<uint8_t>> buffer_ptr)
      : buffer(std::move(buffer_ptr)),
        iter(VPackSlice(buffer->data()).get("result")) {}

  auto next() -> std::optional<PersistingLogEntry> override {
    while (iter != std::default_sentinel) {
      return PersistingLogEntry::fromVelocyPack(*iter++);
    }
    return std::nullopt;
  }

 private:
  std::shared_ptr<velocypack::Buffer<uint8_t>> buffer;
  VPackArrayIterator iter;
};

}  // namespace

struct ReplicatedLogMethodsCoordinator final
    : ReplicatedLogMethods,
      std::enable_shared_from_this<ReplicatedLogMethodsCoordinator> {
  auto waitForLogReady(LogId id, std::uint64_t version) const
      -> futures::Future<ResultT<consensus::index_t>> override {
    using namespace cluster::paths;
    auto path = aliases::current()
                    ->replicatedLogs()
                    ->database(vocbaseName)
                    ->log(id)
                    ->supervision();
    return clusterFeature.agencyCallbackRegistry()
        ->waitFor(path->str(SkipComponents(1)),
                  [version](VPackSlice slice) {
                    if (slice.isNone()) {
                      return false;
                    }

                    auto supervision = velocypack::deserialize<
                        replication2::agency::LogCurrentSupervision>(slice);
                    if (supervision.targetVersion >= version) {
                      return true;
                    }
                    return false;
                  })
        .thenValue([](auto index) { return ResultT{index}; });
  }

  void fillCreateOptions(CreateOptions& options) const {
    if (!options.id.has_value()) {
      options.id = LogId{clusterInfo.uniqid()};
    }

    auto dbservers = clusterInfo.getCurrentDBServers();

    auto expectedNumberOfServers = std::min(dbservers.size(), std::size_t{3});
    if (options.numberOfServers.has_value()) {
      expectedNumberOfServers = *options.numberOfServers;
    } else if (!options.servers.empty()) {
      expectedNumberOfServers = options.servers.size();
    }

    if (!options.config.has_value()) {
      options.config = arangodb::replication2::agency::LogTargetConfig{
          2, expectedNumberOfServers, false};
    }

    if (expectedNumberOfServers > dbservers.size()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS);
    }

    // always make sure that the wished leader is part of the set of servers
    if (options.leader) {
      if (auto iter = std::find(options.servers.begin(), options.servers.end(),
                                *options.leader);
          iter == options.servers.end()) {
        options.servers.emplace_back(*options.leader);
      }
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

  static auto createTargetFromCreateOptions(CreateOptions const& options)
      -> replication2::agency::LogTarget {
    replication2::agency::LogTarget target;
    target.id = options.id.value();
    target.config = options.config.value();
    target.leader = options.leader;
    target.properties.implementation = options.spec;
    target.version = 1;
    for (auto const& server : options.servers) {
      target.participants[server];
    }
    return target;
  }

  auto getReplicatedLogs() const -> futures::Future<std::unordered_map<
      arangodb::replication2::LogId,
      std::variant<replicated_log::LogStatus, ParticipantsList>>> override {
    auto logsParticipants =
        clusterInfo.getReplicatedLogsParticipants(vocbaseName);

    if (logsParticipants.fail()) {
      THROW_ARANGO_EXCEPTION(logsParticipants.result());
    }

    auto result = std::unordered_map<
        arangodb::replication2::LogId,
        std::variant<replicated_log::LogStatus, ParticipantsList>>{};
    for (auto& replicatedLog : logsParticipants.get()) {
      result[replicatedLog.first] = std::move(replicatedLog.second);
    }
    return result;
  }

  auto createReplicatedLog(CreateOptions options) const
      -> futures::Future<ResultT<CreateResult>> override {
    fillCreateOptions(options);
    TRI_ASSERT(options.id.has_value());
    auto target = createTargetFromCreateOptions(options);

    return createReplicatedLog(std::move(target))
        .thenValue([options = std::move(options),
                    self = shared_from_this()](auto&& result) mutable
                   -> futures::Future<ResultT<CreateResult>> {
          auto response = CreateResult{*options.id, std::move(options.servers)};
          if (!result.ok()) {
            return {result};
          }

          if (options.waitForReady) {
            // wait for the state to be ready
            return self->waitForLogReady(*options.id, 1)
                .thenValue([self,
                            resp = std::move(response)](auto&& result) mutable
                           -> futures::Future<ResultT<CreateResult>> {
                  if (result.fail()) {
                    return {result.result()};
                  }
                  return self->clusterInfo
                      .fetchAndWaitForPlanVersion(std::chrono::seconds{240})
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

  auto createReplicatedLog(replication2::agency::LogTarget spec) const
      -> futures::Future<Result> override {
    return replication2::agency::methods::createReplicatedLog(vocbaseName, spec)
        .thenValue([self = shared_from_this()](
                       ResultT<uint64_t>&& res) -> futures::Future<Result> {
          if (res.fail()) {
            return futures::Future<Result>{std::in_place, res.result()};
          }

          return self->clusterInfo.waitForPlan(res.get());
        });
  }

  auto deleteReplicatedLog(LogId id) const -> futures::Future<Result> override {
    return replication2::agency::methods::deleteReplicatedLog(vocbaseName, id)
        .thenValue([self = shared_from_this()](
                       ResultT<uint64_t>&& res) -> futures::Future<Result> {
          if (res.fail()) {
            return futures::Future<Result>{std::in_place, res.result()};
          }

          return self->clusterInfo.waitForPlan(res.get());
        });
  }

  [[noreturn]] auto getLocalStatus(LogId id) const
      -> futures::Future<replication2::replicated_log::LogStatus> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto getGlobalStatus(
      LogId id, replicated_log::GlobalStatus::SpecificationSource source) const
      -> futures::Future<replication2::replicated_log::GlobalStatus> override {
    // 1. Determine which source to use for gathering information
    // 2. Query information from all sources
    auto futureSpec = loadLogSpecification(vocbaseName, id, source);
    return std::move(futureSpec)
        .thenValue([self = shared_from_this(), source](
                       ResultT<std::shared_ptr<
                           replication2::agency::LogPlanSpecification const>>
                           result) {
          if (result.fail()) {
            THROW_ARANGO_EXCEPTION(result.result());
          }

          auto const& spec = result.get();
          TRI_ASSERT(spec != nullptr);
          return self->collectGlobalStatusUsingSpec(spec, source);
        });
  }

  auto getStatus(LogId id) const -> futures::Future<GenericLogStatus> override {
    return getGlobalStatus(id, GlobalStatus::SpecificationSource::kRemoteAgency)
        .thenValue([](GlobalStatus&& status) {
          return GenericLogStatus(std::move(status));
        });
  }

  auto slice(LogId id, LogIndex start, LogIndex stop) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "slice");

    network::RequestOptions opts;
    opts.database = vocbaseName;
    opts.parameters["start"] = to_string(start);
    opts.parameters["stop"] = to_string(stop);
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue([](network::Response&& resp)
                       -> std::unique_ptr<PersistedLogIterator> {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }

          return std::make_unique<VPackLogIterator>(
              resp.response().stealPayload());
        });
  }

  auto poll(LogId id, LogIndex index, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "poll");

    network::RequestOptions opts;
    opts.database = vocbaseName;
    opts.parameters["first"] = to_string(index);
    opts.parameters["limit"] = std::to_string(limit);
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue([](network::Response&& resp)
                       -> std::unique_ptr<PersistedLogIterator> {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }

          return std::make_unique<VPackLogIterator>(
              resp.response().stealPayload());
        });
  }

  auto tail(LogId id, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "tail");

    network::RequestOptions opts;
    opts.database = vocbaseName;
    opts.parameters["limit"] = std::to_string(limit);
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue([](network::Response&& resp)
                       -> std::unique_ptr<PersistedLogIterator> {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }

          return std::make_unique<VPackLogIterator>(
              resp.response().stealPayload());
        });
  }

  auto head(LogId id, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "head");

    network::RequestOptions opts;
    opts.database = vocbaseName;
    opts.parameters["limit"] = std::to_string(limit);
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue([](network::Response&& resp)
                       -> std::unique_ptr<PersistedLogIterator> {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }

          return std::make_unique<VPackLogIterator>(
              resp.response().stealPayload());
        });
  }

  auto insert(LogId id, LogPayload payload, bool waitForSync) const
      -> futures::Future<
          std::pair<LogIndex, replicated_log::WaitForResult>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "insert");

    network::RequestOptions opts;
    opts.database = vocbaseName;
    opts.param(StaticStrings::WaitForSyncString,
               waitForSync ? "true" : "false");
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Post, path,
                                payload.copyBuffer(), opts)
        .thenValue([](network::Response&& resp) {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }
          auto result = resp.slice().get("result");
          auto waitResult = result.get("result");

          auto quorum =
              std::make_shared<replication2::replicated_log::QuorumData const>(
                  waitResult.get("quorum"));
          auto commitIndex = waitResult.get("commitIndex").extract<LogIndex>();
          auto index = result.get("index").extract<LogIndex>();
          return std::pair(index, replicated_log::WaitForResult(
                                      commitIndex, std::move(quorum)));
        });
  }

  auto ping(LogId id, std::optional<std::string> message) const
      -> futures::Future<
          std::pair<LogIndex, replicated_log::WaitForResult>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "ping");

    VPackBufferUInt8 payload;
    if (message) {
      VPackBuilder builder(payload);
      VPackObjectBuilder ob(&builder);
      builder.add("message", VPackValue(*message));
    }

    network::RequestOptions opts;
    opts.database = vocbaseName;
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Post, path,
                                std::move(payload), opts)
        .thenValue([](network::Response&& resp) {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }
          auto result = resp.slice().get("result");
          auto waitResult = result.get("result");

          auto quorum =
              std::make_shared<replication2::replicated_log::QuorumData const>(
                  waitResult.get("quorum"));
          auto commitIndex = waitResult.get("commitIndex").extract<LogIndex>();
          auto index = result.get("index").extract<LogIndex>();
          return std::make_pair(index, replicated_log::WaitForResult(
                                           commitIndex, std::move(quorum)));
        });
  }

  auto insert(LogId id, TypedLogIterator<LogPayload>& iter,
              bool waitForSync) const
      -> futures::Future<std::pair<std::vector<LogIndex>,
                                   replicated_log::WaitForResult>> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "multi-insert");

    std::size_t payloadSize{0};
    VPackBuilder builder{};
    {
      VPackArrayBuilder arrayBuilder{&builder};
      while (auto const payload = iter.next()) {
        builder.add(payload->slice());
        ++payloadSize;
      }
    }

    network::RequestOptions opts;
    opts.database = vocbaseName;
    opts.param(StaticStrings::WaitForSyncString,
               waitForSync ? "true" : "false");
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Post, path,
                                builder.bufferRef(), opts)
        .thenValue([payloadSize](network::Response&& resp) {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }
          auto result = resp.slice().get("result");
          auto waitResult = result.get("result");

          auto quorum =
              std::make_shared<replication2::replicated_log::QuorumData const>(
                  waitResult.get("quorum"));
          auto commitIndex = waitResult.get("commitIndex").extract<LogIndex>();

          std::vector<LogIndex> indexes;
          indexes.reserve(payloadSize);
          auto indexIter = velocypack::ArrayIterator(result.get("indexes"));
          for (auto it : indexIter) {
            indexes.push_back(it.template extract<LogIndex>());
          }
          return std::make_pair(
              std::move(indexes),
              replicated_log::WaitForResult(commitIndex, std::move(quorum)));
        });
  }

  auto insertWithoutCommit(LogId id, LogPayload payload, bool waitForSync) const
      -> futures::Future<LogIndex> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "insert");

    network::RequestOptions opts;
    opts.database = vocbaseName;
    opts.param(StaticStrings::WaitForSyncString,
               waitForSync ? "true" : "false");
    opts.param(StaticStrings::DontWaitForCommit, "true");
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Post, path,
                                payload.copyBuffer(), opts)
        .thenValue([](network::Response&& resp) {
          if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
            THROW_ARANGO_EXCEPTION(resp.combinedResult());
          }
          auto result = resp.slice().get("result");
          auto index = result.get("index").extract<LogIndex>();
          return index;
        });
  }

  auto release(LogId id, LogIndex index) const
      -> futures::Future<Result> override {
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "release");

    VPackBufferUInt8 body;
    {
      VPackBuilder builder(body);
      builder.add(VPackSlice::emptyObjectSlice());
    }

    network::RequestOptions opts;
    opts.database = vocbaseName;
    opts.parameters["index"] = to_string(index);
    return network::sendRequest(pool, "server:" + getLogLeader(id),
                                fuerte::RestVerb::Post, path, std::move(body),
                                opts)
        .thenValue(
            [](network::Response&& resp) { return resp.combinedResult(); });
  }

  auto replaceParticipant(LogId id, ParticipantId const& participantToRemove,
                          ParticipantId const& participantToAdd,
                          std::optional<ParticipantId> const& currentLeader)
      const -> futures::Future<Result> override {
    return replication2::agency::methods::replaceReplicatedStateParticipant(
        vocbaseName, id, participantToRemove, participantToAdd, currentLeader);
  }

  auto setLeader(LogId id, std::optional<ParticipantId> const& leaderId) const
      -> futures::Future<Result> override {
    return replication2::agency::methods::replaceReplicatedSetLeader(
        vocbaseName, id, leaderId);
  }

  auto compact(LogId id) const
      -> futures::Future<CompactionResultMap> override {
    auto spec = clusterInfo.getReplicatedLogPlanSpecification(id).get();

    using ResultPair = std::pair<ParticipantId, CompactionResponse>;
    auto const compactParticipant =
        [&](ParticipantId const& participant) -> futures::Future<ResultPair> {
      auto path = basics::StringUtils::joinT("/", "_api/log", id, "compact");
      network::RequestOptions opts;
      opts.database = vocbaseName;
      opts.timeout = std::chrono::seconds{5};
      VPackBufferUInt8 buffer;
      {
        VPackBuilder builder(buffer);
        builder.add(VPackSlice::emptyObjectSlice());
      }
      return network::sendRequest(pool, "server:" + participant,
                                  fuerte::RestVerb::Post, path, buffer, opts)
          .thenValue(
              [participant](network::Response&& resp) noexcept -> ResultPair {
                auto result = resp.deserialize<CompactionResultMap>();
                if (result.fail()) {
                  return {participant,
                          CompactionResponse::fromResult(result.result())};
                }
                TRI_ASSERT(result->contains(participant));
                TRI_ASSERT(result->size() == 1);
                return {participant, result->at(participant)};
              });
    };

    std::vector<futures::Future<ResultPair>> futs;
    for (auto const& [participant, p] : spec->participantsConfig.participants) {
      futs.emplace_back(compactParticipant(participant));
    }

    return futures::collectAll(std::move(futs))
        .thenValue([](std::vector<futures::Try<ResultPair>>&& results)
                       -> CompactionResultMap {
          CompactionResultMap map;
          for (auto const& tryRes : results) {
            auto const& [p, compactionResponse] = tryRes.get();
            map[p] = compactionResponse;
          }
          return map;
        });
  }

  explicit ReplicatedLogMethodsCoordinator(DatabaseID vocbase,
                                           ArangodServer& server)
      : vocbaseName(std::move(vocbase)),
        clusterFeature(server.getFeature<ClusterFeature>()),
        clusterInfo(clusterFeature.clusterInfo()),
        pool(server.getFeature<NetworkFeature>().pool()) {}

 private:
  auto getLogLeader(LogId id) const -> ServerID {
    auto leader = clusterInfo.getReplicatedLogLeader(id);
    if (leader.fail()) {
      if (leader.is(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED)) {
        throw ParticipantResignedException(leader.result(), ADB_HERE);
      } else {
        THROW_ARANGO_EXCEPTION(leader.result());
      }
    }

    return *leader;
  }

  auto loadLogSpecification(DatabaseID const& database, replication2::LogId id,
                            GlobalStatus::SpecificationSource source) const
      -> futures::Future<ResultT<std::shared_ptr<
          arangodb::replication2::agency::LogPlanSpecification const>>> {
    if (source == GlobalStatus::SpecificationSource::kLocalCache) {
      return clusterInfo.getReplicatedLogPlanSpecification(id);
    } else {
      AsyncAgencyComm ac;
      auto f = ac.getValues(arangodb::cluster::paths::aliases::plan()
                                ->replicatedLogs()
                                ->database(database)
                                ->log(id),
                            std::chrono::seconds{5});

      return std::move(f).then(
          [self = shared_from_this(),
           id](futures::Try<AgencyReadResult>&& tryResult)
              -> ResultT<std::shared_ptr<
                  arangodb::replication2::agency::LogPlanSpecification const>> {
            auto result = basics::catchToResultT(
                [&] { return std::move(tryResult.get()); });

            if (result.fail()) {
              return result.result();
            }

            if (result->value().isNone()) {
              return Result::fmt(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND,
                                 id);
            }

            auto spec = velocypack::deserialize<
                arangodb::replication2::agency::LogPlanSpecification>(
                result->value());

            return {std::make_shared<
                arangodb::replication2::agency::LogPlanSpecification>(
                std::move(spec))};
          });
    }
  }

  auto readSupervisionStatus(replication2::LogId id) const
      -> futures::Future<GlobalStatus::SupervisionStatus> {
    AsyncAgencyComm ac;
    using Status = GlobalStatus::SupervisionStatus;
    // TODO move this into the agency methods
    auto f = ac.getValues(arangodb::cluster::paths::aliases::current()
                              ->replicatedLogs()
                              ->database(vocbaseName)
                              ->log(id)
                              ->supervision(),
                          std::chrono::seconds{5});
    return std::move(f).then([self = shared_from_this()](
                                 futures::Try<AgencyReadResult>&& tryResult) {
      auto result =
          basics::catchToResultT([&] { return std::move(tryResult.get()); });

      auto const statusFromResult = [](Result const& res) {
        return Status{
            .connection = {.error = res.errorNumber(),
                           .errorMessage = std::string{res.errorMessage()}},
            .response = std::nullopt};
      };

      if (result.fail()) {
        return statusFromResult(result.result());
      }
      auto& read = result.get();
      auto status = statusFromResult(read.asResult());
      if (read.ok() && !read.value().isNone()) {
        status.response.emplace(
            velocypack::deserialize<
                arangodb::replication2::agency::LogCurrentSupervision>(
                read.value()));
      }

      return status;
    });
  }

  auto queryParticipantsStatus(replication2::LogId id,
                               replication2::ParticipantId const& participant)
      const -> futures::Future<GlobalStatus::ParticipantStatus> {
    using Status = GlobalStatus::ParticipantStatus;
    auto path = basics::StringUtils::joinT("/", "_api/log", id, "local-status");
    network::RequestOptions opts;
    opts.database = vocbaseName;
    opts.timeout = std::chrono::seconds{5};
    return network::sendRequest(pool, "server:" + participant,
                                fuerte::RestVerb::Get, path, {}, opts)
        .then([](futures::Try<network::Response>&& tryResult) mutable {
          auto result = basics::catchToResultT(
              [&] { return std::move(tryResult.get()); });

          auto const statusFromResult = [](Result const& res) {
            return Status{
                .connection = {.error = res.errorNumber(),
                               .errorMessage = std::string{res.errorMessage()}},
                .response = std::nullopt};
          };

          if (result.fail()) {
            return statusFromResult(result.result());
          }

          auto& response = result.get();
          auto status = statusFromResult(response.combinedResult());
          if (response.combinedResult().ok()) {
            status.response = GlobalStatus::ParticipantStatus::Response{
                .value =
                    replication2::replicated_log::LogStatus::fromVelocyPack(
                        response.slice().get("result"))};
          }
          return status;
        });
  }

  auto collectGlobalStatusUsingSpec(
      std::shared_ptr<replication2::agency::LogPlanSpecification const> spec,
      GlobalStatus::SpecificationSource source) const
      -> futures::Future<GlobalStatus> {
    // send of a request to all participants

    auto psf = std::invoke([&] {
      auto const& participants = spec->participantsConfig.participants;
      std::vector<futures::Future<GlobalStatus::ParticipantStatus>> pfs;
      pfs.reserve(participants.size());
      for (auto const& [id, flags] : participants) {
        pfs.emplace_back(queryParticipantsStatus(spec->id, id));
      }
      return futures::collectAll(pfs);
    });
    auto af = readSupervisionStatus(spec->id);
    return futures::collect(std::move(af), std::move(psf))
        .thenValue([spec, source](auto&& pairResult) {
          auto& [agency, participantResults] = pairResult;

          auto leader = std::optional<ParticipantId>{};
          if (spec->currentTerm && spec->currentTerm->leader) {
            leader = spec->currentTerm->leader->serverId;
          }
          auto participantsMap =
              std::unordered_map<ParticipantId,
                                 GlobalStatus::ParticipantStatus>{};

          auto const& participants = spec->participantsConfig.participants;
          std::size_t idx = 0;
          for (auto const& [id, flags] : participants) {
            auto& result = participantResults.at(idx++);
            participantsMap[id] = std::move(result.get());
          }

          return GlobalStatus{
              .supervision = agency,
              .participants = participantsMap,
              .specification = {.source = source, .plan = *spec},
              .leaderId = std::move(leader)};
        });
  }

  DatabaseID vocbaseName;
  ClusterFeature& clusterFeature;
  ClusterInfo& clusterInfo;
  network::ConnectionPool* pool;
};

}  // namespace

auto ReplicatedLogMethods::createInstance(TRI_vocbase_t& vocbase)
    -> std::shared_ptr<ReplicatedLogMethods> {
  switch (ServerState::instance()->getRole()) {
    case ServerState::ROLE_COORDINATOR:
      return std::make_shared<ReplicatedLogMethodsCoordinator>(
          vocbase.name(), vocbase.server());
    case ServerState::ROLE_DBSERVER:
      return std::make_shared<ReplicatedLogMethodsDBServer>(vocbase);
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "api only on available coordinators or dbservers");
  }
}

auto ReplicatedLogMethods::createInstance(DatabaseID database,
                                          ArangodServer& server)
    -> std::shared_ptr<ReplicatedLogMethods> {
  switch (ServerState::instance()->getRole()) {
    case ServerState::ROLE_COORDINATOR:
      return std::make_shared<ReplicatedLogMethodsCoordinator>(database,
                                                               server);
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "api only on available coordinators or dbservers");
  }
}
