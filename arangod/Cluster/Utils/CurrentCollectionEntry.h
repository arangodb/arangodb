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
#include "Cluster/ClusterTypes.h"
#include "Cluster/Utils/ResponsibleServerList.h"
#include "Containers/FlatHashMap.h"

#include <string>
// TODO: This file needs some rework.
// Would like to get away with simple Inspect.
namespace arangodb {

struct PlanShardToServerMapping;

template<typename T>
class ResultT;

// TODO: Move Out to own file
struct ShardError {
  // In case someone forgets to properly set this, we default to an internal
  // error
  ErrorCode errorNum{TRI_ERROR_INTERNAL};
  bool isError{true};
  std::string errorMessage{StaticStrings::Empty};
};

struct ResponsibleServersOrError {
  std::variant<ResponsibleServerList, ShardError> data;

  static auto fromVPack(arangodb::velocypack::Slice body)
      -> ResultT<ResponsibleServersOrError>;
};

struct CurrentCollectionEntry {
  containers::FlatHashMap<ShardID, ResponsibleServersOrError> shards;

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
  static auto fromVPack(arangodb::velocypack::Slice body)
      -> ResultT<CurrentCollectionEntry>;
};

template<class Inspector>
auto inspect(Inspector& f, ShardError& x) {
  return f.object(x).fields(
      f.field(StaticStrings::Error, x.isError),
      f.field(StaticStrings::ErrorNum, x.errorNum),
      f.field(StaticStrings::ErrorMessage, x.errorMessage));
}
}  // namespace arangodb
