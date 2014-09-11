////////////////////////////////////////////////////////////////////////////////
/// @brief collection name resolver
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

#ifndef ARANGODB_UTILS_COLLECTION_NAME_RESOLVER_H
#define ARANGODB_UTILS_COLLECTION_NAME_RESOLVER_H 1

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
          _vocbase(vocbase),
          _resolvedNames(),
          _resolvedIds() {
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

        TRI_vocbase_col_t const* getCollectionStruct (std::string const& name) const {
          if (! _resolvedNames.empty()) {
            auto it = _resolvedNames.find(name);

            if (it != _resolvedNames.end()) {
              return (*it).second;
            }
          }

          TRI_vocbase_col_t const* collection = TRI_LookupCollectionByNameVocBase(_vocbase, name.c_str());

          if (collection != nullptr) {
            _resolvedNames.emplace(std::make_pair(name, collection));
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

        std::string getCollectionName (TRI_voc_cid_t cid) const {
          if (! _resolvedIds.empty()) {
            auto it = _resolvedIds.find(cid);

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
              if (found->_planId == 0) {
                // DBserver local case
                char* n = TRI_GetCollectionNameByIdVocBase(_vocbase, cid);
                if (0 != n) {
                  name = n;
                  TRI_Free(TRI_UNKNOWN_MEM_ZONE, n);
                }
              }
              else {
                // DBserver case of a shard:
                name = triagens::basics::StringUtils::itoa(found->_planId);
                shared_ptr<CollectionInfo> ci
                  = ClusterInfo::instance()->getCollection(found->_dbName,name);
                name = ci->name();   // can be empty, if collection unknown
              }
            }

            TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(_vocbase);
          }
          else {
            // exactly as in the non-cluster case
            char* n = TRI_GetCollectionNameByIdVocBase(_vocbase, cid);
            if (nullptr != n) {
              name = n;
              TRI_Free(TRI_UNKNOWN_MEM_ZONE, n);
            }
          }
          if (name.empty()) {
            name = "_unknown";
          }

          _resolvedIds.emplace(std::make_pair(cid, name));

          return name;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a collection name for a collection id, this implements
/// some magic in the cluster case: a DBserver in a cluster will automatically
/// translate the local collection ID into a cluster wide collection name.
///
/// the name is copied into <buffer>. the caller is responsible for allocating
/// a big-enough buffer (that is, at least 64 bytes). no NUL byte is appended
/// to the buffer. the length of the collection name is returned.
////////////////////////////////////////////////////////////////////////////////

        size_t getCollectionName (char* buffer, 
                                  TRI_voc_cid_t cid) const {
          if (! _resolvedIds.empty()) {
            auto it = _resolvedIds.find(cid);

            if (it != _resolvedIds.end()) {
              memcpy(buffer, (*it).second.c_str(), (*it).second.size()); 
              return (*it).second.size();
            }
          }

          std::string&& name(getCollectionName(cid));

          memcpy(buffer, name.c_str(), name.size());
          return name.size();
        }


////////////////////////////////////////////////////////////////////////////////
/// @brief look up a cluster-wide collection name for a cluster-wide
/// collection id
////////////////////////////////////////////////////////////////////////////////

        std::string getCollectionNameCluster (TRI_voc_cid_t cid) const {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a cluster-wide collection name for a cluster-wide
/// collection id
///
/// the name is copied into <buffer>. the caller is responsible for allocating
/// a big-enough buffer (that is, at least 64 bytes). no NUL byte is appended
/// to the buffer. the length of the collection name is returned.
////////////////////////////////////////////////////////////////////////////////

        size_t getCollectionNameCluster (char* buffer,
                                         TRI_voc_cid_t cid) const {
          if (! ServerState::instance()->isRunningInCluster()) {
            return getCollectionName(buffer, cid);
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
            memcpy(buffer, name.c_str(), name.size());
            return name.size();
          }

          memcpy(buffer, "_unknown", strlen("_unknown"));
          return strlen("_unknown");
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
