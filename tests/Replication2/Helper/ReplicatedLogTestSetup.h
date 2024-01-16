////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Replication2/Mocks/DelayedLogFollower.h"
#include "Replication2/Mocks/FakeAbstractFollower.h"
#include "Replication2/Mocks/FakeAsyncExecutor.h"
#include "Replication2/Mocks/FakeFollowerFactory.h"
#include "Replication2/Mocks/FakeReplicatedState.h"
#include "Replication2/Mocks/FakeStorageEngineMethods.h"
#include "Replication2/Mocks/RebootIdCacheMock.h"
#include "Replication2/Mocks/ReplicatedLogMetricsMock.h"
#include "Replication2/Mocks/ReplicatedStateHandleMock.h"
#include "Replication2/Mocks/ReplicatedStateMetricsMock.h"
#include "Replication2/Mocks/SchedulerMocks.h"

#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedState/StateCommon.h"
#include "Futures/Utilities.h"

// TODO Remove this conditional as soon as we've upgraded MSVC.
#ifdef DISABLE_I_HAS_SCHEDULER
#pragma message(                                                   \
    "Warning: Not compiling this file due to a compiler bug, see " \
    "IHasScheduler.h for details.")
#else

namespace arangodb::replication2::test {

auto inline operator"" _Lx(unsigned long long x) -> LogIndex {
  return LogIndex{x};
}
auto inline operator"" _T(unsigned long long x) -> LogTerm {
  return LogTerm{x};
}

struct LogArguments {
  std::variant<LogRange, std::vector<LogPayload>> initialLogRange = {};
  // set persistedMetadata to std::nullopt to simulate a read failure.
  // `.stateId` will be set by createParticipant(), but is of no consequence
  // anyway.
  std::optional<storage::PersistedStateInfo> persistedMetadata =
      storage::PersistedStateInfo{
          .snapshot = {.status = replicated_state::SnapshotStatus::kCompleted}};

  std::shared_ptr<ReplicatedLogGlobalSettings> options =
      std::make_shared<ReplicatedLogGlobalSettings>();
};
struct ConfigArguments {
  LogTerm term = LogTerm{1};
  std::size_t writeConcern = 1;
  bool waitForSync = false;
};
struct LogContainer;
struct LogConfig {
  // TODO maybe move to shared_ptrs from reference_wrapper
  // TODO Maybe remove leader and followers entirely; the information is
  //      available in termSpec.leader and
  //      participantsConfig.participantsFlagsMap.
  //      We'd need to move the "installConfig" function somewhere else though
  //      (to WholeLog, probably).
  std::optional<std::reference_wrapper<LogContainer>> leader;
  std::vector<std::reference_wrapper<LogContainer>> followers;
  agency::LogPlanTermSpecification termSpec;
  agency::ParticipantsConfig participantsConfig;

  void installConfig(bool establishLeadership = false);

  static auto makeConfig(
      std::optional<std::reference_wrapper<LogContainer>> leader,
      std::vector<std::reference_wrapper<LogContainer>> follower,
      ConfigArguments configArguments) -> LogConfig;
};

struct ITestParticipant : IHasScheduler {
  virtual ~ITestParticipant() = default;
  virtual auto serverInstance() const -> agency::ServerInstanceReference = 0;
  auto serverId() const -> ParticipantId { return serverInstance().serverId; }
};

struct ParticipantWithFakes : ITestParticipant {
  ParticipantWithFakes(
      LogId logId, agency::ServerInstanceReference serverId,
      LoggerContext loggerContext,
      std::shared_ptr<replicated_log::ReplicatedLogMetrics> logMetrics,
      LogArguments fakeArguments)
      : loggerContext(std::move(loggerContext)),
        logId(logId),
        _serverInstance(std::move(serverId)),
        logMetrics(std::move(logMetrics)),
        storageContext(
            std::make_shared<storage::test::FakeStorageEngineMethodsContext>(
                12, gid.id, storageExecutor, fakeArguments.initialLogRange,
                std::move(fakeArguments.persistedMetadata))),
        options(std::move(fakeArguments.options)) {}

  [[nodiscard]] auto operator==(
      ParticipantWithFakes const& other) const noexcept -> bool {
    // suffices for now, we could conceivably also compare logId +
    // serverInstance instead
    return this == &other;
  }

