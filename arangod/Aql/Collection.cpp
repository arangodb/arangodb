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

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::aql;

/// @brief create a collection wrapper
Collection::Collection(std::string const& name, TRI_vocbase_t* vocbase, AccessMode::Type accessType)
    : _collection(nullptr),
      _vocbase(vocbase),
      _name(name),
      _accessType(accessType),
      _isReadWrite(false) {
  TRI_ASSERT(!_name.empty());
  TRI_ASSERT(_vocbase != nullptr);
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
size_t Collection::count(transaction::Methods* trx) const {
  // estimate for the number of documents in the collection. may be outdated...
  OperationResult res = trx->count(_name, transaction::CountType::TryCache);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res.result);
  }
  return static_cast<size_t>(res.slice().getUInt());
}

/// @brief returns the collection's plan id
TRI_voc_cid_t Collection::getPlanId() const { return getCollection()->id(); }

std::unordered_set<std::string> Collection::responsibleServers() const {
  std::unordered_set<std::string> result;
  auto clusterInfo = arangodb::ClusterInfo::instance();

  auto shardIds = this->shardIds();
  for (auto const& it : *shardIds) {
    auto servers = clusterInfo->getResponsibleServer(it);
    result.emplace((*servers)[0]);
  }
  return result;
}

size_t Collection::responsibleServers(std::unordered_set<std::string>& result) const {
  auto clusterInfo = arangodb::ClusterInfo::instance();

  size_t n = 0;
  auto shardIds = this->shardIds();
  for (auto const& it : *shardIds) {
    auto servers = clusterInfo->getResponsibleServer(it);
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
  auto clusterInfo = arangodb::ClusterInfo::instance();
  auto coll = getCollection();
  if (coll->isSmart() && coll->type() == TRI_COL_TYPE_EDGE) {
    auto names = coll->realNamesForRead();
    auto res = std::make_shared<std::vector<std::string>>();
    for (auto const& n : names) {
      auto collectionInfo = clusterInfo->getCollection(_vocbase->name(), n);
      auto list = clusterInfo->getShardList(
          arangodb::basics::StringUtils::itoa(collectionInfo->id()));
      for (auto const& x : *list) {
        res->push_back(x);
      }
    }
    return res;
  }

  return clusterInfo->getShardList(arangodb::basics::StringUtils::itoa(getPlanId()));
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
    
  if (normalize && 
      coll->isSmart() && coll->type() == TRI_COL_TYPE_DOCUMENT) {
    // smart vertex collection always has ["_key:"] as shard keys
    TRI_ASSERT(originalKeys.size() == 1);
    TRI_ASSERT(originalKeys[0] == "_key:");
    // now normalize it this to _key
    return std::vector<std::string>{ StaticStrings::KeyString };
  }
  
  std::vector<std::string> keys;
  keys.reserve(originalKeys.size());
  for (auto const& key : originalKeys) {
    keys.emplace_back(key);
  }

  return keys;
}

size_t Collection::numberOfShards() const {
  return getCollection()->numberOfShards();
}

/// @brief whether or not the collection uses the default sharding
bool Collection::usesDefaultSharding() const {
  return getCollection()->usesDefaultShardKeys();
}

void Collection::setCollection(arangodb::LogicalCollection* coll) {
  _collection = coll;
}

/// @brief either use the set collection or get one from ClusterInfo:
std::shared_ptr<LogicalCollection> Collection::getCollection() const {
  if (_collection == nullptr) {
    TRI_ASSERT(ServerState::instance()->isRunningInCluster());
    auto clusterInfo = arangodb::ClusterInfo::instance();
    return clusterInfo->getCollection(_vocbase->name(), _name);
  }
  std::shared_ptr<LogicalCollection> dummy;  // intentionally empty
  // Use the aliasing constructor:
  return std::shared_ptr<LogicalCollection>(dummy, _collection);
}

/// @brief check smartness of the underlying collection
bool Collection::isSmart() const { return getCollection()->isSmart(); }

/// @brief check if collection is a satellite collection
bool Collection::isSatellite() const { return getCollection()->isSatellite(); }
  
/// @brief return the name of the smart join attribute (empty string
/// if no smart join attribute is present)
std::string const& Collection::smartJoinAttribute() const { return getCollection()->smartJoinAttribute(); }
