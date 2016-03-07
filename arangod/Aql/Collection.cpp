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
#include "Aql/Index.h"
#include "Basics/StringUtils.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/HashIndex.h"
#include "Indexes/PrimaryIndex.h"
#include "Indexes/SkiplistIndex.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection wrapper
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a collection wrapper
////////////////////////////////////////////////////////////////////////////////

Collection::~Collection() {
  for (auto& idx : indexes) {
    delete idx;
  }
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief get the pointer to the document collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* Collection::documentCollection() const {
  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(collection->_collection != nullptr);

  return collection->_collection;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the collection is an edge collection
////////////////////////////////////////////////////////////////////////////////

bool Collection::isEdgeCollection() const {
  auto document = documentCollection();
  return (document->_info.type() == TRI_COL_TYPE_EDGE);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the collection is a document collection
//////////////////////////////////////////////////////////////////////////////

bool Collection::isDocumentCollection() const {
  auto document = documentCollection();
  return (document->_info.type() == TRI_COL_TYPE_DOCUMENT);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief count the number of documents in the collection
////////////////////////////////////////////////////////////////////////////////

size_t Collection::count() const {
  if (numDocuments == UNINITIALIZED) {
    if (arangodb::ServerState::instance()->isCoordinator()) {
      // cluster case
      uint64_t result;
      int res = arangodb::countOnCoordinator(vocbase->_name, name, result);
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            res, "could not determine number of documents in collection");
      }
      numDocuments = static_cast<int64_t>(result);
    } else {
      // local case
      auto document = documentCollection();
      // cache the result
      numDocuments = static_cast<int64_t>(document->size());
    }
  }

  return static_cast<size_t>(numDocuments);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection's plan id
////////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t Collection::getPlanId() const {
  auto clusterInfo = arangodb::ClusterInfo::instance();
  auto collectionInfo =
      clusterInfo->getCollection(std::string(vocbase->_name), name);

  if (collectionInfo.get() == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_INTERNAL,
                                  "collection not found '%s' -> '%s'",
                                  vocbase->_name, name.c_str());
  }

  return collectionInfo.get()->id();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard ids of a collection
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<std::vector<std::string>> Collection::shardIds() const {
  auto clusterInfo = arangodb::ClusterInfo::instance();
  return clusterInfo->getShardList(
      arangodb::basics::StringUtils::itoa(getPlanId()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard keys of a collection
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Collection::shardKeys() const {
  auto clusterInfo = arangodb::ClusterInfo::instance();

  std::string id;
  if (arangodb::ServerState::instance()->isDBServer() &&
      documentCollection()->_info.planId() > 0) {
    id = std::to_string(documentCollection()->_info.planId());
  } else {
    id = name;
  }

  auto collectionInfo =
      clusterInfo->getCollection(std::string(vocbase->_name), id);
  if (collectionInfo.get() == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_INTERNAL,
                                  "collection not found '%s' -> '%s'",
                                  vocbase->_name, name.c_str());
  }

  std::vector<std::string> keys;
  for (auto const& x : collectionInfo.get()->shardKeys()) {
    keys.emplace_back(x);
  }
  return keys;
}
////////////////////////////////////////////////////////////////////////////////
/// @brief returns the indexes of the collection
////////////////////////////////////////////////////////////////////////////////

std::vector<Index const*> Collection::getIndexes() const {
  fillIndexes();

  return indexes;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an index by its id
////////////////////////////////////////////////////////////////////////////////

Index const* Collection::getIndex(TRI_idx_iid_t id) const {
  fillIndexes();

  for (auto const& idx : indexes) {
    if (idx->id == id) {
      return idx;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an index by its id
////////////////////////////////////////////////////////////////////////////////

Index const* Collection::getIndex(std::string const& id) const {
  return getIndex(arangodb::basics::StringUtils::uint64(id));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the collection uses the default sharding
////////////////////////////////////////////////////////////////////////////////

bool Collection::usesDefaultSharding() const {
  // check if collection shard keys are only _key
  std::vector<std::string> sk(shardKeys());

  if (sk.size() != 1 || sk[0] != TRI_VOC_ATTRIBUTE_KEY) {
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index list for the collection
////////////////////////////////////////////////////////////////////////////////

void Collection::fillIndexes() const {
  if (!indexes.empty()) {
    return;
  }

  auto const role = arangodb::ServerState::instance()->getRole();

  if (arangodb::ServerState::instance()->isCoordinator(role)) {
    fillIndexesCoordinator();
    return;
  }

  // must have a collection
  TRI_ASSERT(collection != nullptr);

// On a DBserver it is not necessary to consult the agency, therefore
// we rather look at the local indexes.
// FIXME: Remove fillIndexesDBServer later, when it is clear that we
// will never have to do this.
#if 0
  if (arangodb::ServerState::instance()->isDBServer(role) && 
      documentCollection()->_info._planId > 0) {
    fillIndexesDBServer();
    return;
  }
#endif

  fillIndexesLocal();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index list, cluster coordinator case
////////////////////////////////////////////////////////////////////////////////

void Collection::fillIndexesCoordinator() const {
  // coordinator case, remote collection
  auto clusterInfo = arangodb::ClusterInfo::instance();
  auto collectionInfo =
      clusterInfo->getCollection(std::string(vocbase->_name), name);

  if (collectionInfo.get() == nullptr || (*collectionInfo).empty()) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_INTERNAL,
                                  "collection not found '%s' in database '%s'",
                                  name.c_str(), vocbase->_name);
  }

  TRI_json_t const* json = (*collectionInfo).getIndexes();
  auto indexBuilder = arangodb::basics::JsonHelper::toVelocyPack(json);
  VPackSlice const slice = indexBuilder->slice();

  if (slice.isArray()) {
    size_t const n = static_cast<size_t>(slice.length());
    indexes.reserve(n);

    for (auto const& v : VPackArrayIterator(slice)) {
      if (!v.isObject()) {
        continue;
      }
      VPackSlice const type = v.get("type");

      if (!type.isString()) {
        // no "type" attribute. this is invalid
        continue;
      }
      std::string typeString = type.copyString();

      if (typeString == "cap") {
        // ignore cap constraints
        continue;
      }

      auto idx = std::make_unique<arangodb::aql::Index>(v);

      indexes.emplace_back(idx.get());
      auto p = idx.release();

      if (p->type == arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
        p->setInternals(new arangodb::PrimaryIndex(v), true);
      } else if (p->type == arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX) {
        p->setInternals(new arangodb::EdgeIndex(v), true);
      } else if (p->type == arangodb::Index::TRI_IDX_TYPE_HASH_INDEX) {
        p->setInternals(new arangodb::HashIndex(v), true);
      } else if (p->type == arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
        p->setInternals(new arangodb::SkiplistIndex(v), true);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index list, cluster DB server case
////////////////////////////////////////////////////////////////////////////////

void Collection::fillIndexesDBServer() const {
  auto document = documentCollection();

  // lookup collection in agency by plan id
  auto clusterInfo = arangodb::ClusterInfo::instance();
  auto collectionInfo = clusterInfo->getCollection(
      std::string(vocbase->_name),
      arangodb::basics::StringUtils::itoa(document->_info.planId()));
  if (collectionInfo.get() == nullptr || (*collectionInfo).empty()) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_INTERNAL,
                                  "collection not found '%s' in database '%s'",
                                  name.c_str(), vocbase->_name);
  }

  std::shared_ptr<VPackBuilder> indexBuilder =
      arangodb::basics::JsonHelper::toVelocyPack(
          (*collectionInfo).getIndexes());
  VPackSlice const slice = indexBuilder->slice();
  if (!slice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unexpected indexes definition format");
  }

  size_t const n = static_cast<size_t>(slice.length());
  indexes.reserve(n);

  // register indexes
  for (auto const& v : VPackArrayIterator(slice)) {
    if (v.isObject()) {
      // lookup index id
      VPackSlice const id = v.get("id");

      if (!id.isString()) {
        // no "id" attribute. this is invalid
        continue;
      }

      VPackSlice const type = v.get("type");

      if (!type.isString()) {
        // no "type" attribute. this is invalid
        continue;
      }

      if (type.copyString() == "cap") {
        // ignore cap constraints
        continue;
      }

      // use numeric index id
      uint64_t iid = arangodb::basics::StringUtils::uint64(id.copyString());
      arangodb::Index* data = nullptr;

      auto const& allIndexes = document->allIndexes();
      size_t const n = allIndexes.size();

      // now check if we can find the local index and map it
      for (size_t j = 0; j < n; ++j) {
        auto localIndex = allIndexes[j];

        if (localIndex->id() == iid) {
          // found
          data = localIndex;
          break;
        }
      }

      auto idx = std::make_unique<arangodb::aql::Index>(v);
      // assign the found local index
      idx->setInternals(data, false);

      indexes.emplace_back(idx.get());
      idx.release();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index list, local server case
/// note: this will also be called for local collection on the DB server
////////////////////////////////////////////////////////////////////////////////

void Collection::fillIndexesLocal() const {
  // local collection
  auto const& allIndexes = documentCollection()->allIndexes();
  size_t const n = allIndexes.size();
  indexes.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    auto idx = std::make_unique<arangodb::aql::Index>(allIndexes[i]);
    indexes.emplace_back(idx.get());
    idx.release();
  }
}
