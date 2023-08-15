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

#include "Replication2/Helper/ReplicatedLogTestSetup.h"

#include "Containers/Enumerate.h"
#include "Futures/Utilities.h"

#include <regex>

namespace arangodb::replication2::test {

void LogConfig::installConfig(bool establishLeadership) {
  auto futures = std::vector<futures::Future<futures::Unit>>{};
  for (auto& follower : followers) {
    futures.emplace_back(follower.get().updateConfig(*this));
  }
  futures.emplace_back(leader.get().updateConfig(*this));

  if (establishLeadership) {
    auto future = futures::collectAll(std::move(futures));
    while (leader.get().hasWork() || IHasScheduler::hasWork(followers)) {
      leader.get().runAll();
      IHasScheduler::runAll(followers);
    }

    EXPECT_TRUE(future.hasValue());
    auto leaderStatus = leader.get().log->getQuickStatus();
    EXPECT_EQ(leaderStatus.role, replicated_log::ParticipantRole::kLeader);
    EXPECT_TRUE(leaderStatus.leadershipEstablished);
    for (auto& follower : followers) {
      auto followerStatus = follower.get().log->getQuickStatus();
      EXPECT_EQ(followerStatus.role,
                replicated_log::ParticipantRole::kFollower);
      EXPECT_TRUE(followerStatus.leadershipEstablished);
    }
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
