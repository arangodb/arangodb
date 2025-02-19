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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/Helper/ReplicatedLogTestSetup.h"

// TODO Remove this conditional as soon as we've upgraded MSVC.
#ifdef DISABLE_I_HAS_SCHEDULER
#pragma message(                                                   \
    "Warning: Not compiling this file due to a compiler bug, see " \
    "IHasScheduler.h for details.")
#else

#include "Containers/Enumerate.h"
#include "Futures/Utilities.h"

#include <regex>

namespace arangodb::replication2::test {

auto LogConfig::makeConfig(
    std::optional<std::reference_wrapper<LogContainer>> leader,
    std::vector<std::reference_wrapper<LogContainer>> follower,
    ConfigArguments configArguments) -> LogConfig {
  auto logConfig = agency::LogPlanConfig{configArguments.writeConcern,
                                         configArguments.waitForSync};
  auto participants = agency::ParticipantsFlagsMap{};
  auto const logToParticipant = [&](LogContainer& f) {
    return std::pair(f.serverId(), ParticipantFlags{});
  };
  if (leader) {
    participants.emplace(logToParticipant(*leader));
  }
  std::transform(follower.begin(), follower.end(),
                 std::inserter(participants, participants.end()),
                 logToParticipant);
  auto participantsConfig = agency::ParticipantsConfig{
      .participants = participants, .config = logConfig};
  auto logSpec = agency::LogPlanTermSpecification{
      configArguments.term,
      leader ? std::optional{leader->get().serverInstance()} : std::nullopt};
  return LogConfig{.leader = leader,
                   .followers = std::move(follower),
                   .termSpec = std::move(logSpec),
                   .participantsConfig = std::move(participantsConfig)};
}

void LogConfig::installConfig(bool establishLeadership) {
  TRI_ASSERT(leader);
  auto futures = std::vector<futures::Future<futures::Unit>>{};
  for (auto& follower : followers) {
    futures.emplace_back(follower.get().updateConfig(this));
  }
  futures.emplace_back(leader->get().updateConfig(this));

  if (establishLeadership) {
    auto future = futures::collectAll(std::move(futures));
    while (leader->get().hasWork() || IHasScheduler::hasWork(followers)) {
      leader->get().runAll();
      IHasScheduler::runAll(followers);
    }

    EXPECT_TRUE(future.isReady());
    EXPECT_TRUE(future.hasValue());
    TRI_ASSERT(leader->get().contains<ParticipantWithFakes>());
    auto& leaderParticipantWithFakes =
        leader->get().getAs<ParticipantWithFakes>();
    auto leaderStatus = leaderParticipantWithFakes.log->getQuickStatus();
    EXPECT_EQ(leaderStatus.role, replicated_log::ParticipantRole::kLeader);
    EXPECT_TRUE(leaderStatus.leadershipEstablished);
    for (auto& follower : followers) {
      if (follower.get().contains<ParticipantWithFakes>()) {
        auto followerStatus =
            follower.get().getAs<ParticipantWithFakes>().log->getQuickStatus();
        EXPECT_EQ(followerStatus.role,
                  replicated_log::ParticipantRole::kFollower);
        EXPECT_TRUE(followerStatus.leadershipEstablished);
      } else {
        TRI_ASSERT(follower.get().contains<ParticipantFakeFollower>());
        // The fake abstract follower doesn't have a status, and doesn't keep
        // track of the commit index. So no checks here.
        TRI_ASSERT(follower.get()
                       .getAs<ParticipantFakeFollower>()
                       .fakeAbstractFollower != nullptr);
      }
    }
  }
}

auto ParticipantWithFakes::updateConfig(LogConfig const* conf)
    -> futures::Future<futures::Unit> {
  bool const isLeader = conf->leader && conf->leader->get() == *this;
  if (isLeader) {
    stateHandleMock->expectLeader();
    for (auto const it : conf->followers) {
      auto& followerContainer = it.get();
      fakeFollowerFactory->followerThunks.try_emplace(
          followerContainer.serverId(), [&followerContainer]() {
            return followerContainer.getAbstractFollower();
          });
    }
  } else {
    stateHandleMock->expectFollower();
  }
  auto fut =
      log->updateConfig(std::move(conf->termSpec),
                        std::move(conf->participantsConfig), _serverInstance);
  if (!isLeader) {
    return std::move(fut).thenValue([&](futures::Unit&&) {
      delayedLogFollower->scheduler.dropWork();
      delayedLogFollower->replaceFollowerWith(getAsFollower());
    });
  } else {
    return fut;
  }
}

void PrintTo(PartialLogEntry const& point, std::ostream* os) {
  *os << "(";
  if (point.term) {
    *os << *point.term;
  } else {
    *os << "?";
  }
  *os << ":";
  if (point.index) {
    *os << *point.index;
  } else {
    *os << "?";
  }
  *os << ";";
  std::visit(overload{
                 [&](std::nullopt_t) { *os << "?"; },
                 [&](PartialLogEntry::IsMeta const&) { *os << "meta=?"; },
                 [&](PartialLogEntry::IsPayload const& pl) {
                   *os << "payload=";
                   if (pl.payload.has_value()) {
                     *os << "\""
                         << std::regex_replace(*pl.payload, std::regex{"\""},
                                               "\\\"")
                         << "\"";
                   } else {
                     *os << "?";
                   }
                 },
             },
             point.payload);
  *os << ")";
}

auto LogContainer::createWithParticipantWithFakes(
    LogId logId, agency::ServerInstanceReference serverId,
    LoggerContext loggerContext,
    std::shared_ptr<replicated_log::ReplicatedLogMetrics> logMetrics,
    LogArguments fakeArguments) -> LogContainer {
  return LogContainer(static_cast<ParticipantWithFakes*>(nullptr), logId,
                      serverId, loggerContext, logMetrics,
                      std::move(fakeArguments));
}

auto LogContainer::createWithFakeFollower(
    agency::ServerInstanceReference serverId) -> LogContainer {
  return LogContainer(static_cast<ParticipantFakeFollower*>(nullptr),
                      std::move(serverId));
}

}  // namespace arangodb::replication2::test

namespace arangodb::replication2 {
void PrintTo(arangodb::replication2::LogEntry const& entry, std::ostream* os) {
  *os << "(";
  *os << entry.logIndex() << ":" << entry.logTerm() << ";";
  if (entry.hasPayload()) {
    *os << "payload=" << entry.logPayload()->slice().toJson();
  } else {
    auto builder = velocypack::Builder();
    entry.meta()->toVelocyPack(builder);
    *os << "meta=" << builder.slice().toJson();
  }
  *os << ")";
}
}  // namespace arangodb::replication2

#endif
