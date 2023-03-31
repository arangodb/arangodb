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

auto LogEventRecorderHandle::resignCurrentState() noexcept
    -> std::unique_ptr<replicated_log::IReplicatedLogMethodsBase> {
  recorder.events.emplace_back(LogEvent{.type = LogEvent::Type::kResign,
                                        .iterator = {},
                                        .leader = {},
                                        .index = {}});
  return std::move(recorder.methods);
}

void LogEventRecorderHandle::leadershipEstablished(
    std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> ptr) {
  recorder.events.emplace_back(
      LogEvent{.type = LogEvent::Type::kLeadershipEstablished,
               .iterator = {},
               .leader = {},
               .index = {}});
  recorder.methods = std::move(ptr);
}

void LogEventRecorderHandle::becomeFollower(
    std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods> ptr) {
  recorder.events.emplace_back(LogEvent{.type = LogEvent::Type::kBecomeFollower,
                                        .iterator = {},
                                        .leader = {},
                                        .index = {}});
  recorder.methods = std::move(ptr);
}

void LogEventRecorderHandle::acquireSnapshot(arangodb::ServerID leader,
                                             LogIndex index) {
  recorder.events.emplace_back(
      LogEvent{.type = LogEvent::Type::kAcquireSnapshot,
               .iterator = {},
               .leader = leader,
               .index = index});
}

void LogEventRecorderHandle::updateCommitIndex(LogIndex index) {
  recorder.events.emplace_back(LogEvent{.type = LogEvent::Type::kCommitIndex,
                                        .iterator = {},
                                        .leader = {},
                                        .index = {}});
}

void LogEventRecorderHandle::dropEntries() {
  recorder.events.emplace_back(LogEvent{.type = LogEvent::Type::kDropEntries,
                                        .iterator = {},
                                        .leader = {},
                                        .index = {}});
}
LogEventRecorderHandle::LogEventRecorderHandle(LogEventRecorder& recorder)
    : recorder(recorder) {}

auto LogEventRecorder::createHandle()
    -> std::unique_ptr<replicated_log::IReplicatedStateHandle> {
  return std::make_unique<LogEventRecorderHandle>(*this);
}
