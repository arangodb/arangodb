////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for single collection transactions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILS_SINGLE_COLLECTION_TRANSACTION_H
#define TRIAGENS_UTILS_SINGLE_COLLECTION_TRANSACTION_H 1

#include "BasicsC/common.h"

#include "VocBase/barrier.h"
#include "VocBase/primary-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"
#include "BasicsC/voc-errors.h"

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

#include "Utils/Transaction.h"

namespace triagens {
  namespace arango {

    template<typename T>
    class SingleCollectionTransaction : public Transaction<T> {

// -----------------------------------------------------------------------------
// --SECTION--                                 class SingleCollectionTransaction
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction, using a collection object
///
/// A single collection transaction operates on a single collection (you guessed
/// it)
////////////////////////////////////////////////////////////////////////////////

        SingleCollectionTransaction (TRI_vocbase_t* const vocbase,
                                     const triagens::arango::CollectionNameResolver& resolver,
                                     const TRI_transaction_cid_t cid,
                                     const TRI_transaction_type_e accessType) :
          Transaction<T>(vocbase, resolver), 
          _cid(cid) {

          this->addCollection(cid, accessType);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        virtual ~SingleCollectionTransaction () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying primary collection
////////////////////////////////////////////////////////////////////////////////

        inline TRI_primary_collection_t* primaryCollection () {
          assert(this->_cid > 0);
          TRI_vocbase_col_t* collection = TRI_LookupCollectionByIdVocBase(this->_vocbase, this->_cid); 

          assert(collection != 0);
          assert(collection->_collection != 0);
          return collection->_collection;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying collection's id
////////////////////////////////////////////////////////////////////////////////

        inline TRI_voc_cid_t cid () {
          return this->_cid;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection's shaper
////////////////////////////////////////////////////////////////////////////////
        
        inline TRI_shaper_t* shaper () {
          return primaryCollection()->_shaper;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection's barrier list
////////////////////////////////////////////////////////////////////////////////
        
        inline TRI_barrier_list_t* barrierList () {
          return &primaryCollection()->_barrierList;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read any (random) document within a transaction
////////////////////////////////////////////////////////////////////////////////

        int read (TRI_doc_mptr_t** mptr, TRI_barrier_t** barrier) {
          TRI_primary_collection_t* const primary = primaryCollection();

          return this->readCollectionAny(primary, mptr, barrier);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document within a transaction
////////////////////////////////////////////////////////////////////////////////

        int read (TRI_doc_mptr_t** mptr, const string& key) {
          TRI_primary_collection_t* const primary = primaryCollection();

          return this->readCollectionDocument(primary, mptr, key);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all document ids within a transaction
////////////////////////////////////////////////////////////////////////////////

        int read (vector<string>& ids) {
          TRI_primary_collection_t* primary = primaryCollection();

          return this->readCollectionDocuments(primary, ids);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all documents within a transaction, using skip and limit
////////////////////////////////////////////////////////////////////////////////

        int read (vector<TRI_doc_mptr_t*>& docs, 
                  TRI_barrier_t** barrier,
                  TRI_voc_ssize_t skip, 
                  TRI_voc_size_t limit,
                  uint32_t* total) {
          TRI_primary_collection_t* primary = primaryCollection();

          return this->readCollectionPointers(primary, docs, barrier, skip, limit, total);
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

        TRI_transaction_cid_t _cid;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
