////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_REPLICATION2_INMEMORYLOG_H
#define ARANGOD_REPLICATION2_INMEMORYLOG_H

#include <Futures/Future.h>
#include <velocypack/SharedSlice.h>

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace arangodb::replication2 {

struct LogIndex {
  LogIndex() : value{0} {}
  explicit LogIndex(std::uint64_t value) : value{value} {}
  std::uint64_t value;

  [[nodiscard]] auto operator==(LogIndex) const -> bool;
  [[nodiscard]] auto operator<=(LogIndex) const -> bool;
  [[nodiscard]] auto operator<(LogIndex) const -> bool;
};
struct LogTerm {
  LogTerm() : value{0} {}
  explicit LogTerm(std::uint64_t value) : value{value} {}
  std::uint64_t value;
};
struct LogPayload {
  // just a placeholder for now
  std::string dummy;
};
// just a placeholder for now, must have a hash<>
using ParticipantId = std::size_t;
struct LogStatistics {
  LogIndex spearHead{};
  LogIndex commitIndex{};
};

// TODO This should probably be moved into a separate file
class LogEntry {
 public:
  LogEntry(LogTerm, LogIndex, LogPayload);

  [[nodiscard]] LogTerm logTerm() const;
  [[nodiscard]] LogIndex logIndex() const;
  [[nodiscard]] LogPayload const& logPayload() const;

 private:
  LogTerm _logTerm{};
  LogIndex _logIndex{};
  LogPayload _payload;
};

struct AppendEntriesResult {
  bool success = false;
  LogTerm logTerm = LogTerm{};
};

struct AppendEntriesRequest {
  LogTerm leaderTerm;
  ParticipantId leaderId;
  LogTerm prevLogTerm;
  LogIndex prevLogIndex;
  LogIndex leaderCommit;
  std::vector<LogEntry> entries;
};

/**
 * @brief State stub, later to be replaced by a RocksDB state.
 */
class InMemoryState {
 public:
  // TODO We probably want an immer::map to be able to get snapshots
  std::map<std::string, arangodb::velocypack::SharedSlice> _state;
};

/**
 * @brief A simple non-persistent log implementation, mainly for prototyping
 * replication 2.0.
 */
class InMemoryLog {
 public:
  InMemoryLog() = delete;
  explicit InMemoryLog(ParticipantId participantId, std::shared_ptr<InMemoryState> state);

  // follower only
  auto appendEntries(AppendEntriesRequest) -> arangodb::futures::Future<AppendEntriesResult>;

  // leader only
  auto insert(LogPayload) -> LogIndex;

  // leader only
  auto createSnapshot() -> std::pair<LogIndex, std::shared_ptr<InMemoryState const>>;

  // leader only
  auto waitFor(LogIndex) -> arangodb::futures::Future<arangodb::futures::Unit>;

  // Set to follower, and (strictly increase) term to the given value
  auto becomeFollower(LogTerm, ParticipantId leaderId) -> void;

  // Set to leader, and (strictly increase) term to the given value
  auto becomeLeader(LogTerm, std::unordered_set<ParticipantId> followerIds, std::size_t writeConcern) -> void;

  [[nodiscard]] auto getStatistics() const -> LogStatistics;

  auto runAsyncStep() -> void;

 private:
  struct Unconfigured {};
  struct Leader {
    std::unordered_set<ParticipantId> followerIds{};
    std::size_t writeConcern{};
  };
  struct Follower {
    ParticipantId leaderId{};
  };

  std::variant<Unconfigured, Leader, Follower> _role;
  ParticipantId _id{};
  LogTerm _currentTerm = LogTerm{};
  std::deque<LogEntry> _log;
  std::shared_ptr<InMemoryState> _state;
  LogIndex _commitIndex{};

  using WaitForPromise = futures::Promise<arangodb::futures::Unit>;
  std::multimap<LogIndex, WaitForPromise> _waitForQueue;

 private:
  LogIndex nextIndex();
};

}  // namespace arangodb::replication2

#endif  // ARANGOD_REPLICATION2_INMEMORYLOG_H
