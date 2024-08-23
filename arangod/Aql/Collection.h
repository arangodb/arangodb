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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Cluster/ClusterTypes.h"
#include "Cluster/Utils/ShardID.h"
#include "Transaction/CountCache.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/voc-types.h"

#include <unordered_set>

struct TRI_vocbase_t;

namespace arangodb {
namespace transaction {
class Methods;
}

class Index;
class LogicalCollection;

namespace aql {

struct Collection {
  enum class Hint {
    // either a view (no collection) or collection is known to not exist
    None,
    // cluster-wide collection name
    Collection,
    // DB-server local shard
    Shard,
  };

  Collection() = delete;
  Collection(Collection const&) = delete;
  Collection& operator=(Collection const&) = delete;

  Collection(std::string const&, TRI_vocbase_t*, AccessMode::Type accessType,
             Hint hint);

  TRI_vocbase_t* vocbase() const noexcept;

  /// @brief upgrade the access type to exclusive
  void setExclusiveAccess() noexcept;

  AccessMode::Type accessType() const noexcept;
  void accessType(AccessMode::Type type) noexcept;

  bool isReadWrite() const noexcept;

  void isReadWrite(bool isReadWrite) noexcept;

  /// @brief get the collection id
  DataSourceId id() const;

  /// @brief returns the name of the collection, translated for the sharding
  /// case. this will return _currentShard if it is set, and name otherwise
  std::string const& name() const;

  /// @brief collection type
  TRI_col_type_e type() const;

  /// @brief count the number of documents in the collection
  size_t count(transaction::Methods* trx, transaction::CountType type) const;

  /// @brief returns the "distributeShardsLike" attribute for the collection
  std::string distributeShardsLike() const;

  /// @brief returns the shard ids of a collection
  std::shared_ptr<std::vector<ShardID> const> shardIds() const;

  /// @brief returns the filtered list of shard ids of a collection
  std::shared_ptr<std::vector<ShardID> const> shardIds(
      std::unordered_set<ShardID> const& includedShards) const;

  /// @brief returns the shard keys of a collection
  /// if "normalize" is true, then the shard keys for a smart vertex collection
  /// will be reported as "_key" instead of "_key:"
  std::vector<std::string> shardKeys(bool normalize) const;

  size_t numberOfShards() const;

  /// @brief whether or not the collection uses the default sharding
  bool usesDefaultSharding() const;

  /// @brief check smartness of the underlying collection
  bool isSmart() const;

  /// @brief check if collection is a disjoint (edge) collection
  bool isDisjoint() const;

  /// @brief check if collection is a SatelliteCollection
  bool isSatellite() const;

  /// @brief return the name of the SmartJoin attribute (empty string
  /// if no SmartJoin attribute is present)
  std::string const& smartJoinAttribute() const;

  /// @brief add a mapping of shard => server id
  /// for later lookup on DistributeNodes
  void addShardToServer(ShardID const& sid, ServerID const& server) const {
    // Cannot add the same shard twice!
    TRI_ASSERT(_shardToServerMapping.find(sid) == _shardToServerMapping.end());
    _shardToServerMapping.try_emplace(sid, server);
  }

  /// @brief lookup the server responsible for the given shard.
  ServerID const& getServerForShard(ShardID const& sid) const {
    auto const& it = _shardToServerMapping.find(sid);
    // Every shard in question has been registered!
    TRI_ASSERT(it != _shardToServerMapping.end());
    return it->second;
  }

  /// @brief get the index by its identifier. Will either throw or
  ///        return a valid index. nullptr is impossible.
  std::shared_ptr<Index> indexByIdentifier(std::string const& idxId) const;

  std::vector<std::shared_ptr<Index>> indexes() const;

  /// @brief use the already set collection
  /// Whenever getCollection() is called, _collection must be a non-nullptr or
  /// an assertion will be triggered. In non-maintainer mode an exception will
  /// be thrown. that means getCollection() must not be called on non-existing
  /// collections or views
  std::shared_ptr<LogicalCollection> getCollection() const;

  /// @brief whether or not we have a collection object underneath (true for
  /// existing collections, false for non-existing collections and for views).
  bool hasCollectionObject() const noexcept;

 private:
  /// @brief throw if the underlying collection has not been set
  void checkCollection() const;

 private:
  // _collection will only be populated here in the constructor, and not later.
  // note that it will only be populated for "real" collections and shards
  // though. aql::Collection objects can also be created for views and for
  // non-existing collections. In these cases it is not possible to populate
  // _collection, at all.
  std::shared_ptr<arangodb::LogicalCollection> _collection;

  TRI_vocbase_t* _vocbase;

  std::string const _name;

  /// @brief currently handled shard. this is a temporary variable that will
  /// only be filled during plan creation
  std::string _currentShard;

  AccessMode::Type _accessType;

  bool _isReadWrite;

  /// @brief a constant mapping for shards => ServerName as they were planned
  /// at the beginning. This way we can distribute data to servers without
  /// asking the Agency periodically, or even suffer from failover scenarios.
  mutable std::unordered_map<ShardID, ServerID> _shardToServerMapping;
};
}  // namespace aql
}  // namespace arangodb
