////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, collection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/StringUtils.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

using namespace triagens::aql;
      
// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection wrapper
////////////////////////////////////////////////////////////////////////////////

Collection::Collection (std::string const& name,
                        struct TRI_vocbase_s* vocbase,
                        TRI_transaction_type_e accessType) 
  : currentShard(),
    name(name),
    vocbase(vocbase),
    collection(nullptr),
    accessType(accessType),
    isReadWrite(false) {
          
  TRI_ASSERT(! name.empty());
  TRI_ASSERT(vocbase != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a collection wrapper
////////////////////////////////////////////////////////////////////////////////
      
Collection::~Collection () {
  for (auto idx : indexes) {
    delete idx;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief count the number of documents in the collection
////////////////////////////////////////////////////////////////////////////////

size_t Collection::count () const {
  if (numDocuments == UNINITIALIZED) {
    if (triagens::arango::ServerState::instance()->isCoordinator()) {
      // cluster case
      uint64_t result;
      int res = triagens::arango::countOnCoordinator(vocbase->_name, name, result); 
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION_MESSAGE(res, "could not determine number of documents in collection");
      }
      numDocuments = static_cast<int64_t>(result);
    }
    else {
      // local case
      auto document = documentCollection();
      // cache the result
      numDocuments = static_cast<int64_t>(document->size(document));
    }
  }

  return static_cast<size_t>(numDocuments);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection's plan id
////////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t Collection::getPlanId () const {
  auto clusterInfo = triagens::arango::ClusterInfo::instance();
  auto collectionInfo = clusterInfo->getCollection(std::string(vocbase->_name), name);

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

std::vector<std::string> Collection::shardIds () const {
  auto clusterInfo = triagens::arango::ClusterInfo::instance();
  auto collectionInfo = clusterInfo->getCollection(std::string(vocbase->_name), name);
  if (collectionInfo.get() == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_INTERNAL, 
                                  "collection not found '%s' -> '%s'",
                                  vocbase->_name, name.c_str());
  }

  std::vector<std::string> ids;
  for (auto const& it : collectionInfo.get()->shardIds()) {
    ids.emplace_back(it.first);
  }
  return ids;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard keys of a collection
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Collection::shardKeys () const {
  auto clusterInfo = triagens::arango::ClusterInfo::instance();
  auto collectionInfo = clusterInfo->getCollection(std::string(vocbase->_name), name);
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

std::vector<Index*> Collection::getIndexes () {
  fillIndexes();

  return indexes;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an index by its id
////////////////////////////////////////////////////////////////////////////////
  
Index* Collection::getIndex (TRI_idx_iid_t id) const {
  fillIndexes();

  for (auto idx : indexes) {
    if (idx->id == id) {
      return idx;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an index by its id
////////////////////////////////////////////////////////////////////////////////
  
Index* Collection::getIndex (std::string const& id) const {
  return getIndex(triagens::basics::StringUtils::uint64(id));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the collection uses the default sharding
////////////////////////////////////////////////////////////////////////////////

bool Collection::usesDefaultSharding () const {
  // check if collection shard keys are only _key
  std::vector<std::string>&& sk = shardKeys();
  if (sk.size() != 1 || sk[0] != TRI_VOC_ATTRIBUTE_KEY) {
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index list for the collection
////////////////////////////////////////////////////////////////////////////////

void Collection::fillIndexes () const {
  if (! indexes.empty()) {
    return;
  }

  if (triagens::arango::ServerState::instance()->isCoordinator()) {
    fillIndexesCoordinator();
    return;
  }
  
  // must have a collection  
  TRI_ASSERT(collection != nullptr);

  if (triagens::arango::ServerState::instance()->isDBserver() && 
      documentCollection()->_info._planId > 0) {
    fillIndexesDBServer();
    return;
  }

  fillIndexesLocal();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index list, cluster coordinator case
////////////////////////////////////////////////////////////////////////////////

void Collection::fillIndexesCoordinator () const {
  // coordinator case, remote collection
  auto clusterInfo = triagens::arango::ClusterInfo::instance();
  auto collectionInfo = clusterInfo->getCollection(std::string(vocbase->_name), name);
  if (collectionInfo.get() == nullptr || (*collectionInfo).empty()) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_INTERNAL, 
                                  "collection not found '%s' in database '%s'",
                                  name.c_str(), vocbase->_name);
  }

  TRI_json_t const* json = (*collectionInfo).getIndexes();

  if (TRI_IsArrayJson(json)) {
    size_t const n = TRI_LengthArrayJson(json);
    indexes.reserve(n);

    for (size_t i = 0; i < n; ++i) {
      TRI_json_t const* v = TRI_LookupArrayJson(json, i);
      if (v != nullptr) {
        indexes.emplace_back(new Index(v));
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index list, cluster DB server case
////////////////////////////////////////////////////////////////////////////////

void Collection::fillIndexesDBServer () const {
  auto document = documentCollection();

  // lookup collection in agency by plan id  
  auto clusterInfo = triagens::arango::ClusterInfo::instance();
  auto collectionInfo = clusterInfo->getCollection(std::string(vocbase->_name), triagens::basics::StringUtils::itoa(document->_info._planId));
  if (collectionInfo.get() == nullptr || (*collectionInfo).empty()) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_INTERNAL, 
                                  "collection not found '%s' in database '%s'",
                                  name.c_str(), vocbase->_name);
  }

  TRI_json_t const* json = (*collectionInfo).getIndexes();
  if (! TRI_IsArrayJson(json)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected indexes definition format");
  }

  size_t const n = TRI_LengthArrayJson(json);
  indexes.reserve(n);
    
  // register indexes
  for (size_t i = 0; i < n; ++i) {
    TRI_json_t const* v = TRI_LookupArrayJson(json, i);
    if (TRI_IsObjectJson(v)) {
      // lookup index id
      TRI_json_t const* id = TRI_LookupObjectJson(v, "id");
      if (! TRI_IsStringJson(id)) {
        continue;
      }

      // use numeric index id
      uint64_t iid = triagens::basics::StringUtils::uint64(id->_value._string.data, id->_value._string.length - 1);
      TRI_index_t* data = nullptr;

      // now check if we can find the local index and map it
      for (size_t j = 0; j < document->_allIndexes._length; ++j) {
        auto localIndex = static_cast<TRI_index_t*>(document->_allIndexes._buffer[j]);

        if (localIndex != nullptr) {
          if (localIndex->_iid == iid) {
            // found
            data = localIndex;
            break;
          }
          else if (localIndex->_type == TRI_IDX_TYPE_PRIMARY_INDEX || 
                   localIndex->_type == TRI_IDX_TYPE_EDGE_INDEX) {
          }
        }
      }

      auto idx = new Index(v);
      // assign the found local index
      idx->setInternals(data);

      try {
        indexes.emplace_back(idx);
      }
      catch (...) {
        delete idx;
        throw;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index list, local server case
/// note: this will also be called for local collection on the DB server
////////////////////////////////////////////////////////////////////////////////

void Collection::fillIndexesLocal () const {
  // local collection
  auto document = documentCollection();
  size_t const n = document->_allIndexes._length;
  indexes.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    indexes.emplace_back(new Index(static_cast<TRI_index_t*>(document->_allIndexes._buffer[i])));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
