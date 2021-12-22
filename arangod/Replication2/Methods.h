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
#pragma once

#include "Replication2/ReplicatedLog/LogCommon.h"

namespace arangodb {
class Result;
namespace futures {
template <typename T>
class Future;
}
}  // namespace arangodb

namespace arangodb::replication2 {

namespace agency {
struct LogPlanSpecification;
struct LogPlanTermSpecification;
}  // namespace agency
namespace replicated_log {
struct LogStatus;
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
  static constexpr auto kDefaultLimit = std::size_t{10};

  virtual ~ReplicatedLogMethods() = default;
  virtual auto createReplicatedLog(agency::LogPlanSpecification const& spec) const
      -> futures::Future<Result> = 0;
  virtual auto deleteReplicatedLog(LogId id) const -> futures::Future<Result> = 0;
  virtual auto getReplicatedLogs() const
      -> futures::Future<std::unordered_map<arangodb::replication2::LogId, replicated_log::LogStatus>> = 0;
  virtual auto getLogStatus(LogId) const
      -> futures::Future<replication2::replicated_log::LogStatus> = 0;


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

  virtual auto insert(LogId, LogPayload) const
      -> futures::Future<std::pair<LogIndex, replicated_log::WaitForResult>> = 0;
  virtual auto insert(LogId, TypedLogIterator<LogPayload>& iter) const
      -> futures::Future<std::pair<std::vector<LogIndex>, replicated_log::WaitForResult>> = 0;
  virtual auto release(LogId, LogIndex) const -> futures::Future<Result> = 0;

  static auto createInstance(TRI_vocbase_t& vocbase)
      -> std::shared_ptr<ReplicatedLogMethods>;
};

}  // namespace arangodb::replication2