  [[nodiscard]] auto updateConfig(LogConfig const* conf)
      -> futures::Future<futures::Unit>;
  [[nodiscard]] auto getAsLeader() {
    auto leader = std::dynamic_pointer_cast<replicated_log::ILogLeader>(
        log->getParticipant());
    EXPECT_NE(leader, nullptr);
    return leader;
  }
  [[nodiscard]] auto getAsFollower()
      -> std::shared_ptr<replicated_log::ILogFollower> {
    auto follower = std::dynamic_pointer_cast<replicated_log::ILogFollower>(
        log->getParticipant());
    EXPECT_NE(follower, nullptr);
    return follower;
  }

  [[nodiscard]] auto getAbstractFollower()
      -> std::shared_ptr<replicated_log::AbstractFollower> {
    return delayedLogFollower;
  }
  // Calls insert from the state machine's perspective. Only works on the leader
  // (obviously), and only after leadership has been established.
  [[nodiscard]] auto insert(LogPayload payload, bool waitForSync = false) {
    return stateHandleMock->logLeaderMethods->insert(std::move(payload),
                                                     waitForSync);
  }
  [[nodiscard]] auto getQuickStatus() {
    return log->getParticipant()->getQuickStatus();
  }

  auto runAll() noexcept -> std::size_t override {
    if (delayedLogFollower != nullptr) {
      return IHasScheduler::runAll(logScheduler, storageExecutor,
                                   delayedLogFollower->scheduler);
    } else {
      return IHasScheduler::runAll(logScheduler, storageExecutor);
    }
  }
  auto hasWork() const noexcept -> bool override {
    if (delayedLogFollower == nullptr) {
      return IHasScheduler::hasWork(logScheduler, storageExecutor);
    } else {
      return IHasScheduler::hasWork(logScheduler, storageExecutor,
                                    delayedLogFollower->scheduler);
    }
  }
  auto stealFollower() {
    auto scheduler = std::make_shared<DelayedScheduler>();
    auto follower = std::make_shared<DelayedLogFollower>(serverId());
    std::swap(scheduler, logScheduler);
    std::swap(follower, delayedLogFollower);
    return std::pair(std::move(follower), std::move(scheduler));
  }

  auto serverInstance() const -> agency::ServerInstanceReference override {
    return _serverInstance;
  }

  LoggerContext loggerContext;
  LogId const logId;
  agency::ServerInstanceReference const _serverInstance;
  std::shared_ptr<replicated_log::ReplicatedLogMetrics> logMetrics;

  GlobalLogIdentifier gid = GlobalLogIdentifier("db", logId);

  std::shared_ptr<storage::rocksdb::test::DelayedExecutor> storageExecutor =
      std::make_shared<storage::rocksdb::test::DelayedExecutor>();
  // Note that this purposefully does not initialize the PersistedStateInfo that
  // is returned by the StorageEngineMethods. readMetadata() will return a
  // document not found error unless you initialize it in your test.
  std::shared_ptr<storage::test::FakeStorageEngineMethodsContext>
      storageContext;
  storage::IStorageEngineMethods* methodsPtr =
      storageContext->getMethods().release();
  std::shared_ptr<ReplicatedLogGlobalSettings> options;

  std::shared_ptr<DelayedScheduler> logScheduler =
      std::make_shared<DelayedScheduler>();
  std::shared_ptr<test::RebootIdCacheMock> rebootIdCache =
      std::make_shared<testing::NiceMock<test::RebootIdCacheMock>>();
  std::shared_ptr<test::FakeFollowerFactory> fakeFollowerFactory =
      std::make_shared<FakeFollowerFactory>();
  std::shared_ptr<replicated_log::DefaultParticipantsFactory>
      participantsFactory =
          std::make_shared<replicated_log::DefaultParticipantsFactory>(
              fakeFollowerFactory, logScheduler, rebootIdCache);

  std::shared_ptr<replicated_log::ReplicatedLog> log =
      std::make_shared<replicated_log::ReplicatedLog>(
          std::unique_ptr<storage::IStorageEngineMethods>{methodsPtr},
          logMetrics, options, participantsFactory, loggerContext,
          _serverInstance);

  test::ReplicatedStateHandleMock* stateHandleMock =
      new testing::NiceMock<test::ReplicatedStateHandleMock>();
  replicated_log::ReplicatedLogConnection connection = log->connect(
      std::unique_ptr<replicated_log::IReplicatedStateHandle>(stateHandleMock));

