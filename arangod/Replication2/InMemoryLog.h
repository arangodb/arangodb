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
#include <vector>

namespace arangodb::replication2 {

struct LogIndex {
  LogIndex() : value{0} {}
  std::uint64_t value;
};
struct LogTerm {
  LogTerm() : value{0} {}
  std::uint64_t value;
};
struct LogPayload {
  // just a placeholder for now
};
struct ParticipantId {
  // just a placeholder for now
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

struct LogConfiguration {
  ParticipantId leaderId;
  std::unordered_set<ParticipantId> followerIds;
  std::size_t writeConcern;
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
  InMemoryLog() = default;
  explicit InMemoryLog(std::shared_ptr<InMemoryState> state);

  auto appendEntries(AppendEntriesRequest) -> arangodb::futures::Future<AppendEntriesResult>;

  auto insert(LogPayload) -> LogIndex;

  auto createSnapshot() -> std::pair<LogIndex, std::shared_ptr<InMemoryState const>>;

  auto waitFor(LogIndex) -> arangodb::futures::Future<arangodb::futures::Unit>;

  // Set to follower, and (strictly increase) term to the given value
  auto becomeFollower(LogTerm, LogConfiguration) -> void;

  // Set to leader, and (strictly increase) term to the given value
  auto becomeLeader(LogTerm, LogConfiguration) -> void;

 private:
  bool isLeader = false;
  LogTerm _currentTerm = LogTerm{};
  std::deque<LogEntry> _log;
  std::shared_ptr<InMemoryState> _state;
};

}  // namespace arangodb::replication2

#endif  // ARANGOD_REPLICATION2_INMEMORYLOG_H
