////////////////////////////////////////////////////////////////////////////////
/// @brief collection name resolver
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILS_COLLECTION_NAME_RESOLVER_H
#define TRIAGENS_UTILS_COLLECTION_NAME_RESOLVER_H 1

#include "Basics/Common.h"

#include "Basics/StringUtils.h"
#include "VocBase/vocbase.h"

#include "Cluster/ServerState.h"
#include "Cluster/ClusterInfo.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                      class CollectionNameResolver
// -----------------------------------------------------------------------------

    class CollectionNameResolver {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the resolver
////////////////////////////////////////////////////////////////////////////////

        CollectionNameResolver (TRI_vocbase_t* vocbase) :
          _vocbase(vocbase), _resolvedNames(), _resolvedIds() {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the resolver
////////////////////////////////////////////////////////////////////////////////

        ~CollectionNameResolver () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a collection id for a collection name (local case)
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_cid_t getCollectionId (std::string const& name) const {
          if (name[0] >= '0' && name[0] <= '9') {
            // name is a numeric id
            return (TRI_voc_cid_t) triagens::basics::StringUtils::uint64(name);
          }

          TRI_vocbase_col_t const* collection = getCollectionStruct(name);

          if (collection != nullptr) {
            return collection->_cid;
          }
          return 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a collection struct for a collection name
////////////////////////////////////////////////////////////////////////////////

        const TRI_vocbase_col_t* getCollectionStruct (std::string const& name) const {
          std::unordered_map<std::string, TRI_vocbase_col_t const*>::iterator it;

          if (! _resolvedNames.empty()) {
            it = _resolvedNames.find(name);

            if (it != _resolvedNames.end()) {
              return (*it).second;
            }
          }

          TRI_vocbase_col_t const* collection = TRI_LookupCollectionByNameVocBase(_vocbase, name.c_str());

          if (collection != nullptr) {
            _resolvedNames.insert(it, std::make_pair(name, collection));
          }

          return collection;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a cluster collection id for a cluster collection name
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_cid_t getCollectionIdCluster (std::string const& name) const {
          if (! ServerState::instance()->isRunningInCluster()) {
            return getCollectionId(name);
          }
          if (name[0] >= '0' && name[0] <= '9') {
            // name is a numeric id
            return (TRI_voc_cid_t) triagens::basics::StringUtils::uint64(name);
          }

          // We have to look up the collection info:
          ClusterInfo* ci = ClusterInfo::instance();
          shared_ptr<CollectionInfo> cinfo 
            = ci->getCollection(DatabaseID(_vocbase->_name),
                                name);
          if (cinfo->empty()) {
            return 0;
          }
          return cinfo->id();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a collection name for a collection id, this implements
/// some magic in the cluster case: a DBserver in a cluster will automatically
/// translate the local collection ID into a cluster wide collection name.
////////////////////////////////////////////////////////////////////////////////

        std::string getCollectionName (const TRI_voc_cid_t cid) const {
          std::unordered_map<TRI_voc_cid_t, std::string>::iterator it;

          if (! _resolvedIds.empty()) {
            it = _resolvedIds.find(cid);

            if (it != _resolvedIds.end()) {
              return (*it).second;
            }
          }

          std::string name;
          if (ServerState::instance()->isDBserver()) {
            TRI_READ_LOCK_COLLECTIONS_VOCBASE(_vocbase);

            TRI_vocbase_col_t* found 
                = static_cast<TRI_vocbase_col_t*>(
                     TRI_LookupByKeyAssociativePointer
                                   (&_vocbase->_collectionsById, &cid));

            if (nullptr != found) {
              name = triagens::basics::StringUtils::itoa(found->_planId);
            }

            TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(_vocbase);

            if (! name.empty()) {
              shared_ptr<CollectionInfo> ci
                = ClusterInfo::instance()->getCollection(found->_dbName, name);
              name = ci->name();
            }
          }
          else {
            // exactly as in the non-cluster case
            char *n = TRI_GetCollectionNameByIdVocBase(_vocbase, cid);
            if (0 != n) {
              name = n;
              TRI_Free(TRI_UNKNOWN_MEM_ZONE, n);
            }
          }
          if (name.empty()) {
            name = "_unknown";
          }

          _resolvedIds.insert(it, std::make_pair(cid, name));

          return name;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a cluster-wide collection name for a cluster-wide 
/// collection id
////////////////////////////////////////////////////////////////////////////////

        std::string getCollectionNameCluster (const TRI_voc_cid_t cid) const {
          if (! ServerState::instance()->isRunningInCluster()) {
            return getCollectionName(cid);
          }

          int tries = 0;

          while (tries++ < 2) {
            shared_ptr<CollectionInfo> ci
              = ClusterInfo::instance()->getCollection(_vocbase->_name, 
                             triagens::basics::StringUtils::itoa(cid));
            std::string name = ci->name();

            if (name.empty()) {
              ClusterInfo::instance()->flush();
              continue;
            }
            return name;
          }

          return "_unknown";
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase base pointer
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection id => collection struct map
////////////////////////////////////////////////////////////////////////////////

        mutable std::unordered_map<std::string, TRI_vocbase_col_t const*> _resolvedNames;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection id => collection name map
////////////////////////////////////////////////////////////////////////////////

        mutable std::unordered_map<TRI_voc_cid_t, std::string> _resolvedIds;

    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
