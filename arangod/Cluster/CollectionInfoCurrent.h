////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string_view>
#include <vector>

#include <velocypack/Slice.h>

#include "Cluster/ClusterTypes.h"
#include "Cluster/Utils/ShardID.h"
#include "Containers/FlatHashMap.h"

namespace arangodb {

class CollectionInfoCurrent {
  friend class ClusterInfo;

 public:
  explicit CollectionInfoCurrent(uint64_t currentVersion);

  CollectionInfoCurrent(CollectionInfoCurrent const&) = delete;

  CollectionInfoCurrent(CollectionInfoCurrent&&) = delete;

  CollectionInfoCurrent& operator=(CollectionInfoCurrent const&) = delete;

  CollectionInfoCurrent& operator=(CollectionInfoCurrent&&) = delete;

  virtual ~CollectionInfoCurrent();

 public:
  bool add(ShardID const& shardID, VPackSlice slice);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the indexes
  //////////////////////////////////////////////////////////////////////////////

  [[nodiscard]] VPackSlice getIndexes(ShardID const& shardID) const;

  /// @brief returns the error flag for a shardID
  [[nodiscard]] bool error(ShardID const& shardID) const;

  /// @brief returns the error flag for all shardIDs
  [[nodiscard]] containers::FlatHashMap<ShardID, bool> error() const;

  /// @brief returns the errorNum for one shardID
  [[nodiscard]] int errorNum(ShardID const& shardID) const;

  /// @brief returns the errorNum for all shardIDs
  [[nodiscard]] containers::FlatHashMap<ShardID, int> errorNum() const;

  /// @brief returns the current leader and followers for a shard
  [[nodiscard]] TEST_VIRTUAL std::vector<ServerID> servers(
      ShardID const& shardID) const;

  /// @brief returns the current failover candidates for the given shard
  [[nodiscard]] TEST_VIRTUAL std::vector<ServerID> failoverCandidates(
      ShardID const& shardID) const;

  /// @brief returns the errorMessage entry for one shardID
  [[nodiscard]] std::string errorMessage(ShardID const& shardID) const;

  /// @brief get version that underlies this info in Current in the agency
  [[nodiscard]] uint64_t getCurrentVersion() const;

 private:
  /// @brief local helper to return boolean flags
  [[nodiscard]] bool getFlag(std::string_view name,
                             ShardID const& shardID) const;

  /// @brief local helper to return a map to boolean
  [[nodiscard]] containers::FlatHashMap<ShardID, bool> getFlag(
      std::string_view name) const;

  containers::FlatHashMap<ShardID, std::shared_ptr<VPackBuilder>> _vpacks;

  uint64_t _currentVersion;  // Version of Current in the agency that
                             // underpins the data presented in this object
};

}  // end namespace arangodb
