////////////////////////////////////////////////////////////////////////////////
/// @brief base transaction wrapper
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILS_TRANSACTION_H
#define TRIAGENS_UTILS_TRANSACTION_H 1

#include "VocBase/barrier.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/update-policy.h"
#include "VocBase/vocbase.h"

#include "BasicsC/random.h"
#include "BasicsC/strings.h"

#include "Logger/Logger.h"
#include "Utils/CollectionNameResolver.h"

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
                     const triagens::arango::CollectionNameResolver& resolver) :
          T(),
          _trx(0),
          _setupState(TRI_ERROR_NO_ERROR),
          _nestingLevel(0),
          _readOnly(true),
          _hints(0),
          _vocbase(vocbase),
          _resolver(resolver) {

          TRI_ASSERT_DEBUG(_vocbase != 0);

          this->setupTransaction();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction
////////////////////////////////////////////////////////////////////////////////

        virtual ~Transaction () {
          if (_trx == 0) {
            return;
          }
          
          if (isEmbeddedTransaction()) {
            _trx->_nestingLevel--;
          }
          else {
            if (getStatus() == TRI_TRANSACTION_RUNNING) {
              // auto abort a running transaction
              this->abort();
            }

            // free the data associated with the transaction
            freeTransaction();
          }
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
/// @brief whether or not the transaction is embedded
////////////////////////////////////////////////////////////////////////////////

        inline bool isEmbeddedTransaction () const {
          return (_nestingLevel > 0);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether or not the transaction is read-only
////////////////////////////////////////////////////////////////////////////////

        inline bool isReadOnlyTransaction () const {
          return _readOnly;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the status of the transaction
////////////////////////////////////////////////////////////////////////////////

        inline TRI_transaction_status_e getStatus () const {
          if (_trx != 0) {
            return _trx->_status;
          }

          return TRI_TRANSACTION_UNDEFINED;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief begin the transaction
////////////////////////////////////////////////////////////////////////////////

        int begin () {
          if (_trx == 0) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }
          
          if (_setupState != TRI_ERROR_NO_ERROR) {
            return _setupState;
          }

          int res = TRI_BeginTransaction(_trx, _hints, _nestingLevel);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief commit / finish the transaction
////////////////////////////////////////////////////////////////////////////////

        int commit () {
          if (_trx == 0 || getStatus() != TRI_TRANSACTION_RUNNING) {
            // transaction not created or not running
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          int res = TRI_CommitTransaction(_trx, _nestingLevel);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief abort the transaction
////////////////////////////////////////////////////////////////////////////////

        int abort () {
          if (_trx == 0 || getStatus() != TRI_TRANSACTION_RUNNING) {
            // transaction not created or not running
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          int res = TRI_AbortTransaction(_trx, _nestingLevel);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief finish a transaction (commit or abort), based on the previous state
////////////////////////////////////////////////////////////////////////////////

        int finish (const int errorNum) {
          int res;

          if (errorNum == TRI_ERROR_NO_ERROR) {
            // there was no previous error, so we'll commit
            res = this->commit();
          }
          else {
            // there was a previous error, so we'll abort
            this->abort();

            // return original error number
            res = errorNum;
          }
          
          return res;
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
/// @brief add a collection by id
////////////////////////////////////////////////////////////////////////////////

        int addCollection (TRI_transaction_cid_t cid,
                           TRI_transaction_type_e type) {
          if (_trx == 0) {
            return registerError(TRI_ERROR_INTERNAL);
          }

          if (cid == 0) {
            // invalid cid
            return registerError(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
          }

          const TRI_transaction_status_e status = getStatus();
              
          if (status == TRI_TRANSACTION_COMMITTED ||
              status == TRI_TRANSACTION_ABORTED) {
            // transaction already finished?
            return registerError(TRI_ERROR_TRANSACTION_INVALID_STATE);
          }

          int res;

          if (this->isEmbeddedTransaction()) {
            res = this->addCollectionEmbedded(cid, type);
          }
          else {
            res = this->addCollectionToplevel(cid, type);
          }
          
          if (type == TRI_TRANSACTION_WRITE) {
            _readOnly = false;
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by name
////////////////////////////////////////////////////////////////////////////////

        int addCollection (const string& name,
                           TRI_transaction_type_e type) {

          return addCollection(_resolver.getCollectionId(name), type);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a transaction hint
////////////////////////////////////////////////////////////////////////////////

        void addHint (const TRI_transaction_hint_e hint) {
          _hints |= (TRI_transaction_hint_t) hint;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-lock a collection
////////////////////////////////////////////////////////////////////////////////

        int lockExplicit (TRI_primary_collection_t* const primary,
                          const TRI_transaction_type_e type) {
          if (_trx == 0 || getStatus() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          int res = TRI_LockCollectionTransaction(_trx, (TRI_transaction_cid_t) primary->base._info._cid, type, _nestingLevel);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-unlock a collection
////////////////////////////////////////////////////////////////////////////////

        int unlockExplicit (TRI_primary_collection_t* const primary,
                            const TRI_transaction_type_e type) {
          if (_trx == 0 || getStatus() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          int res = TRI_UnlockCollectionTransaction(_trx, (TRI_transaction_cid_t) primary->base._info._cid, type, _nestingLevel);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-unlock a collection
////////////////////////////////////////////////////////////////////////////////

        bool isLocked (TRI_primary_collection_t* const primary,
                       const TRI_transaction_type_e type) {
          if (_trx == 0 || getStatus() != TRI_TRANSACTION_RUNNING) {
            return false;
          }

          const bool locked = TRI_IsLockedCollectionTransaction(_trx, (TRI_transaction_cid_t) primary->base._info._cid, type, _nestingLevel);

          return locked;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read any (random) document
////////////////////////////////////////////////////////////////////////////////

        int readCollectionAny (TRI_primary_collection_t* const primary,
                               TRI_doc_mptr_t* mptr,
                               TRI_barrier_t** barrier) {
          *barrier = TRI_CreateBarrierElement(&primary->_barrierList);
          if (*barrier == 0) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          // READ-LOCK START
          this->lockExplicit(primary, TRI_TRANSACTION_READ);

          if (primary->_primaryIndex._nrUsed == 0) {
            TRI_FreeBarrier(*barrier);
            *barrier = 0;

            // no document found
            mptr->_key = 0;
            mptr->_data = 0;
          }
          else {
            size_t total = primary->_primaryIndex._nrAlloc;
            size_t pos = TRI_UInt32Random() % total;
            void** beg = primary->_primaryIndex._table;

            while (beg[pos] == 0 || (((TRI_doc_mptr_t*) beg[pos])->_validTo != 0)) {
              pos = (pos + 1) % total;
            }

            *mptr = *((TRI_doc_mptr_t*) beg[pos]);
          }

          this->unlockExplicit(primary, TRI_TRANSACTION_READ);
          // READ-LOCK END

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a single document, identified by key
////////////////////////////////////////////////////////////////////////////////

        int readCollectionDocument (TRI_primary_collection_t* const primary,
                                    TRI_doc_mptr_t* mptr,
                                    const string& key,
                                    const bool lock) {
          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary);

          int res = primary->read(&context, 
                                  (TRI_voc_key_t) key.c_str(), 
                                  mptr,
                                  lock && ! isLocked(primary, TRI_TRANSACTION_READ));

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all documents
////////////////////////////////////////////////////////////////////////////////

        int readCollectionDocuments (TRI_primary_collection_t* const primary,
                                     vector<string>& ids) {
          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary);

          // READ-LOCK START
          this->lockExplicit(primary, TRI_TRANSACTION_READ);

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

          this->unlockExplicit(primary, TRI_TRANSACTION_READ);
          // READ-LOCK END

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all master pointers, using skip and limit
////////////////////////////////////////////////////////////////////////////////

        int readCollectionPointers (TRI_primary_collection_t* const primary,
                                    vector<TRI_doc_mptr_t>& docs,
                                    TRI_barrier_t** barrier,
                                    TRI_voc_ssize_t skip,
                                    TRI_voc_size_t limit,
                                    uint32_t* total) {
          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary);

          if (limit == 0) {
            // nothing to do
            return TRI_ERROR_NO_ERROR;
          }

          // READ-LOCK START
          this->lockExplicit(primary, TRI_TRANSACTION_READ);

          if (primary->_primaryIndex._nrUsed == 0) {
            // nothing to do

            this->unlockExplicit(primary, TRI_TRANSACTION_READ);
            // READ-LOCK END
            return TRI_ERROR_NO_ERROR;
          }

          *barrier = TRI_CreateBarrierElement(&primary->_barrierList);
          if (*barrier == 0) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          void** beg = primary->_primaryIndex._table;
          void** ptr = beg;
          void** end = ptr + primary->_primaryIndex._nrAlloc;
          uint32_t count = 0;
          // TODO: this is not valid in MVCC context
          *total = (uint32_t) primary->_primaryIndex._nrUsed;

          // apply skip
          if (skip > 0) {
            // skip from the beginning
            for (;  ptr < end && 0 < skip;  ++ptr) {
              if (*ptr) {
                TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

                if (d->_validTo == 0) {
                  --skip;
                }
              }
            }
          }
          else if (skip < 0) {
            // skip from the end
            ptr = end - 1;

            for (; beg <= ptr; --ptr) {
              if (*ptr) {
                TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

                if (d->_validTo == 0) {
                  ++skip;

                  if (skip == 0) {
                    break;
                  }
                }
              }
            }

            if (ptr < beg) {
              ptr = beg;
            }
          }

          // fetch documents, taking limit into account
          for (; ptr < end && count < limit; ++ptr) {
            if (*ptr) {
              TRI_doc_mptr_t* d = (TRI_doc_mptr_t*) *ptr;

              if (d->_validTo == 0) {
                docs.push_back(*d);
                ++count;
              }
            }
          }

          this->unlockExplicit(primary, TRI_TRANSACTION_READ);
          // READ-LOCK END

          if (count == 0) {
            // barrier not needed, kill it
            TRI_FreeBarrier(*barrier);
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single document, using JSON
////////////////////////////////////////////////////////////////////////////////

        int createCollectionDocument (TRI_primary_collection_t* const primary,
                                      const TRI_df_marker_type_e markerType,
                                      TRI_doc_mptr_t* mptr,
                                      TRI_json_t const* json,
                                      void const* data,
                                      const bool forceSync,
                                      const bool lock) {
          TRI_voc_key_t key = 0;
          int res = this->getKey(json, &key);

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(primary->_shaper, json);

          if (shaped == 0) {
            return TRI_ERROR_ARANGO_SHAPER_FAILED;
          }

          res = createCollectionShaped(primary, 
                                       markerType, 
                                       key, 
                                       mptr, 
                                       shaped, 
                                       data, 
                                       forceSync, 
                                       lock);

          TRI_FreeShapedJson(primary->_shaper, shaped);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single document, using shaped json
////////////////////////////////////////////////////////////////////////////////

        inline int createCollectionShaped (TRI_primary_collection_t* const primary,
                                           const TRI_df_marker_type_e markerType,
                                           TRI_voc_key_t key,
                                           TRI_doc_mptr_t* mptr,
                                           TRI_shaped_json_t const* shaped,
                                           void const* data,
                                           const bool forceSync,
                                           const bool lock) {
          
          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary);

          int res = primary->insert(&context, 
                                    key, 
                                    mptr, 
                                    markerType, 
                                    shaped, 
                                    data, 
                                    (lock && ! isLocked(primary, TRI_TRANSACTION_WRITE)), 
                                    forceSync);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief update a single document, using JSON
////////////////////////////////////////////////////////////////////////////////

        int updateCollectionDocument (TRI_primary_collection_t* const primary,
                                      const string& key,
                                      TRI_doc_mptr_t* mptr,
                                      TRI_json_t* const json,
                                      const TRI_doc_update_policy_e policy,
                                      const TRI_voc_rid_t expectedRevision,
                                      TRI_voc_rid_t* actualRevision,
                                      const bool forceSync,
                                      const bool lock) {

          TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(primary->_shaper, json);

          if (shaped == 0) {
            return TRI_ERROR_ARANGO_SHAPER_FAILED;
          }

          int res = updateCollectionShaped(primary, 
                                           key, 
                                           mptr, 
                                           shaped, 
                                           policy, 
                                           expectedRevision, 
                                           actualRevision, 
                                           forceSync, 
                                           lock);

          TRI_FreeShapedJson(primary->_shaper, shaped);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief update a single document, using shaped json
////////////////////////////////////////////////////////////////////////////////

        inline int updateCollectionShaped (TRI_primary_collection_t* const primary,
                                           const string& key,
                                           TRI_doc_mptr_t* mptr,
                                           TRI_shaped_json_t* const shaped,
                                           const TRI_doc_update_policy_e policy,
                                           const TRI_voc_rid_t expectedRevision,
                                           TRI_voc_rid_t* actualRevision,
                                           const bool forceSync,
                                           const bool lock) {

          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary);

          TRI_doc_update_policy_t updatePolicy;
          TRI_InitUpdatePolicy(&updatePolicy, policy, expectedRevision, actualRevision);
          
          int res = primary->update(&context, 
                                    (TRI_voc_key_t) key.c_str(), 
                                    mptr, 
                                    shaped, 
                                    &updatePolicy, 
                                    (lock && ! isLocked(primary, TRI_TRANSACTION_WRITE)), 
                                    forceSync);
          
          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a single document
////////////////////////////////////////////////////////////////////////////////

        int deleteCollectionDocument (TRI_primary_collection_t* const primary,
                                      const string& key,
                                      const TRI_doc_update_policy_e policy,
                                      const TRI_voc_rid_t expectedRevision,
                                      TRI_voc_rid_t* actualRevision,
                                      const bool forceSync,
                                      const bool lock) {
          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary);
          
          TRI_doc_update_policy_t updatePolicy;
          TRI_InitUpdatePolicy(&updatePolicy, policy, expectedRevision, actualRevision);

          int res = primary->destroy(&context, 
                                     (TRI_voc_key_t) key.c_str(), 
                                     &updatePolicy, 
                                     (lock && ! isLocked(primary, TRI_TRANSACTION_WRITE)), 
                                     forceSync);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate a collection
////////////////////////////////////////////////////////////////////////////////

        int truncateCollection (TRI_primary_collection_t* const primary,
                                const bool forceSync) {

          vector<string> ids;

          int res = readCollectionDocuments(primary, ids);
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary);
          
          TRI_doc_update_policy_t updatePolicy;
          TRI_InitUpdatePolicy(&updatePolicy, TRI_DOC_UPDATE_LAST_WRITE, 0, NULL);

          size_t n = ids.size();

          res = TRI_ERROR_NO_ERROR;

          // WRITE-LOCK START
          this->lockExplicit(primary, TRI_TRANSACTION_WRITE);
          // TODO: fix locks

          for (size_t i = 0; i < n; ++i) {
            const string& id = ids[i];
          
            res = primary->destroy(&context, 
                                   (TRI_voc_key_t) id.c_str(), 
                                   &updatePolicy, 
                                   false,
                                   forceSync);

            if (res != TRI_ERROR_NO_ERROR) {
              // halt on first error
              break;
            }
          }

          this->unlockExplicit(primary, TRI_TRANSACTION_WRITE);
          // WRITE-LOCK END

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the pointer to the C transaction struct 
/// DEPRECATED
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_t* getTrx () {
          return this->_trx;
        }

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
/// @brief register an error for the transaction
////////////////////////////////////////////////////////////////////////////////

        int registerError (int errorNum) {
          TRI_ASSERT_DEBUG(errorNum != TRI_ERROR_NO_ERROR);

          if (_setupState == TRI_ERROR_NO_ERROR) {
            _setupState = errorNum;
          }

          TRI_ASSERT_DEBUG(_setupState != TRI_ERROR_NO_ERROR);

          return errorNum;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to an embedded transaction
////////////////////////////////////////////////////////////////////////////////

        int addCollectionEmbedded (TRI_voc_cid_t cid, TRI_transaction_type_e type) {
          TRI_ASSERT_DEBUG(_trx != 0);

          int res = TRI_AddCollectionTransaction(_trx, cid, type, _nestingLevel);

          if (res != TRI_ERROR_NO_ERROR) {
            return registerError(res);
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to a top-level transaction
////////////////////////////////////////////////////////////////////////////////

        int addCollectionToplevel (TRI_voc_cid_t cid, TRI_transaction_type_e type) {
          TRI_ASSERT_DEBUG(_trx != 0);
          
          int res;

          if (getStatus() != TRI_TRANSACTION_CREATED) {
            // transaction already started?
            res = TRI_ERROR_TRANSACTION_INVALID_STATE;
          }
          else {
            res = TRI_AddCollectionTransaction(_trx, cid, type, _nestingLevel);
          }

          if (res != TRI_ERROR_NO_ERROR) {
            registerError(res);
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the transaction
/// this will first check if the transaction is embedded in a parent 
/// transaction. if not, it will create a transaction of its own
////////////////////////////////////////////////////////////////////////////////

        int setupTransaction () {
          // check in the context if we are running embedded
          _trx = this->getParentTransaction();

          if (_trx != 0) {
            // yes, we are embedded
            _setupState = setupEmbedded();
          }
          else {
            // non-embedded
            _setupState = setupToplevel();
          }

          // this may well be TRI_ERROR_NO_ERROR...
          return _setupState;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set up an embedded transaction
////////////////////////////////////////////////////////////////////////////////

        int setupEmbedded () {
          TRI_ASSERT_DEBUG(_nestingLevel == 0);

          _nestingLevel = ++_trx->_nestingLevel;
            
          if (! this->isEmbeddable()) {
            // we are embedded but this is disallowed...
            LOGGER_WARNING("logic error. invalid nesting of transactions");

            return TRI_ERROR_TRANSACTION_NESTED;
          }
            
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set up a top-level transaction
////////////////////////////////////////////////////////////////////////////////

        int setupToplevel () {
          TRI_ASSERT_DEBUG(_nestingLevel == 0);

          // we are not embedded. now start our own transaction 
          _trx = TRI_CreateTransaction(_vocbase->_transactionContext);

          if (_trx == 0) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          // register the transaction in the context
          return this->registerTransaction(_trx);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief free transaction
////////////////////////////////////////////////////////////////////////////////

        int freeTransaction () {
          TRI_ASSERT_DEBUG(! isEmbeddedTransaction());

          if (_trx != 0) {
            this->unregisterTransaction();

            TRI_FreeTransaction(_trx);
            _trx = 0;
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the "_key" attribute from a JSON object
/// TODO: move out of this class
////////////////////////////////////////////////////////////////////////////////

        int getKey (TRI_json_t const* json, TRI_voc_key_t* key) {
          *key = 0;

          // check type of json
          if (json == 0 || json->_type != TRI_JSON_ARRAY) {
            return TRI_ERROR_NO_ERROR;
          }

          // check _key is there
          const TRI_json_t* k = TRI_LookupArrayJson((TRI_json_t*) json, "_key");

          if (k == 0) {
            return TRI_ERROR_NO_ERROR;
          }

          if (k->_type != TRI_JSON_STRING) {
            // _key is there but not a string
            return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
          }

          // _key is there and a string
          *key = k->_value._string.data;

          return TRI_ERROR_NO_ERROR;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief the C transaction struct
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_t* _trx;

////////////////////////////////////////////////////////////////////////////////
/// @brief error that occurred on transaction initialisation (before begin())
////////////////////////////////////////////////////////////////////////////////

        int _setupState;

////////////////////////////////////////////////////////////////////////////////
/// @brief how deep the transaction is down in a nested transaction structure
////////////////////////////////////////////////////////////////////////////////

        int _nestingLevel;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the transaction is read-only
////////////////////////////////////////////////////////////////////////////////

        bool _readOnly;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction hints
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_hint_t _hints;

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
/// @brief collection name resolver
////////////////////////////////////////////////////////////////////////////////

        const CollectionNameResolver _resolver;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
