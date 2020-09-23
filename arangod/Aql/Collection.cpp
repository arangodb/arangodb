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

#include "Collection.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Transaction/Methods.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::aql;

/// @brief create a collection wrapper
Collection::Collection(std::string const& name, 
                       TRI_vocbase_t* vocbase, 
                       AccessMode::Type accessType,
                       Hint hint)
    : _collection(nullptr),
      _vocbase(vocbase),
      _name(name),
      _accessType(accessType),
      _isReadWrite(false) {
  TRI_ASSERT(!_name.empty());
  TRI_ASSERT(_vocbase != nullptr);

  // _collection will only be populated here in the constructor, and not later.
  // note that it will only be populated for "real" collections and shards though. 
  // aql::Collection objects can also be created for views and for non-existing 
  // collections. In these cases it is not possible to populate _collection, at all.
  if (hint == Hint::Collection) {
    if (ServerState::instance()->isRunningInCluster()) {
      auto& clusterInfo = _vocbase->server().getFeature<ClusterFeature>().clusterInfo();
      _collection = clusterInfo.getCollection(_vocbase->name(), _name);
    } else {
      _collection = _vocbase->lookupCollection(_name);
      //ensureCollection(); // will throw if collection does not exist
    }
  } else if (hint == Hint::Shard) {
    if (ServerState::instance()->isCoordinator()) {
      auto& clusterInfo = _vocbase->server().getFeature<ClusterFeature>().clusterInfo();
      _collection = clusterInfo.getCollection(_vocbase->name(), _name);
    } else {
      _collection = _vocbase->lookupCollection(_name);
      //ensureCollection(); // will throw if collection does not exist
    }
  } else if (hint == Hint::None) {
    // nothing special to do here
  }

  // Whenever getCollection() is called later, _collection must have been set
  // here to a non-nullptr, or an assertion will be triggered. 
  // In non-maintainer mode an exception will be thrown.
}

/// @brief upgrade the access type to exclusive
void Collection::setExclusiveAccess() {
  TRI_ASSERT(AccessMode::isWriteOrExclusive(_accessType));
  _accessType = AccessMode::Type::EXCLUSIVE;
}

/// @brief get the collection id
TRI_voc_cid_t Collection::id() const { return getCollection()->id(); }

/// @brief collection type
TRI_col_type_e Collection::type() const { return getCollection()->type(); }

/// @brief count the number of documents in the collection
size_t Collection::count(transaction::Methods* trx, transaction::CountType type) const {
  OperationResult res = trx->count(_name, type); 
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res.result);
  }
  return static_cast<size_t>(res.slice().getUInt());
}

std::unordered_set<std::string> Collection::responsibleServers() const {
  std::unordered_set<std::string> result;
  auto& clusterInfo = _vocbase->server().getFeature<ClusterFeature>().clusterInfo();

  auto shardIds = this->shardIds();
  for (auto const& it : *shardIds) {
    auto servers = clusterInfo.getResponsibleServer(it);
    result.emplace((*servers)[0]);
  }
  return result;
}

size_t Collection::responsibleServers(std::unordered_set<std::string>& result) const {
  auto& clusterInfo = _vocbase->server().getFeature<ClusterFeature>().clusterInfo();

  size_t n = 0;
  auto shardIds = this->shardIds();
  for (auto const& it : *shardIds) {
    auto servers = clusterInfo.getResponsibleServer(it);
    result.emplace((*servers)[0]);
    ++n;
  }
  return n;
}

std::string Collection::distributeShardsLike() const {
  return getCollection()->distributeShardsLike();
}

/// @brief returns the shard ids of a collection
std::shared_ptr<std::vector<std::string>> Collection::shardIds() const {
  auto& clusterInfo = _vocbase->server().getFeature<ClusterFeature>().clusterInfo();
  auto coll = getCollection();
  if (coll->isSmart() && coll->type() == TRI_COL_TYPE_EDGE) {
    auto names = coll->realNamesForRead();
    auto res = std::make_shared<std::vector<std::string>>();
    for (auto const& n : names) {
      auto collectionInfo = clusterInfo.getCollection(_vocbase->name(), n);
      auto list = clusterInfo.getShardList(
          arangodb::basics::StringUtils::itoa(collectionInfo->id()));
      for (auto const& x : *list) {
        res->push_back(x);
      }
    }
    return res;
  }

  return clusterInfo.getShardList(arangodb::basics::StringUtils::itoa(id()));
}

