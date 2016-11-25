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

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::aql;

/// @brief create a collection wrapper
Collection::Collection(std::string const& name, TRI_vocbase_t* vocbase,
                       TRI_transaction_type_e accessType)
    : collection(nullptr),
      currentShard(),
      name(name),
      vocbase(vocbase),
      accessType(accessType),
      isReadWrite(false) {
  TRI_ASSERT(!name.empty());
  TRI_ASSERT(vocbase != nullptr);
}

/// @brief destroy a collection wrapper
Collection::~Collection() {}


/// @brief get the collection id
TRI_voc_cid_t Collection::cid() const {
  return getCollection()->cid();
}
  
/// @brief count the number of documents in the collection
size_t Collection::count() const {
  if (numDocuments == UNINITIALIZED) {
    if (arangodb::ServerState::instance()->isCoordinator()) {
      // cluster case
      uint64_t result;
      int res = arangodb::countOnCoordinator(vocbase->name(), name, result);
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            res, "could not determine number of documents in collection");
      }
      numDocuments = static_cast<int64_t>(result);
    } else {
      // local case
      // cache the result
      numDocuments = static_cast<int64_t>(collection->numberDocuments());
    }
  }

  return static_cast<size_t>(numDocuments);
}

/// @brief returns the collection's plan id
TRI_voc_cid_t Collection::getPlanId() const {
  return getCollection()->cid();
}

/// @brief returns the shard ids of a collection
std::shared_ptr<std::vector<std::string>> Collection::shardIds() const {
  auto clusterInfo = arangodb::ClusterInfo::instance();
  auto coll = getCollection();
  if (coll->isSmart() && coll->type() == TRI_COL_TYPE_EDGE) {
    auto names = coll->realNamesForRead();
    auto res = std::make_shared<std::vector<std::string>>();
    for (auto const& n : names) {
      auto collectionInfo = clusterInfo->getCollection(vocbase->name(), n);
      auto list = clusterInfo->getShardList(
          arangodb::basics::StringUtils::itoa(collectionInfo->cid()));
      for (auto const& x : *list) {
        res->push_back(x);
      }
    }
    return res;
  }
  return clusterInfo->getShardList(
      arangodb::basics::StringUtils::itoa(getPlanId()));
}

/// @brief returns the shard keys of a collection
std::vector<std::string> Collection::shardKeys() const {
  auto coll = getCollection();
  std::vector<std::string> keys;
  for (auto const& x : coll->shardKeys()) {
    keys.emplace_back(x);
  }
  return keys;
}

/// @brief whether or not the collection uses the default sharding
bool Collection::usesDefaultSharding() const {
  return getCollection()->usesDefaultShardKeys();
}

/// @brief either use the set collection or get one from ClusterInfo:
std::shared_ptr<LogicalCollection> Collection::getCollection() const {
  if (collection == nullptr) {
    auto clusterInfo = arangodb::ClusterInfo::instance();
    return clusterInfo->getCollection(vocbase->name(), name);
  }
  std::shared_ptr<LogicalCollection> dummy;   // intentionally empty
  // Use the aliasing constructor:
  return std::shared_ptr<LogicalCollection>(dummy, collection);
}
