////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_COLLECTION_H
#define ARANGOD_AQL_COLLECTION_H 1

#include "Cluster/ClusterTypes.h"
#include "Transaction/CountCache.h"
#include "VocBase/AccessMode.h"
#include "VocBase/vocbase.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace transaction {
class Methods;
}

class Index;

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

  Collection(std::string const&, TRI_vocbase_t*, AccessMode::Type accessType, Hint hint);

  TRI_vocbase_t* vocbase() const;

  /// @brief upgrade the access type to exclusive
  void setExclusiveAccess();

  AccessMode::Type accessType() const;
  void accessType(AccessMode::Type type);

  bool isReadWrite() const;

  void isReadWrite(bool isReadWrite);

  /// @brief get the collection id
  TRI_voc_cid_t id() const;

  /// @brief returns the name of the collection, translated for the sharding
  /// case. this will return _currentShard if it is set, and name otherwise
  std::string const& name() const;

  /// @brief collection type
  TRI_col_type_e type() const;

  /// @brief count the number of documents in the collection
  size_t count(transaction::Methods* trx, transaction::CountType type) const;

  /// @brief returns the responsible servers for the collection
  std::unordered_set<std::string> responsibleServers() const;

  /// @brief returns the "distributeShardsLike" attribute for the collection
  std::string distributeShardsLike() const;

  /// @brief fills the set with the responsible servers for the collection
  /// returns the number of responsible servers found for the collection
  size_t responsibleServers(std::unordered_set<std::string>&) const;

  /// @brief returns the shard ids of a collection
  std::shared_ptr<std::vector<std::string>> shardIds() const;

  /// @brief returns the filtered list of shard ids of a collection
  std::shared_ptr<std::vector<std::string>> shardIds(std::unordered_set<std::string> const& includedShards) const;

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

  /// @brief get the index by it's identifier. Will either throw or
  ///        return a valid index. nullptr is impossible.
  std::shared_ptr<arangodb::Index> indexByIdentifier(std::string const& idxId) const;
  
  std::vector<std::shared_ptr<arangodb::Index>> indexes() const;
  
  /// @brief use the already set collection 
  /// Whenever getCollection() is called, _collection must be a non-nullptr or an 
  /// assertion will be triggered. In non-maintainer mode an exception will be thrown.
  /// that means getCollection() must not be called on non-existing collections or
  /// views
  std::shared_ptr<arangodb::LogicalCollection> getCollection() const;

  /// @brief whether or not we have a collection object underneath (true for
  /// existing collections, false for non-existing collections and for views).
  bool hasCollectionObject() const noexcept;
  
 private:
  /// @brief throw if the underlying collection has not been set
  void ensureCollection() const;

 private:
  // _collection will only be populated here in the constructor, and not later.
  // note that it will only be populated for "real" collections and shards though. 
  // aql::Collection objects can also be created for views and for non-existing 
  // collections. In these cases it is not possible to populate _collection, at all.
  std::shared_ptr<arangodb::LogicalCollection> _collection;

  TRI_vocbase_t* _vocbase;

  std::string const _name;

  /// @brief currently handled shard. this is a temporary variable that will
  /// only be filled during plan creation
  std::string _currentShard;

  AccessMode::Type _accessType;

  bool _isReadWrite;

  /// @brief a constant mapping for shards => ServerName as they ware planned
  /// at the beginning. This way we can distribute data to servers without
  /// asking the Agency periodically, or even suffer from failover scenarios.
  mutable std::unordered_map<ShardID, ServerID> _shardToServerMapping;
};
}  // namespace aql
}  // namespace arangodb

#endif
