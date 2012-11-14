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

        SelfContainedTransaction (Collection* collection) :
          Transaction(collection->vocbase()), 
          _collection(collection) {
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

          // unuse underlying collection
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
/// @brief start a transaction
////////////////////////////////////////////////////////////////////////////////

        int begin () {
          if (_trx != 0) {
            // already started
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          // register usage of the underlying collection
          int res = _collection->use();
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          _trx = TRI_CreateTransaction(_vocbase->_transactionContext, TRI_TRANSACTION_READ_REPEATABLE, 0);
          if (_trx == 0) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }
  
          if (! TRI_AddCollectionTransaction(_trx, _collection->name().c_str(), type(), _collection->collection())) {
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

          if (type() == TRI_TRANSACTION_READ) {
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
/// @brief finish a transaction, based on the previous state
////////////////////////////////////////////////////////////////////////////////

        int finish (const int errorNumber) {
          if (_trx == 0) {
            // transaction already ended or not created
            return TRI_ERROR_NO_ERROR;
          }

          if (status() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          int res;
          if (errorNumber == TRI_ERROR_NO_ERROR) {
            // there was no previous error, so we'll commit
            res = commit();
          }
          else {
            // there was a previous error, so we'll abort
            abort();
            // return original error number
            res = errorNumber;
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document within a transaction
////////////////////////////////////////////////////////////////////////////////

        int read (TRI_doc_mptr_t** mptr,const string& key) {
          TRI_primary_collection_t* primary = _collection->primary();
          TRI_doc_operation_context_t context;

          TRI_InitReadContextPrimaryCollection(&context, primary);
          
          CollectionReadLock lock(_collection);

          return primary->read(&context, mptr, (TRI_voc_key_t) key.c_str());
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

// -----------------------------------------------------------------------------
// --SECTION--                                       virtual protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction type
////////////////////////////////////////////////////////////////////////////////

        virtual TRI_transaction_type_e type () const = 0;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the collection that is worked on
////////////////////////////////////////////////////////////////////////////////

        Collection* _collection;

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
