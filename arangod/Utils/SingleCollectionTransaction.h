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

#include "common.h"

#include "VocBase/primary-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"
#include "BasicsC/voc-errors.h"

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

#include "Utils/Transaction.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                 class SingleCollectionTransaction
// -----------------------------------------------------------------------------

    template<bool E>
    class SingleCollectionTransaction : public Transaction<E> {

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

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
/// it), and may execute at most one write operation if it is a write 
/// transaction. It may execute multiple reads, though.
////////////////////////////////////////////////////////////////////////////////

        SingleCollectionTransaction (TRI_vocbase_t* const vocbase,
                                     TRI_transaction_t* previousTrx,
                                     const string& collectionName,
                                     const TRI_col_type_e collectionType,
                                     const bool createCollection, 
                                     const string& trxName,
                                     const TRI_transaction_type_e type) :
          Transaction<E>(vocbase, previousTrx, trxName), 
          _collectionName(collectionName),
          _collectionType(collectionType),
          _createCollection(createCollection),
          _type(type),
          _collection(0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        virtual ~SingleCollectionTransaction () {
          if (! this->isEmbedded()) {
            if (this->_trx != 0) {
              if (this->status() == TRI_TRANSACTION_RUNNING) {
                // auto abort
                this->abort();
              }
            }

            releaseCollection();
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                       virtual protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief use the underlying collection
////////////////////////////////////////////////////////////////////////////////

        int useCollections () {
          if (_collection != 0) {
            // we already used this collectino, nothing to do
            return TRI_ERROR_NO_ERROR;
          }

          if (_collectionName.empty()) {
            // name is empty. cannot open the collection
            return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
          }

          // open or create the collection
          if (isdigit(_collectionName[0])) {
            TRI_voc_cid_t id = triagens::basics::StringUtils::uint64(_collectionName);

            _collection = TRI_LookupCollectionByIdVocBase(this->_vocbase, id);
          }
          else {
            if (_collectionType == TRI_COL_TYPE_DOCUMENT) {
              _collection = TRI_FindDocumentCollectionByNameVocBase(this->_vocbase, _collectionName.c_str(), _createCollection);
            }
            else if (_collectionType == TRI_COL_TYPE_EDGE) {
              _collection = TRI_FindEdgeCollectionByNameVocBase(this->_vocbase, _collectionName.c_str(), _createCollection);
            }
          }
 
          if (_collection == 0) {
            // collection not found
            return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
          }

          int res = TRI_UseCollectionVocBase(this->_vocbase, const_cast<TRI_vocbase_col_s*>(_collection));

          if (res != TRI_ERROR_NO_ERROR) {
            _collection = 0;

            return res;
          }

          LOGGER_TRACE << "using collection " << _collectionName;
          assert(_collection->_collection != 0);

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add all collections to the transaction (only one)
////////////////////////////////////////////////////////////////////////////////

        int addCollections () {
          if (this->isEmbedded()) {
            assert(_collection == 0);

            _collection = TRI_CheckCollectionTransaction(this->_trx, _collectionName.c_str(), type());
            if (_collection == 0) {
              return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
            }

            return TRI_ERROR_NO_ERROR;
          }
          else {
            assert(_collection != 0);

            int res = TRI_AddCollectionTransaction(this->_trx, _collectionName.c_str(), type(), _collection);
            return res;
          }
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
          assert(_collection->_collection != 0);
          return _collection->_collection;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection's shaper
////////////////////////////////////////////////////////////////////////////////
        
        inline TRI_shaper_t* shaper () {
          return primaryCollection()->_shaper;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying collection's id
////////////////////////////////////////////////////////////////////////////////

        inline TRI_voc_cid_t cid () const {
          assert(_collection != 0);
          return _collection->_cid;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document within a transaction
////////////////////////////////////////////////////////////////////////////////

        int read (TRI_doc_mptr_t** mptr, const string& key) {
          TRI_primary_collection_t* const primary = primaryCollection();

          return this->readCollectionDocument(primary, mptr, key);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all documents within a transaction
////////////////////////////////////////////////////////////////////////////////

        int read (vector<string>& ids) {
          TRI_primary_collection_t* primary = primaryCollection();

          return this->readCollectionDocuments(primary, ids);
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief release the underlying collection
////////////////////////////////////////////////////////////////////////////////

        int releaseCollection () {
          // unuse underlying collection
          if (_collection != 0) {
            TRI_ReleaseCollectionVocBase(this->_vocbase, _collection);
            _collection = 0;
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction type
////////////////////////////////////////////////////////////////////////////////

        inline TRI_transaction_type_e type () const {
          return _type;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the collection that is worked on
////////////////////////////////////////////////////////////////////////////////

        const string _collectionName;

////////////////////////////////////////////////////////////////////////////////
/// @brief the type of the collection
////////////////////////////////////////////////////////////////////////////////

        const TRI_col_type_e _collectionType;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to create the collection
////////////////////////////////////////////////////////////////////////////////

        const bool _createCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction type (READ | WRITE)
////////////////////////////////////////////////////////////////////////////////

        const TRI_transaction_type_e _type;

////////////////////////////////////////////////////////////////////////////////
/// @brief data structure for the single collection used
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_col_t* _collection;

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