/// @brief returns the filtered list of shard ids of a collection
std::shared_ptr<std::vector<std::string>> Collection::shardIds(
    std::unordered_set<std::string> const& includedShards) const {
  // use the simple method first
  auto copy = shardIds();

  if (includedShards.empty()) {
    // no shards given => return them all!
    return copy;
  }

  // copy first as we will modify the result
  auto result = std::make_shared<std::vector<std::string>>();

  // post-filter the result
  for (auto const& it : *copy) {
    if (includedShards.find(it) == includedShards.end()) {
      continue;
    }
    result->emplace_back(it);
  }

  return result;
}

/// @brief returns the shard keys of a collection
std::vector<std::string> Collection::shardKeys(bool normalize) const {
  auto coll = getCollection();
  auto const& originalKeys = coll->shardKeys();

  if (normalize && coll->isSmart() && coll->type() == TRI_COL_TYPE_DOCUMENT) {
    // smart vertex collection always has ["_key:"] as shard keys
    TRI_ASSERT(originalKeys.size() == 1);
    TRI_ASSERT(originalKeys[0] == "_key:");
    // now normalize it this to _key
    return std::vector<std::string>{StaticStrings::KeyString};
  }
  return {originalKeys.begin(), originalKeys.end()};
}

size_t Collection::numberOfShards() const {
  return getCollection()->numberOfShards();
}

/// @brief whether or not the collection uses the default sharding
bool Collection::usesDefaultSharding() const {
  return getCollection()->usesDefaultShardKeys();
}

/// @brief check smartness of the underlying collection
bool Collection::isSmart() const { return getCollection()->isSmart(); }

bool Collection::isDisjoint() const { return getCollection()->isDisjoint(); }

/// @brief check if collection is a SatelliteCollection
bool Collection::isSatellite() const { return getCollection()->isSatellite(); }

/// @brief return the name of the SmartJoin attribute (empty string
/// if no SmartJoin attribute is present)
std::string const& Collection::smartJoinAttribute() const {
  return getCollection()->smartJoinAttribute();
}

TRI_vocbase_t* Collection::vocbase() const { return _vocbase; }

AccessMode::Type Collection::accessType() const { return _accessType; }

void Collection::accessType(AccessMode::Type type) { _accessType = type; }

bool Collection::isReadWrite() const { return _isReadWrite; }

void Collection::isReadWrite(bool isReadWrite) { _isReadWrite = isReadWrite; }

std::string const& Collection::name() const {
  if (!_currentShard.empty()) {
    // sharding case: return the current shard name instead of the collection
    // name
    return _currentShard;
  }

  // non-sharding case: simply return the name
  return _name;
}

// moved here from transaction::Methods::getIndexByIdentifier(..)
std::shared_ptr<arangodb::Index> Collection::indexByIdentifier(std::string const& idxId) const {
   if (idxId.empty()) {
     THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                    "The index id cannot be empty.");
   }

   if (!arangodb::Index::validateId(idxId.c_str())) {
     THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
   }
  
  auto iid = arangodb::IndexId{arangodb::basics::StringUtils::uint64(idxId)};
  auto idx = this->getCollection()->lookupIndex(iid);

  if (!idx) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
                                   "Could not find index '" + idxId +
                                       "' in collection '" + this->name() +
                                       "'.");
  }
  
  return idx;
}

std::vector<std::shared_ptr<arangodb::Index>> Collection::indexes() const {
  auto coll = this->getCollection();
  
  // update selectivity estimates if they were expired
  if (ServerState::instance()->isCoordinator()) {
    coll->clusterIndexEstimates(true); 
  }
  
  std::vector<std::shared_ptr<Index>> indexes = coll->getIndexes();
  indexes.erase(std::remove_if(indexes.begin(), indexes.end(),
                               [](std::shared_ptr<Index> const& x) {
                                 return x->isHidden();
                               }),
                indexes.end());
  return indexes;
}

/// @brief use the already set collection 
std::shared_ptr<LogicalCollection> Collection::getCollection() const {
  TRI_ASSERT(_collection != nullptr);
  ensureCollection();
  return _collection;
}
  
/// @brief whether or not we have a collection object underneath (true for
/// existing collections, false for non-existing collections and for views).
bool Collection::hasCollectionObject() const noexcept {
  return _collection != nullptr;
}

/// @brief throw if the underlying collection has not been set
void Collection::ensureCollection() const {
  if (_collection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   std::string(TRI_errno_string(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) + ": " + _name);
  }
}