  std::shared_ptr<DelayedLogFollower> delayedLogFollower =
      std::make_shared<DelayedLogFollower>(_serverInstance.serverId);
};

struct ParticipantFakeFollower : ITestParticipant {
  auto getAbstractFollower()
      -> std::shared_ptr<replicated_log::AbstractFollower> {
    return fakeAbstractFollower;
  }

  explicit ParticipantFakeFollower(
      agency::ServerInstanceReference serverInstanceReference)
      : serverInstanceReference(serverInstanceReference),
        fakeAbstractFollower(std::make_shared<FakeAbstractFollower>(
            serverInstanceReference.serverId)) {}

  auto serverInstance() const -> agency::ServerInstanceReference override {
    return serverInstanceReference;
  }

  [[nodiscard]] auto operator==(
      ParticipantFakeFollower const& other) const noexcept -> bool {
    // suffices for now, we could conceivably also compare logId +
    // serverInstance instead
    return this == &other;
  }

  auto hasWork() const noexcept -> bool override {
    return fakeAbstractFollower->hasWork();
  }
  auto runAll() noexcept -> std::size_t override {
    return fakeAbstractFollower->runAll();
  }

  agency::ServerInstanceReference serverInstanceReference;
  std::shared_ptr<FakeAbstractFollower> fakeAbstractFollower = nullptr;
};

// Holds one replicated log participant, together with all the necessary mocks
// and fakes.
struct LogContainer : IHasScheduler {
  static auto createWithParticipantWithFakes(
      LogId logId, agency::ServerInstanceReference serverId,
      LoggerContext loggerContext,
      std::shared_ptr<replicated_log::ReplicatedLogMetrics> logMetrics,
      LogArguments fakeArguments) -> LogContainer;
  static auto createWithFakeFollower(agency::ServerInstanceReference serverId)
      -> LogContainer;

  template<typename... Ts>
  auto visitLog(Ts&&... ts) -> decltype(auto) {
    return std::visit(overload{std::forward<Ts>(ts)...}, _log);
  }
  template<typename... Ts>
  auto visitLog(Ts const&... ts) const -> decltype(auto) {
    return std::visit(overload{ts...}, _log);
  }

  [[nodiscard]] auto updateConfig(LogConfig const* conf)
      -> futures::Future<futures::Unit> {
    return visitLog(
        [&](ParticipantWithFakes& log) { return log.updateConfig(conf); },
        [](ParticipantFakeFollower& log) { return futures::makeFuture(); });
  }

  [[nodiscard]] auto getAsLeader() {
    return visitLog(
        [&](ParticipantWithFakes& log) { return log.getAsLeader(); },
        [](ParticipantFakeFollower& log)
            -> std::shared_ptr<replicated_log::ILogLeader> { std::abort(); });
  }
  [[nodiscard]] auto getAsFollower()
      -> std::shared_ptr<replicated_log::ILogFollower> {
    return visitLog(
        [&](ParticipantWithFakes& log) { return log.getAsFollower(); },
        [](ParticipantFakeFollower& log) {
          return std::dynamic_pointer_cast<replicated_log::ILogFollower>(
              log.getAbstractFollower());
        });
  }

  // Get an AbstractFollower for the leader to use. Will usually be a
  // DelayedLogFollower, but can also be a FakeAbstractFollower.
  [[nodiscard]] auto getAbstractFollower()
      -> std::shared_ptr<replicated_log::AbstractFollower> {
    return visitLog([&](auto&& log) { return log.getAbstractFollower(); });
  }
  // Calls insert from the state machine's perspective. Only works on the leader
  // (obviously), and only after leadership has been established.
  [[nodiscard]] auto insert(LogPayload payload, bool waitForSync = false) {
    return visitLog(
        [&](ParticipantWithFakes& log) -> LogIndex {
          return log.insert(std::move(payload), waitForSync);
        },
        [&](ParticipantFakeFollower& log) -> LogIndex { std::abort(); });
  }

