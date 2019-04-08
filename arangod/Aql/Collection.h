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

#include "Basics/Common.h"
#include "VocBase/AccessMode.h"
#include "VocBase/vocbase.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace transaction {
class Methods;
}
namespace aql {

struct Collection {
  Collection() = delete;
  Collection(Collection const&) = delete;
  Collection& operator=(Collection const&) = delete;

  Collection(std::string const&, TRI_vocbase_t*, AccessMode::Type);

  TRI_vocbase_t* vocbase() const { return _vocbase; }

  /// @brief upgrade the access type to exclusive
  void setExclusiveAccess();

  AccessMode::Type accessType() const { return _accessType; }
  void accessType(AccessMode::Type type) { _accessType = type; }

  bool isReadWrite() const { return _isReadWrite; }

  void isReadWrite(bool isReadWrite) { _isReadWrite = isReadWrite; }

  /// @brief set the current shard
  void setCurrentShard(std::string const& shard) { _currentShard = shard; }

  /// @brief remove the current shard
  void resetCurrentShard() { _currentShard.clear(); }

  /// @brief get the collection id
  TRI_voc_cid_t id() const;

  /// @brief returns the name of the collection, translated for the sharding
  /// case. this will return _currentShard if it is set, and name otherwise
  std::string const& name() const {
    if (!_currentShard.empty()) {
      // sharding case: return the current shard name instead of the collection
      // name
      return _currentShard;
    }

    // non-sharding case: simply return the name
    return _name;
  }
  
  /// @brief collection type
  TRI_col_type_e type() const;

  /// @brief count the number of documents in the collection
  size_t count(transaction::Methods* trx) const;

  /// @brief returns the collection's plan id
  TRI_voc_cid_t getPlanId() const;

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

  /// @brief set the underlying collection
  void setCollection(arangodb::LogicalCollection* coll);

  /// @brief either use the set collection or get one from ClusterInfo:
  std::shared_ptr<arangodb::LogicalCollection> getCollection() const;

  /// @brief check smartness of the underlying collection
  bool isSmart() const;

  /// @brief check if collection is a satellite collection
  bool isSatellite() const;

  /// @brief return the name of the smart join attribute (empty string
  /// if no smart join attribute is present)
  std::string const& smartJoinAttribute() const;

 private:
  arangodb::LogicalCollection* _collection;

  TRI_vocbase_t* _vocbase;

  std::string _name;

  /// @brief currently handled shard. this is a temporary variable that will
  /// only be filled during plan creation
  std::string _currentShard;

  AccessMode::Type _accessType;

  bool _isReadWrite;
};
}  // namespace aql
}  // namespace arangodb

#endif
