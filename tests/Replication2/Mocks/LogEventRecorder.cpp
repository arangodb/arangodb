////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2022 ArangoDB GmbH, Cologne, Germany
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

#include "LogEventRecorder.h"
using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::test;

auto LogEventRecorderHandler::resign() noexcept
    -> std::unique_ptr<replicated_log::IReplicatedLogMethodsBase> {
  events.emplace_back(LogEvent{.type = LogEvent::Type::kResign});
  return std::move(methods);
}

void LogEventRecorderHandler::leadershipEstablished(
    std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> ptr) {
  events.emplace_back(LogEvent{.type = LogEvent::Type::kLeadershipEstablished});
  methods = std::move(ptr);
}

void LogEventRecorderHandler::becomeFollower(
    std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods> ptr) {
  events.emplace_back(LogEvent{.type = LogEvent::Type::kBecomeFollower});
  methods = std::move(ptr);
}

void LogEventRecorderHandler::acquireSnapshot(arangodb::ServerID leader,
                                              LogIndex index) {
  events.emplace_back(LogEvent{.type = LogEvent::Type::kAcquireSnapshot,
                               .leader = leader,
                               .index = index});
}

void LogEventRecorderHandler::updateCommitIndex(LogIndex index) {
  events.emplace_back(LogEvent{.type = LogEvent::Type::kCommitIndex});
}

void LogEventRecorderHandler::dropEntries() {
  events.emplace_back(LogEvent{.type = LogEvent::Type::kDropEntries});
}