  auto runAll() noexcept -> std::size_t override {
    return visitLog([](ITestParticipant& log) { return log.runAll(); });
  }
  auto hasWork() const noexcept -> bool override {
    return visitLog([](ITestParticipant const& log) { return log.hasWork(); });
  }
  auto stealFollower() {
    using ReturnType =
        decltype(std::declval<ParticipantWithFakes>().stealFollower());
    return visitLog(
        [](ParticipantWithFakes& log) { return log.stealFollower(); },
        [](ParticipantFakeFollower&) -> ReturnType { std::abort(); });
  }

  auto serverInstance() {
    return visitLog([](ITestParticipant& log) { return log.serverInstance(); });
  }
  auto serverId() { return serverInstance().serverId; }

  [[nodiscard]] auto operator==(LogContainer const& other) const noexcept
      -> bool {
    // suffices for now, we could conceivably also compare logId +
    // serverInstance instead
    return this == &other;
  }
  [[nodiscard]] auto operator==(
      ParticipantWithFakes const& other) const noexcept -> bool {
    return visitLog(
        [&](ParticipantWithFakes const& log) { return log == other; },
        [](ParticipantFakeFollower const&) { return false; });
  }

  template<typename T>
  requires std::is_same_v<T, ParticipantWithFakes> ||
      std::is_same_v<T, ParticipantFakeFollower>
  auto contains() const { return std::holds_alternative<T>(_log); }
  template<typename T>
  requires std::is_same_v<T, ParticipantWithFakes> ||
      std::is_same_v<T, ParticipantFakeFollower>
  [[nodiscard]] auto getAs() -> T& { return std::get<T>(_log); }
  [[nodiscard]] auto getAsParticipant() -> ParticipantWithFakes& {
    return getAs<ParticipantWithFakes>();
  }

  LogContainer(ParticipantWithFakes*, LogId logId,
               agency::ServerInstanceReference serverId,
               LoggerContext loggerContext,
               std::shared_ptr<replicated_log::ReplicatedLogMetrics> logMetrics,
               LogArguments fakeArguments)
      : _log(std::in_place_type<ParticipantWithFakes>, logId,
             std::move(serverId), std::move(loggerContext),
             std::move(logMetrics), std::move(fakeArguments)) {}
  LogContainer(ParticipantFakeFollower*,
               agency::ServerInstanceReference serverId)
      : _log(std::in_place_type<ParticipantFakeFollower>, serverId) {}

  std::variant<ParticipantWithFakes, ParticipantFakeFollower> _log;
};

// One replicated log instance, including all participants, possibly
// going over
// multiple terms.
struct WholeLog {
  LogId const logId;
  // Relying on stable pointers and references to logs for now. Move to
  // shared_ptr<LogContainer> if necessary.
  std::map<ParticipantId, LogContainer> logs;
  // TODO We might need to allow for multiple configs in the same term, or
  //      changing a config mid-term.
  //      Currently, we don't need the whole history, but it doesn't hurt.
  std::map<LogTerm, LogConfig> terms;

  explicit WholeLog(LoggerContext loggerContext, LogId logId)
      : logId(logId), loggerContext(loggerContext) {}

  auto createParticipant(LogArguments fakeArguments) -> ParticipantWithFakes* {
    auto const id = _nextParticipantId++;
    auto serverId = agency::ServerInstanceReference{fmt::format("dbs{:02}", id),
                                                    RebootId{1}};
    if (fakeArguments.persistedMetadata.has_value()) {
      fakeArguments.persistedMetadata->stateId = logId;
    }
    auto it = logs.try_emplace(logs.end(), serverId.serverId,
                               LogContainer::createWithParticipantWithFakes(
                                   logId, serverId, loggerContext,
                                   logMetricsMock, std::move(fakeArguments)));
    return &std::get<ParticipantWithFakes>(it->second._log);
  }

  auto createFakeFollower() -> ParticipantFakeFollower* {
    auto const id = _nextParticipantId++;
    auto serverId = agency::ServerInstanceReference{fmt::format("dbs{:02}", id),
                                                    RebootId{1}};
    auto it = logs.try_emplace(logs.end(), serverId.serverId,
                               LogContainer::createWithFakeFollower(serverId));
    return &std::get<ParticipantFakeFollower>(it->second._log);
  }

