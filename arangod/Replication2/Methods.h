////////////////////////////////////////////////////////////////////////////////
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Agency/AgencyCommon.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/LogEntries.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"

#include <string>
#include <variant>
#include <vector>

namespace arangodb {
class Result;
namespace futures {
template<typename T>
class Future;
}
}  // namespace arangodb

namespace arangodb::replication2 {

namespace agency {
struct LogTarget;
}

namespace replicated_log {
struct AppendEntriesRequest;
struct AppendEntriesResult;
struct WaitForResult;
}  // namespace replicated_log

/**
 * This is a collection functions that is used by the RestHandler and the V8
 * interface. It covers two different implementations. One for dbservers that
 * actually execute the commands and one for coordinators that forward the
 * request to the leader.
 */
struct ReplicatedLogMethods {
  virtual ~ReplicatedLogMethods() = default;

  static constexpr auto kDefaultLimit = std::size_t{10};

  using GenericLogStatus =
      std::variant<replication2::replicated_log::LogStatus,
                   replication2::replicated_log::GlobalStatus>;
  using ParticipantsList = std::vector<std::string>;

  struct CreateOptions {
    bool waitForReady{true};
    std::optional<LogId> id;
    std::optional<LogConfig> config;
    std::optional<ParticipantId> leader;
    std::vector<ParticipantId> servers;
  };

  struct CreateResult {
    LogId id;
    std::vector<ParticipantId> servers;
  };

  virtual auto createReplicatedLog(CreateOptions spec) const
      -> futures::Future<ResultT<CreateResult>> = 0;

  virtual auto deleteReplicatedLog(LogId id) const
      -> futures::Future<Result> = 0;
  virtual auto getReplicatedLogs() const -> futures::Future<std::unordered_map<
      arangodb::replication2::LogId,
      std::variant<replicated_log::LogStatus, ParticipantsList>>> = 0;
  virtual auto getLocalStatus(LogId) const
      -> futures::Future<replication2::replicated_log::LogStatus> = 0;
  virtual auto getGlobalStatus(
      LogId, replicated_log::GlobalStatus::SpecificationSource) const
      -> futures::Future<replication2::replicated_log::GlobalStatus> = 0;
  virtual auto getStatus(LogId) const -> futures::Future<GenericLogStatus> = 0;

  virtual auto getLogEntryByIndex(LogId, LogIndex) const
      -> futures::Future<std::optional<PersistingLogEntry>> = 0;

  virtual auto slice(LogId, LogIndex start, LogIndex stop) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> = 0;
  virtual auto poll(LogId, LogIndex, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> = 0;
  virtual auto head(LogId, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> = 0;
  virtual auto tail(LogId, std::size_t limit) const
      -> futures::Future<std::unique_ptr<PersistedLogIterator>> = 0;

  virtual auto insert(LogId, LogPayload, bool waitForSync) const
      -> futures::Future<
          std::pair<LogIndex, replicated_log::WaitForResult>> = 0;
  virtual auto insert(LogId, TypedLogIterator<LogPayload>& iter,
                      bool waitForSync) const
      -> futures::Future<
          std::pair<std::vector<LogIndex>, replicated_log::WaitForResult>> = 0;

  // Insert an entry without waiting for the corresponding LogIndex to be
  // committed.
  // TODO This could be merged with `insert()` by using a common result type,
  //      Future<InsertResult> or so, which internally differentiates between
  //      the variants.
  //      See https://arangodb.atlassian.net/browse/CINFRA-278.
  // TODO Implement this for a list of payloads as well, as insert() does.
  //      See https://arangodb.atlassian.net/browse/CINFRA-278.
  virtual auto insertWithoutCommit(LogId, LogPayload, bool waitForSync) const
      -> futures::Future<LogIndex> = 0;

  virtual auto release(LogId, LogIndex) const -> futures::Future<Result> = 0;

  /*
   * Wait until the supervision reports that the replicated log has converged
   * to the given version.
   */
  [[nodiscard]] virtual auto waitForLogReady(LogId, std::uint64_t version) const
      -> futures::Future<ResultT<consensus::index_t>> = 0;

  static auto createInstance(TRI_vocbase_t& vocbase)
      -> std::shared_ptr<ReplicatedLogMethods>;

 private:
  virtual auto createReplicatedLog(agency::LogTarget spec) const
      -> futures::Future<Result> = 0;
};

template<class Inspector>
auto inspect(Inspector& f, ReplicatedLogMethods::CreateOptions& x) {
  return f.object(x).fields(
      f.field("waitForReady", x.waitForReady).fallback(true),
      f.field("id", x.id), f.field("config", x.config),
      f.field("leader", x.leader),
      f.field("servers", x.servers).fallback(std::vector<ParticipantId>{}));
}

template<class Inspector>
auto inspect(Inspector& f, ReplicatedLogMethods::CreateResult& x) {
  return f.object(x).fields(f.field("id", x.id), f.field("servers", x.servers));
}

struct ReplicatedStateMethods {
  virtual ~ReplicatedStateMethods() = default;

  [[nodiscard]] virtual auto waitForStateReady(LogId, std::uint64_t version)
      -> futures::Future<ResultT<consensus::index_t>> = 0;

  virtual auto createReplicatedState(replicated_state::agency::Target spec)
      const -> futures::Future<Result> = 0;
  virtual auto deleteReplicatedLog(LogId id) const
      -> futures::Future<Result> = 0;

  virtual auto getLocalStatus(LogId) const
      -> futures::Future<replicated_state::StateStatus> = 0;

  struct ParticipantSnapshotStatus {
    replicated_state::SnapshotInfo status;
    replicated_state::StateGeneration generation;
  };

  using GlobalSnapshotStatus =
      std::unordered_map<ParticipantId, ParticipantSnapshotStatus>;

  virtual auto getGlobalSnapshotStatus(LogId) const
      -> futures::Future<ResultT<GlobalSnapshotStatus>> = 0;

  static auto createInstance(TRI_vocbase_t& vocbase)
      -> std::shared_ptr<ReplicatedStateMethods>;

  [[nodiscard]] virtual auto replaceParticipant(
      LogId, ParticipantId const& participantToRemove,
      ParticipantId const& participantToAdd,
      std::optional<ParticipantId> const& currentLeader) const
      -> futures::Future<Result> = 0;

  [[nodiscard]] virtual auto setLeader(
      LogId id, std::optional<ParticipantId> const& leaderId) const
      -> futures::Future<Result> = 0;
};

template<class Inspector>
auto inspect(Inspector& f,
             ReplicatedStateMethods::ParticipantSnapshotStatus& x) {
  return f.object(x).fields(f.field("status", x.status),
                            f.field("generation", x.generation));
}

}  // namespace arangodb::replication2
