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

#include "CurrentCollectionEntry.h"

#include "Basics/VelocyPackHelper.h"
#include "Cluster/Utils/PlanShardToServerMappping.h"
#include "Inspection/VPack.h"

#include <algorithm>

using namespace arangodb;

auto CurrentCollectionEntry::hasError() const noexcept -> bool {
  return std::any_of(shards.begin(), shards.end(),
                     [](auto const& s) { return s.second.isError; });
}

auto CurrentCollectionEntry::createErrorReport() const noexcept -> std::string {
  std::string report;
  for (auto const& [shardId, maybeError] : shards) {
    if (maybeError.isError) {
      report += " shardID:" + shardId + ": " + maybeError.errorMessage +
                " (errorNum=" + to_string(maybeError.errorNum) + ")";
    }
  }
  return report;
}

auto CurrentCollectionEntry::haveAllShardsReported(
    size_t expectedNumberOfShards) const noexcept -> bool {
  // If this assert triggers CURRENT contains more shards for a collection
  // than we asked for.
  TRI_ASSERT(shards.size() <= expectedNumberOfShards);
  return shards.size() >= expectedNumberOfShards;
}

auto CurrentCollectionEntry::doExpectedServersMatch(
    PlanShardToServerMapping const& expected) const noexcept -> bool {
  if (expected.shards.size() != shards.size()) {
    // Not all have reported yet;
    return false;
  }

  return std::all_of(shards.begin(), shards.end(), [&expected](auto const& s) {
    auto const& [shardId, maybeResponse] = s;
    if (!maybeResponse.isError) {
      TRI_ASSERT(maybeResponse.servers.has_value());
      auto const expectedListIt = expected.shards.find(shardId);
      if (expectedListIt == expected.shards.end()) {
        // Got report for a shard we did not expected
        // Should not happen, assert in maintainer.
        // Return false in Prod to eventually abort with "did not work"
        TRI_ASSERT(false);
        return false;
      } else {
        if (expectedListIt->second != maybeResponse.servers.value()) {
          return false;
        }
      }
      return true;
    }
    // we have an error here
    return false;
  });
}
