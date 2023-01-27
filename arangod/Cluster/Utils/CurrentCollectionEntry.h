////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/StaticStrings.h"
#include "Cluster/Utils/ResponsibleServerList.h"
#include "Containers/NodeHashMap.h"

#include <string>

namespace arangodb {

struct PlanShardToServerMapping;

// TODO: Move Out to own file
struct CurrentShardEntry {
  std::optional<ResponsibleServerList> servers{std::nullopt};
  std::optional<ResponsibleServerList> failoverCandidates{std::nullopt};
  std::optional<velocypack::Builder> indexes{std::nullopt};

  ErrorCode errorNum{TRI_ERROR_NO_ERROR};
  bool isError{false};
  std::string errorMessage{StaticStrings::Empty};
};

template<class Inspector>
auto inspect(Inspector& f, CurrentShardEntry& entry) {
  return f.object(entry).fields(
      f.field("servers", entry.servers), f.field("indexes", entry.indexes),
      f.field(StaticStrings::FailoverCandidates, entry.failoverCandidates),
      f.field(StaticStrings::Error, entry.isError),
      f.field(StaticStrings::ErrorNum, entry.errorNum),
      f.field(StaticStrings::ErrorMessage, entry.errorMessage));
}

struct CurrentCollectionEntry {
  containers::NodeHashMap<ShardID, CurrentShardEntry> shards;

  /**
   * Quick check if any shard reported an error
   * @return true if there is at least on error
   */
  [[nodiscard]] auto hasError() const noexcept -> bool;

  /**
   * Create a report on all errors that are reported to current
   * @return Printable string that contains all error messages
   */
  [[nodiscard]] auto createErrorReport() const noexcept -> std::string;

  /**
   * Tests if we have exactly one entry per expected shard.
   * This is true if all leaders of shards have reported at least once.
   * This will *not* check if followers are reported.
   *
   * @param expectedNumberOfShards
   * @return true if we have expected number of shards (or more, should not
   * happen though)
   */
  [[nodiscard]] auto haveAllShardsReported(
      size_t expectedNumberOfShards) const noexcept -> bool;

  /**
   * Tests if current entry matches the expected plan.
   * This is only true if al leaders match, and all followers are
   * as requested
   *
   * @param expected the entry in the plan, that we want to reach in current
   * @return true only on exact matches including followers.
   */
  [[nodiscard]] auto doExpectedServersMatch(
      PlanShardToServerMapping const& expected) const noexcept -> bool;

  // Cannot use built_in variant type here, as we have no field that denotes
  // which one is in use, they have different attributes each Would prefer to
  // use an inspect here as well.
};

template<class Inspector>
auto inspect(Inspector& f, CurrentCollectionEntry& x) {
  return f.apply(x.shards);
}

}  // namespace arangodb