  // Create a new term, using the LogTerm from configArguments. It must not yet
  // exist.
  auto addNewTerm(std::optional<std::reference_wrapper<LogContainer>> leader,
                  std::vector<std::reference_wrapper<LogContainer>> follower,
                  ConfigArguments configArguments) -> LogConfig* {
    auto const term = configArguments.term;
    TRI_ASSERT(!terms.contains(term));

    auto it =
        terms.try_emplace(terms.end(), term,
                          LogConfig::makeConfig(leader, std::move(follower),
                                                std::move(configArguments)));
    return &it->second;
  }
  auto addNewTerm(std::optional<ParticipantId> leader,
                  std::vector<ParticipantId> follower,
                  ConfigArguments configArguments) -> LogConfig* {
    auto const term = configArguments.term;
    TRI_ASSERT(!terms.contains(term));

    auto leaderContainer =
        leader ? std::optional<std::reference_wrapper<LogContainer>>{logs.at(
                     *leader)}
               : std::nullopt;
    auto fs = std::vector<std::reference_wrapper<LogContainer>>();
    std::transform(follower.begin(), follower.end(), std::back_inserter(fs),
                   [&](auto& f) { return std::reference_wrapper{logs.at(f)}; });
    auto it =
        terms.try_emplace(terms.end(), term,
                          LogConfig::makeConfig(leaderContainer, std::move(fs),
                                                std::move(configArguments)));
    return &it->second;
  }

  struct ConfigUpdates {
    // TODO Use participant IDs instead of references
    std::optional<std::reference_wrapper<LogContainer>> setLeader;
    std::vector<std::reference_wrapper<LogContainer>> addParticipants;
    std::vector<std::reference_wrapper<LogContainer>> removeParticipants;
    std::optional<std::uint64_t> setWriteConcern;
    std::optional<bool> setWaitForSync;

    auto updateConfig(LogConfig config) const -> LogConfig {
      for (auto const& toRemove : removeParticipants) {
        auto const shouldBeRemoved = [&](auto other) {
          return toRemove.get() == other.get();
        };
        auto it = std::remove_if(config.followers.begin(),
                                 config.followers.end(), shouldBeRemoved);
        config.followers.erase(it, config.followers.end());
        config.participantsConfig.participants.erase(toRemove.get().serverId());
        if (config.leader && shouldBeRemoved(*config.leader)) {
          config.leader = std::nullopt;
          config.termSpec.leader = std::nullopt;
        }
      }
      std::copy(addParticipants.begin(), addParticipants.end(),
                std::back_inserter(config.followers));
      std::transform(
          addParticipants.begin(), addParticipants.end(),
          std::inserter(config.participantsConfig.participants,
                        config.participantsConfig.participants.end()),
          [](auto const& it) {
            return std::pair(it.get().serverId(), ParticipantFlags{});
          });
      if (setLeader) {
        config.leader = *setLeader;
      }
      if (setWriteConcern) {
        config.participantsConfig.config.effectiveWriteConcern =
            *setWriteConcern;
      }
      if (setWaitForSync) {
        config.participantsConfig.config.waitForSync = *setWaitForSync;
      }
      return config;
    }
  };

  // TODO Regarding both addNewTerm/addUpdatedTerm: Should we require the term
  //      to be passed, or generate a new one and return it?
  auto addUpdatedTerm(ConfigUpdates updates) -> LogConfig* {
    auto lit = terms.rbegin();
    TRI_ASSERT(lit != terms.rend());
    auto const [prevTerm, prevConf] = *lit;
    auto const term = prevTerm.succ();

    auto config = updates.updateConfig(prevConf);
    config.termSpec.term = term;

    auto it = terms.try_emplace(terms.end(), term, std::move(config));
    return &it->second;
  }

 public:
  std::shared_ptr<test::ReplicatedLogMetricsMock> logMetricsMock =
      std::make_shared<test::ReplicatedLogMetricsMock>();

 protected:
  LoggerContext const loggerContext;
  std::size_t _nextParticipantId = 1;
};

struct ReplicatedLogTest : ::testing::Test {
  testing::TestInfo const* const testInfo =
      testing::UnitTest::GetInstance()->current_test_info();
  constexpr static char gtestStr[] = "gtest";
  LoggerContext const loggerContext =
      LoggerContext(Logger::REPLICATION2)
          .with<gtestStr>(fmt::format("{}.{}", testInfo->test_suite_name(),
                                      testInfo->test_case_name()));

  WholeLog wholeLog = WholeLog(loggerContext, LogId{1});

