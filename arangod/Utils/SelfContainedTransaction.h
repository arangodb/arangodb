////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for self-contained, single collection transactions
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

#ifndef TRIAGENS_UTILS_SELF_CONTAINED_TRANSACTION_H
#define TRIAGENS_UTILS_SELF_CONTAINED_TRANSACTION_H 1

#include "common.h"

#include "BasicsC/voc-errors.h"
#include "VocBase/primary-collection.h"

#include "Utils/Collection.h"
#include "Utils/CollectionReadLock.h"
#include "Utils/CollectionWriteLock.h"
#include "Utils/Transaction.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                    class SelfContainedTransaction
// -----------------------------------------------------------------------------

    class SelfContainedTransaction : public Transaction {

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
////////////////////////////////////////////////////////////////////////////////

        SelfContainedTransaction (Collection* collection, const TRI_transaction_type_e accessType) : 
          Transaction(collection->vocbase()), _collection(collection), _accessType(accessType), _synchronous(false) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        ~SelfContainedTransaction () {
          if (_trx != 0) {
            if (status() == TRI_TRANSACTION_RUNNING) {
              // auto abort
              abort();
            }
          }

          _collection->release();
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the last write in a transaction was synchronous
////////////////////////////////////////////////////////////////////////////////

        inline bool synchronous () const {
          return _synchronous;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief start a transaction
////////////////////////////////////////////////////////////////////////////////

        int begin () {
          if (_trx != 0) {
            // already started
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          int res = _collection->use();
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          _trx = TRI_CreateTransaction(_vocbase->_transactionContext, TRI_TRANSACTION_READ_REPEATABLE, 0);
          if (_trx == 0) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }
  
          if (! TRI_AddCollectionTransaction(_trx, _collection->name().c_str(), _accessType, _collection->collection())) {
            return TRI_ERROR_INTERNAL;
          }

          if (status() != TRI_TRANSACTION_CREATED) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          return TRI_StartTransaction(_trx);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////

        int commit () {
          if (_trx == 0 || status() != TRI_TRANSACTION_RUNNING) {
            // not created or not running
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          int res;

          if (_trx->_type == TRI_TRANSACTION_READ) {
            res = TRI_FinishTransaction(_trx);
          }
          else {
            res = TRI_CommitTransaction(_trx);
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief abort a transaction
////////////////////////////////////////////////////////////////////////////////

        int abort () {
          if (_trx == 0) {
            // transaction already ended or not created
            return TRI_ERROR_NO_ERROR;
          }

          if (status() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          int res = TRI_AbortTransaction(_trx);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single document within a transaction
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_mptr_t createDocument (TRI_json_t const* json, 
                                       bool forceSync) {
          TRI_primary_collection_t* primary = _collection->primary();
          TRI_doc_operation_context_t context;

          TRI_InitContextPrimaryCollection(&context, primary, TRI_DOC_UPDATE_ERROR, forceSync);
          _synchronous = context._sync;

          CollectionWriteLock lock(_collection);

          return primary->createJson(&context, TRI_DOC_MARKER_KEY_DOCUMENT, json, 0);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single edge within a transaction
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_mptr_t createEdge (TRI_json_t const* json, 
                                   bool forceSync, 
                                   void const* data) {
          TRI_primary_collection_t* primary = _collection->primary();
          TRI_doc_operation_context_t context;

          TRI_InitContextPrimaryCollection(&context, primary, TRI_DOC_UPDATE_ERROR, forceSync);
          _synchronous = context._sync;

          CollectionWriteLock lock(_collection);

          return primary->createJson(&context, TRI_DOC_MARKER_KEY_EDGE, json, data);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief update a single document within a transaction
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_mptr_t updateJson (const string& key, 
                                   TRI_json_t* const json, 
                                   const TRI_doc_update_policy_e policy, 
                                   bool forceSync, 
                                   const TRI_voc_rid_t expectedRevision, 
                                   TRI_voc_rid_t* actualRevision) {
          TRI_primary_collection_t* primary = _collection->primary();
          TRI_doc_operation_context_t context;

          TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);
          context._expectedRid = expectedRevision;
          context._previousRid = actualRevision;
          _synchronous = context._sync;

          CollectionWriteLock lock(_collection);

          return primary->updateJson(&context, json, (TRI_voc_key_t) key.c_str());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a single document within a transaction
////////////////////////////////////////////////////////////////////////////////

        int destroy (const string& key, 
                     const TRI_doc_update_policy_e policy, 
                     bool forceSync, 
                     const TRI_voc_rid_t expectedRevision, 
                     TRI_voc_rid_t* actualRevision) {
          TRI_primary_collection_t* primary = _collection->primary();
          TRI_doc_operation_context_t context;

          TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);
          context._expectedRid = expectedRevision;
          context._previousRid = actualRevision;
          _synchronous = context._sync;

          CollectionWriteLock lock(_collection);

          return primary->destroy(&context, (TRI_voc_key_t) key.c_str());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document within a transaction
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_mptr_t read (const string& key) {
          TRI_primary_collection_t* primary = _collection->primary();
          TRI_doc_operation_context_t context;

          TRI_InitReadContextPrimaryCollection(&context, primary);
          
          CollectionReadLock lock(_collection);

          return primary->read(&context, (TRI_voc_key_t) key.c_str());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all documents within a transaction
////////////////////////////////////////////////////////////////////////////////

        int read (vector<string>& ids) {
          TRI_primary_collection_t* primary = _collection->primary();
          TRI_doc_operation_context_t context;

          TRI_InitReadContextPrimaryCollection(&context, primary);
          
          CollectionReadLock lock(_collection);

          if (primary->_primaryIndex._nrUsed > 0) {
            void** ptr = primary->_primaryIndex._table;
            void** end = ptr + primary->_primaryIndex._nrAlloc;

            for (;  ptr < end;  ++ptr) {
              if (*ptr) {
                TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

                if (d->_validTo == 0) {
                  ids.push_back(d->_key);
                }
              }
            }
          }

          return TRI_ERROR_NO_ERROR;
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
/// @brief the collection that is worked on
////////////////////////////////////////////////////////////////////////////////

        Collection* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief the access type for the collection (read | write)
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_type_e _accessType;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether of not the last write action was executed synchronously
////////////////////////////////////////////////////////////////////////////////

        bool _synchronous;

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
