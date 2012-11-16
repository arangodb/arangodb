////////////////////////////////////////////////////////////////////////////////
/// @brief base transaction wrapper
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

#ifndef TRIAGENS_UTILS_TRANSACTION_H
#define TRIAGENS_UTILS_TRANSACTION_H 1

#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

#include "Logger/Logger.h"
#include "Utils/CollectionReadLock.h"
#include "Utils/CollectionWriteLock.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                 class Transaction
// -----------------------------------------------------------------------------

    template<typename T>
    class Transaction : public T {

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Transaction
////////////////////////////////////////////////////////////////////////////////

      private:
        Transaction (const Transaction&);
        Transaction& operator= (const Transaction&);

////////////////////////////////////////////////////////////////////////////////
/// @}
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
/// @brief create the transaction
////////////////////////////////////////////////////////////////////////////////

        Transaction (TRI_vocbase_t* const vocbase, 
                     const string& trxName) :
          T(), 
          _vocbase(vocbase), 
          _trxName(trxName),
          _hints(0),
          _setupError(TRI_ERROR_NO_ERROR) {
          assert(_vocbase != 0);

          int res = createTransaction();
          if (res != TRI_ERROR_NO_ERROR) {
            this->_setupError = res;
          }

#ifdef TRI_ENABLE_TRX
          LOGGER_INFO << "created transaction " << this->_trxName;
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction
////////////////////////////////////////////////////////////////////////////////

        virtual ~Transaction () {
          freeTransaction();
          
#ifdef TRI_ENABLE_TRX
          LOGGER_INFO << "destroyed transaction " << this->_trxName;
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief begin the transaction
////////////////////////////////////////////////////////////////////////////////
        
        int begin () {
          if (this->_setupError != TRI_ERROR_NO_ERROR) {
            return this->_setupError;
          }

          if (this->_trx == 0) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }
          
          int res;

          if (this->isEmbedded()) {
            res = addCollections();
            return res;
          }
          
          if (status() != TRI_TRANSACTION_CREATED) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }
          
          // register usage of the underlying collections
          res = useCollections();
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
  
          res = addCollections();
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
          
          return TRI_StartTransaction(this->_trx, _hints);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief commit / finish the transaction
////////////////////////////////////////////////////////////////////////////////

        int commit () {
          if (this->_trx == 0 || status() != TRI_TRANSACTION_RUNNING) {
            // not created or not running
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          if (this->isEmbedded()) {
            return TRI_ERROR_NO_ERROR;
          }

          int res;

          if (this->_trx->_type == TRI_TRANSACTION_READ) {
            res = TRI_FinishTransaction(this->_trx);
          }
          else {
            res = TRI_CommitTransaction(this->_trx);
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief abort the transaction
////////////////////////////////////////////////////////////////////////////////

        int abort () {
          if (this->_trx == 0) {
            // transaction not created
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          if (status() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          int res = TRI_AbortTransaction(this->_trx);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief finish a transaction, based on the previous state
////////////////////////////////////////////////////////////////////////////////

        int finish (const int errorNumber) {
          if (this->_trx == 0) {
            // transaction already not created
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
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
            this->abort();
            // return original error number
            res = errorNumber;
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name of the transaction
////////////////////////////////////////////////////////////////////////////////

        inline string name () const {
          return this->_trxName;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the "vocbase"
////////////////////////////////////////////////////////////////////////////////

        inline TRI_vocbase_t* vocbase () const {
          return this->_vocbase;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the status of the transaction
////////////////////////////////////////////////////////////////////////////////

        inline TRI_transaction_status_e status () const {
          if (this->_trx != 0) {
            return this->_trx->_status;
          }
          
          return TRI_TRANSACTION_UNDEFINED;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the transaction
////////////////////////////////////////////////////////////////////////////////

        void dump () {
          if (this->_trx != 0) {
            TRI_DumpTransaction(this->_trx);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief add a transaction hint
////////////////////////////////////////////////////////////////////////////////

        void addHint (const TRI_transaction_hint_e hint) {
          this->_hints |= (TRI_transaction_hint_t) hint;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a single document, identified by key
////////////////////////////////////////////////////////////////////////////////
        
        int readCollectionDocument (TRI_primary_collection_t* const primary, 
                                    TRI_doc_mptr_t** mptr,
                                    const string& key) {
          TRI_doc_operation_context_t context;
          TRI_InitReadContextPrimaryCollection(&context, primary);
          
          CollectionReadLock lock(primary);

          return primary->read(&context, mptr, (TRI_voc_key_t) key.c_str());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all documents
////////////////////////////////////////////////////////////////////////////////
        
        int readCollectionDocuments (TRI_primary_collection_t* const primary, 
                                     vector<string>& ids) {
          TRI_doc_operation_context_t context;
          TRI_InitReadContextPrimaryCollection(&context, primary);
          
          CollectionReadLock lock(primary);
          
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
/// @brief create a single document, using JSON 
////////////////////////////////////////////////////////////////////////////////

        int createCollectionDocument (TRI_primary_collection_t* const primary,
                                      const TRI_df_marker_type_e markerType,
                                      TRI_doc_mptr_t** mptr,
                                      TRI_json_t const* json, 
                                      void const* data,
                                      const bool forceSync) {
          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary, TRI_DOC_UPDATE_ERROR, forceSync);

          CollectionWriteLock lock(primary);

          return primary->createJson(&context, markerType, mptr, json, data);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single document, using shaped json
////////////////////////////////////////////////////////////////////////////////

        int createCollectionShaped (TRI_primary_collection_t* const primary,
                                    const TRI_df_marker_type_e markerType,
                                    TRI_voc_key_t key,
                                    TRI_doc_mptr_t** mptr,
                                    TRI_shaped_json_t const* shaped, 
                                    void const* data,
                                    const bool forceSync) {
          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary, TRI_DOC_UPDATE_ERROR, forceSync);

          CollectionWriteLock lock(primary);

          return primary->create(&context, markerType, mptr, shaped, data, key);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief update a single document, using JSON
////////////////////////////////////////////////////////////////////////////////
        
        int updateCollectionDocument (TRI_primary_collection_t* const primary,
                                      const string& key,
                                      TRI_doc_mptr_t** mptr,
                                      TRI_json_t* const json,
                                      const TRI_doc_update_policy_e policy,
                                      const TRI_voc_rid_t expectedRevision,
                                      TRI_voc_rid_t* actualRevision,
                                      const bool forceSync) {
          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);
          context._expectedRid = expectedRevision;
          context._previousRid = actualRevision;

          CollectionWriteLock lock(primary);

          return primary->updateJson(&context, mptr, json, (TRI_voc_key_t) key.c_str());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a single document
////////////////////////////////////////////////////////////////////////////////

        int deleteCollectionDocument (TRI_primary_collection_t* const primary,
                                      const string& key,
                                      const TRI_doc_update_policy_e policy,
                                      const TRI_voc_rid_t expectedRevision,
                                      TRI_voc_rid_t* actualRevision,
                                      const bool forceSync) {
          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);
          context._expectedRid = expectedRevision;
          context._previousRid = actualRevision;

          CollectionWriteLock lock(primary);

          return primary->destroy(&context, (TRI_voc_key_t) key.c_str());
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         virtual protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief use all collection
////////////////////////////////////////////////////////////////////////////////

        virtual int useCollections () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief add all collections to the transaction
////////////////////////////////////////////////////////////////////////////////

        virtual int addCollections () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////
      
      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief create transaction 
////////////////////////////////////////////////////////////////////////////////

        int createTransaction () {
          if (this->isEmbedded()) {
            if (! this->isEmbeddable()) {
              return TRI_ERROR_TRANSACTION_NESTED;
            }

            this->_trx = this->getParent();

            return TRI_ERROR_NO_ERROR;
          }

          this->_trx = TRI_CreateTransaction(_vocbase->_transactionContext, TRI_TRANSACTION_READ_REPEATABLE);
          if (this->_trx == 0) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          this->registerTransaction(this->_trx);

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief free transaction
////////////////////////////////////////////////////////////////////////////////

        int freeTransaction () {
          if (this->isEmbedded()) {
            return TRI_ERROR_NO_ERROR;
          }

          if (this->_trx != 0) { 
            this->unregisterTransaction();

            TRI_FreeTransaction(this->_trx);
            this->_trx = 0;
          }
          
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////
      
      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the vocbase
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* const _vocbase;

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

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction name
////////////////////////////////////////////////////////////////////////////////

        string _trxName;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction hints
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_hint_t _hints;

////////////////////////////////////////////////////////////////////////////////
/// @brief error that occurred on transaction initialisation (before begin())
////////////////////////////////////////////////////////////////////////////////

        int _setupError;
          
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