  auto createParticipant(LogArguments fakeArguments) {
    return wholeLog.createParticipant(fakeArguments);
  }
  auto createFakeFollower() { return wholeLog.createFakeFollower(); }

  // TODO Maybe take `LogContainer*` instead of
  //      `std::reference_wrapper<LogContainer>`? It should match what
  //      createParticipant returns, for convenience and consistency.
  auto addNewTerm(std::optional<std::reference_wrapper<LogContainer>> leader,
                  std::vector<std::reference_wrapper<LogContainer>> follower,
                  ConfigArguments configArguments) {
    return wholeLog.addNewTerm(leader, std::move(follower), configArguments);
  }
  auto addNewTerm(std::optional<ParticipantId> leader,
                  std::vector<ParticipantId> follower,
                  ConfigArguments configArguments) {
    return wholeLog.addNewTerm(leader, std::move(follower), configArguments);
  }
  // TODO Maybe take ParticipantWithFakes as well (or even FakeFollower) - just
  //      something that we can get a participant ID from:
  // auto addNewTerm(std::optional<ITestParticipant*> leader,
  //                 std::vector<ITestParticipant*> follower,
  //                 ConfigArguments configArguments) {
  //   ...
  // }

  auto addUpdatedTerm(WholeLog::ConfigUpdates updates) {
    return wholeLog.addUpdatedTerm(std::move(updates));
  }

  template<std::size_t replicationFactor>
  requires requires { replicationFactor >= 1; }
  auto createLogs(LogConfig config)
      -> std::pair<LogContainer,
                   std::array<LogContainer, replicationFactor - 1>> {
    auto leader = createParticipant({});
    auto logs = std::array<LogContainer, replicationFactor>{};
    for (auto&& [i, log] : enumerate(logs)) {
      auto& follower = config.followers[i];
      log = createParticipant({});
    }
    return std::pair(leader, logs);
  }
};

}  // namespace arangodb::replication2::test

// TODO Maybe move the rest of this file into a separate header
namespace arangodb::replication2 {
void PrintTo(LogEntry const& entry, std::ostream* os);
}  // namespace arangodb::replication2

namespace arangodb::replication2::test {
// Allows matching a log entry partially in gtest EXPECT_THAT. Members set to
// std::nullopt are ignored when matching; only the set members are matched.
struct PartialLogEntry {
  std::optional<LogTerm> term{};
  std::optional<LogIndex> index{};
  // Note: Add more (optional) fields to IsMeta/IsPayload as needed;
  // then match them in MatchesMapLogEntry, and print them in PrintTo.
  struct IsMeta {};
  struct IsPayload {
    // Taking a string should suffice for the tests. If you need e.g. velocypack
    // instead, make it a variant (and add a nullopt as well).
    std::optional<std::string> payload{};
  };
  std::variant<std::nullopt_t, IsMeta, IsPayload> payload = std::nullopt;

  friend void PrintTo(PartialLogEntry const& point, std::ostream* os);
};
using PartialLogEntries = std::initializer_list<PartialLogEntry>;

MATCHER_P2(IsTermIndexPair, term, index, "") {
  return arg.term == term and arg.index == index;
}

// Matches a map entry pair (LogIndex, LogEntry) against a PartialLogEntry.
MATCHER(MatchesMapLogEntry,
        fmt::format("{} log entries", negation ? "doesn't match" : "matches")) {
  auto const& logIndex = std::get<0>(arg).first;
  auto const& logEntry = std::get<0>(arg).second;
  auto const& partialLogEntry = std::get<1>(arg);
  return (not partialLogEntry.term.has_value() or
          partialLogEntry.term == logEntry.logTerm()) and
         (not partialLogEntry.index.has_value() or
          (partialLogEntry.index == logIndex and
           partialLogEntry.index == logEntry.logIndex())) and
         (std::visit(
             overload{
                 [](std::nullopt_t) { return true; },
                 [&](PartialLogEntry::IsPayload const& payload) {
                   return logEntry.hasPayload() &&
                          (!payload.payload.has_value() ||
                           (logEntry.logPayload()->slice().isString() &&
                            payload.payload ==
                                logEntry.logPayload()->slice().stringView()));
                 },
                 [&](PartialLogEntry::IsMeta const& payload) {
                   return logEntry.hasMeta();
                 },
             },
             partialLogEntry.payload));
}
}  // namespace arangodb::replication2::test

#endif
