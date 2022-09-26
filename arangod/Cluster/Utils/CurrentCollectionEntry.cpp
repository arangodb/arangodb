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

#include "Basics/ResultT.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/Utils/PlanShardToServerMappping.h"
#include "Inspection/VPack.h"

using namespace arangodb;

ResultT<ResponsibleServersOrError> ResponsibleServersOrError::fromVPack(
    VPackSlice body) {
  if (!body.isObject()) {
    return Result{TRI_ERROR_BAD_PARAMETER, "Shard information in current is not an object"};
  }
  auto isError = basics::VelocyPackHelper::getBooleanValue(
      body, StaticStrings::Error, false);
  if (isError) {
    ShardError err;
    auto status = velocypack::deserializeWithStatus(body, err);
    if (!status.ok()) {
      return Result{
          TRI_ERROR_BAD_PARAMETER,
          status.error() +
              (status.path().empty() ? "" : " on path " + status.path())};
    }
    return ResponsibleServersOrError{err};
  } else {
    ResponsibleServerList servers;
    auto status = velocypack::deserializeWithStatus(body, servers);
    if (!status.ok()) {
      return Result{
          TRI_ERROR_BAD_PARAMETER,
          status.error() +
              (status.path().empty() ? "" : " on path " + status.path())};
    }
    return ResponsibleServersOrError{servers};
  }
  return {TRI_ERROR_NO_ERROR};
}

auto CurrentCollectionEntry::hasError() const noexcept -> bool {
  return std::ranges::any_of(shards, [](auto const& s) {
    return holds_alternative<ShardError>(s.second.data);
  });
}

auto CurrentCollectionEntry::createErrorReport() const noexcept -> std::string {
  std::string report;
  for (auto const& [shardId, maybeError] : shards) {
    if (holds_alternative<ShardError>(maybeError.data)) {
      auto const& err = get<ShardError>(maybeError.data);
      report += " shardID:" + shardId + ": " + err.errorMessage +
                " (errorNum=" + to_string(err.errorNum) + ")";
    }
  }
  return report;
}

auto CurrentCollectionEntry::haveAllShardsReported(size_t expectedNumberOfShards) const noexcept
    -> bool {
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

  return std::ranges::all_of(shards, [&expected](auto const& s) {
    auto const& [shardId, maybeResponse] = s;
    if (holds_alternative<ResponsibleServerList>(maybeResponse.data)) {
      auto const& reportedList = get<ResponsibleServerList>(maybeResponse.data);
      auto const expectedListIt = expected.shards.find(shardId);
      if (expectedListIt == expected.shards.end()) {
        // Got report for a shard we did not expected
        // Should not happen, assert in maintainer.
        // Return false in Prod to eventually abort with "did not work"
        TRI_ASSERT(false);
        return false;
      } else {
        if (expectedListIt->second != reportedList) {
          return false;
        }
      }
      return true;
    }
    // we have an error here
    return false;
  });
}

ResultT<CurrentCollectionEntry> CurrentCollectionEntry::fromVPack(
    VPackSlice body) {
  if (!body.isObject()) {
    return Result{TRI_ERROR_BAD_PARAMETER, "Shard response list in current is not an object"};
  }
  CurrentCollectionEntry result;
  for (auto const& [shardId, value] : VPackObjectIterator(body)) {
    auto info = ResponsibleServersOrError::fromVPack(value);
    if (info.fail()) {
      return info.result();
    }
    result.shards.emplace(shardId.copyString(), std::move(info.get()));
  }
  return result;
}
