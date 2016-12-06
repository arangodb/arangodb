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
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
namespace aql {
struct Index;

struct Collection {
  Collection& operator=(Collection const&) = delete;
  Collection(Collection const&) = delete;
  Collection() = delete;

  Collection(std::string const&, TRI_vocbase_t*, TRI_transaction_type_e);

  ~Collection();

  /// @brief set the current shard
  inline void setCurrentShard(std::string const& shard) {
    currentShard = shard;
  }

  /// @brief remove the current shard
  inline void resetCurrentShard() { currentShard = ""; }

  /// @brief get the collection id
  TRI_voc_cid_t cid() const;

  /// @brief returns the name of the collection, translated for the sharding
  /// case. this will return currentShard if it is set, and name otherwise
  std::string getName() const {
    if (!currentShard.empty()) {
      // sharding case: return the current shard name instead of the collection
      // name
      return currentShard;
    }

    // non-sharding case: simply return the name
    return name;
  }

  /// @brief count the LOCAL number of documents in the collection
  size_t count() const;

  /// @brief returns the collection's plan id
  TRI_voc_cid_t getPlanId() const;

  /// @brief returns the shard ids of a collection
  std::shared_ptr<std::vector<std::string>> shardIds() const;

  /// @brief returns the shard keys of a collection
  std::vector<std::string> shardKeys() const;

  /// @brief whether or not the collection uses the default sharding
  bool usesDefaultSharding() const;

  /// @brief set the underlying collection
  void setCollection(arangodb::LogicalCollection* coll) { collection = coll; }

  /// @brief either use the set collection or get one from ClusterInfo:
  std::shared_ptr<arangodb::LogicalCollection> getCollection() const;

  /// @brief check smartness of the underlying collection
  bool isSmart() const {
    return getCollection()->isSmart();
  }

  /// @brief check if collection is a satellite collection
  bool isSatellite() const {
    return getCollection()->isSatellite();
  }

 private:

  arangodb::LogicalCollection* collection;

  /// @brief currently handled shard. this is a temporary variable that will
  /// only be filled during plan creation
  std::string currentShard;

 public:
  std::string const name;
  TRI_vocbase_t* vocbase;
  TRI_transaction_type_e accessType;
  bool isReadWrite;
  int64_t mutable numDocuments = UNINITIALIZED;

  static int64_t const UNINITIALIZED = -1;
};
}
}

#endif
