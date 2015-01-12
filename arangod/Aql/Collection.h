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

#ifndef ARANGODB_AQL_COLLECTION_H
#define ARANGODB_AQL_COLLECTION_H 1

#include "Basics/Common.h"
#include "Aql/Index.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                 struct Collection
// -----------------------------------------------------------------------------

    struct Collection {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      Collection& operator= (Collection const&) = delete;
      Collection (Collection const&) = delete;
      Collection () = delete;
      
      Collection (std::string const&,
                  struct TRI_vocbase_s*,
                  TRI_transaction_type_e);
      
      ~Collection ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current shard 
////////////////////////////////////////////////////////////////////////////////

      inline void setCurrentShard (std::string const& shard) {
        currentShard = shard;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the current shard
////////////////////////////////////////////////////////////////////////////////
      
      inline void resetCurrentShard () {
        currentShard = "";
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the collection id
////////////////////////////////////////////////////////////////////////////////

      inline TRI_voc_cid_t cid () const {
        TRI_ASSERT(collection != nullptr);
        return collection->_cid;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name of the collection, translated for the sharding
/// case. this will return currentShard if it is set, and name otherwise
////////////////////////////////////////////////////////////////////////////////

      std::string getName () const {
        if (! currentShard.empty()) {
          // sharding case: return the current shard name instead of the collection name
          return currentShard;
        }

        // non-sharding case: simply return the name
        return name;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the pointer to the document collection
////////////////////////////////////////////////////////////////////////////////
        
      inline TRI_document_collection_t* documentCollection () const {
        TRI_ASSERT(collection != nullptr);
        TRI_ASSERT(collection->_collection != nullptr);

        return collection->_collection;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the collection is an edge collection
////////////////////////////////////////////////////////////////////////////////

      inline bool isEdgeCollection () const {
        auto document = documentCollection();
        return (document->_info._type == TRI_COL_TYPE_EDGE);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the collection is a document collection
////////////////////////////////////////////////////////////////////////////////
      
      inline bool isDocumentCollection () const {
        auto document = documentCollection();
        return (document->_info._type == TRI_COL_TYPE_DOCUMENT);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief count the LOCAL number of documents in the collection
////////////////////////////////////////////////////////////////////////////////

      size_t count () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection's plan id
////////////////////////////////////////////////////////////////////////////////

      TRI_voc_cid_t getPlanId() const;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard ids of a collection
////////////////////////////////////////////////////////////////////////////////

      std::vector<std::string> shardIds () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard keys of a collection
////////////////////////////////////////////////////////////////////////////////

      std::vector<std::string> shardKeys () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the indexes of the collection
////////////////////////////////////////////////////////////////////////////////

      std::vector<Index*> getIndexes (); 

////////////////////////////////////////////////////////////////////////////////
/// @brief return an index by its id
////////////////////////////////////////////////////////////////////////////////
  
      Index* getIndex (TRI_idx_iid_t) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return an index by its id
////////////////////////////////////////////////////////////////////////////////
  
      Index* getIndex (std::string const&) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the collection uses the default sharding
////////////////////////////////////////////////////////////////////////////////

      bool usesDefaultSharding () const;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

    private:

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index list for the collection
////////////////////////////////////////////////////////////////////////////////

      void fillIndexes () const;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
    
    private:

      std::string                  currentShard;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

    public:

      std::string const            name;
      TRI_vocbase_t*               vocbase;
      TRI_vocbase_col_t*           collection;
      TRI_transaction_type_e       accessType;
      std::vector<Index*> mutable  indexes;
      int64_t mutable              numDocuments = UNINITIALIZED;

      static int64_t const         UNINITIALIZED = -1;
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
