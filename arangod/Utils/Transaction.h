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
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

#include "BasicsC/random.h"

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
          _vocbase(vocbase),
          _resolver(resolver),
          _setupError(TRI_ERROR_NO_ERROR),
          _readOnly(true),
          _hints(0) {

          assert(_vocbase != 0);

          int res = createTransaction();
          if (res != TRI_ERROR_NO_ERROR) {
            _setupError = res;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction
////////////////////////////////////////////////////////////////////////////////

        virtual ~Transaction () {
          if (this->_trx != 0 && ! this->isEmbedded()) {
            if (this->status() == TRI_TRANSACTION_RUNNING) {
              // auto abort
              this->abort();
            }
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
/// @brief begin the transaction
////////////////////////////////////////////////////////////////////////////////

        int begin () {
          if (this->_trx == 0) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          if (this->isEmbedded()) {
            if (this->status() == TRI_TRANSACTION_RUNNING) {
              if (this->_setupError != TRI_ERROR_NO_ERROR) {
                return this->_setupError;
              }

              return TRI_ERROR_NO_ERROR;
            }
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          if (this->_setupError != TRI_ERROR_NO_ERROR) {
            return this->_setupError;
          }

          if (this->status() != TRI_TRANSACTION_CREATED) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          return TRI_StartTransaction(this->_trx, _hints);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief commit / finish the transaction
////////////////////////////////////////////////////////////////////////////////

        int commit () {
          if (this->_trx == 0 || this->status() != TRI_TRANSACTION_RUNNING) {
            // transaction not created or not running
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          if (this->isEmbedded()) {
            // return instantly if the transaction is embedded
            return TRI_ERROR_NO_ERROR;
          }


          int res;

          if (this->_trx->_type == TRI_TRANSACTION_READ) {
            // a read transaction just finishes
            res = TRI_FinishTransaction(this->_trx);
          }
          else {
            // a write transaction commits
            res = TRI_CommitTransaction(this->_trx);
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief abort the transaction
////////////////////////////////////////////////////////////////////////////////

        int abort () {
          if (this->_trx == 0 || this->status() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          if (this->isEmbedded() && this->isReadOnlyTransaction()) {
            // return instantly if the transaction is embedded
            return TRI_ERROR_NO_ERROR;
          }

          int res = TRI_AbortTransaction(this->_trx);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief finish a transaction, based on the previous state
////////////////////////////////////////////////////////////////////////////////

        int finish (const int errorNumber) {
          if (this->_trx == 0 || this->status() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          int res;
          if (errorNumber == TRI_ERROR_NO_ERROR) {
            // there was no previous error, so we'll commit
            res = this->commit();
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
/// @brief get the status of the transaction
////////////////////////////////////////////////////////////////////////////////

        inline TRI_transaction_status_e status () const {
          if (this->_trx != 0) {
            return this->_trx->_status;
          }

          return TRI_TRANSACTION_UNDEFINED;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether or not the transaction is read-only
////////////////////////////////////////////////////////////////////////////////

        inline bool isReadOnlyTransaction () const {
          return _readOnly;
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
/// @brief add a collection by id
////////////////////////////////////////////////////////////////////////////////

        int addCollection (TRI_transaction_cid_t cid,
                           TRI_transaction_type_e type) {
          if (this->_trx == 0) {
            return TRI_ERROR_INTERNAL;
          }

          if ((this->status() == TRI_TRANSACTION_RUNNING && ! this->isEmbedded()) ||
              this->status() == TRI_TRANSACTION_COMMITTED ||
              this->status() == TRI_TRANSACTION_ABORTED ||
              this->status() == TRI_TRANSACTION_FINISHED) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          if (cid == 0) {
            // invalid cid
            return _setupError = TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
          }

          if (type == TRI_TRANSACTION_WRITE) {
            _readOnly = false;
          }

          if (this->isEmbedded()) {
            TRI_vocbase_col_t* collection = TRI_CheckCollectionTransaction(this->_trx, cid, type);

            if (collection == 0) {
              // adding an unknown collection...
              int res = TRI_ERROR_NO_ERROR;

              if (type == TRI_TRANSACTION_READ && this->isReadOnlyTransaction()) {
                res = TRI_AddDelayedReadCollectionTransaction(this->_trx, cid);
              }
              else {
                res = TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
              }

              if (res != TRI_ERROR_NO_ERROR) {
                return _setupError = res;
              }
            }

            return TRI_ERROR_NO_ERROR;
          }

          int res = TRI_AddCollectionTransaction(this->_trx, cid, type);

          if (res != TRI_ERROR_NO_ERROR) {
            _setupError = res;
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by name
////////////////////////////////////////////////////////////////////////////////

        int addCollection (const string& name,
                           TRI_transaction_type_e type) {
          TRI_voc_cid_t cid = _resolver.getCollectionId(name);

          if (cid == 0) {
            return _setupError = TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
          }

          return this->addCollection(cid, type);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a transaction hint
////////////////////////////////////////////////////////////////////////////////

        void addHint (const TRI_transaction_hint_e hint) {
          this->_hints |= (TRI_transaction_hint_t) hint;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-lock a collection
////////////////////////////////////////////////////////////////////////////////

        int lockExplicit (TRI_primary_collection_t* const primary,
                          const TRI_transaction_type_e type) {
          if (this->_trx == 0) {
            return TRI_ERROR_INTERNAL;
          }

          if (this->status() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          if (this->isEmbedded()) {
            // locking is a no-op in embedded transactions
            return TRI_ERROR_NO_ERROR;
          }

          return TRI_LockCollectionTransaction(this->_trx, (TRI_transaction_cid_t) primary->base._info._cid, type);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-unlock a collection
////////////////////////////////////////////////////////////////////////////////

        int unlockExplicit (TRI_primary_collection_t* const primary,
                            const TRI_transaction_type_e type) {
          if (this->_trx == 0) {
            return TRI_ERROR_INTERNAL;
          }

          if (this->status() != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          if (this->isEmbedded()) {
            // locking is a no-op in embedded transactions
            return TRI_ERROR_NO_ERROR;
          }

          return TRI_UnlockCollectionTransaction(this->_trx, (TRI_transaction_cid_t) primary->base._info._cid, type);
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
          TRI_InitReadContextPrimaryCollection(&context, primary);

          if (lock) {
            // READ-LOCK START
            this->lockExplicit(primary, TRI_TRANSACTION_READ);
          }

          int res = primary->read(&context, mptr, (TRI_voc_key_t) key.c_str());

          if (lock) {
            this->unlockExplicit(primary, TRI_TRANSACTION_READ);
            // READ-LOCK END
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief read all documents
////////////////////////////////////////////////////////////////////////////////

        int readCollectionDocuments (TRI_primary_collection_t* const primary,
                                     vector<string>& ids) {
          TRI_doc_operation_context_t context;
          TRI_InitReadContextPrimaryCollection(&context, primary);

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
          TRI_InitReadContextPrimaryCollection(&context, primary);

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

          // check if _key is present
          if (json != NULL && json->_type == TRI_JSON_ARRAY) {
            // _key is there
            TRI_json_t* k = TRI_LookupArrayJson((TRI_json_t*) json, "_key");
            if (k != NULL) {
              if (k->_type == TRI_JSON_STRING) {
                // _key is there and a string
                key = k->_value._string.data;
              }
              else {
                // _key is there but not a string
                return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
              }
            }
          }

          TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(primary->_shaper, json);
          if (shaped == 0) {
            return TRI_ERROR_ARANGO_SHAPER_FAILED;
          }

          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary, TRI_DOC_UPDATE_ERROR, forceSync);


          if (lock) {
            // WRITE-LOCK START
            this->lockExplicit(primary, TRI_TRANSACTION_WRITE);
          }

          int res = primary->create(&context, markerType, mptr, shaped, data, key);

          if (lock) {
            this->unlockExplicit(primary, TRI_TRANSACTION_WRITE);
            // WRITE-LOCK END
          }

          TRI_FreeShapedJson(primary->_shaper, shaped);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a single document, using shaped json
////////////////////////////////////////////////////////////////////////////////

        int createCollectionShaped (TRI_primary_collection_t* const primary,
                                    const TRI_df_marker_type_e markerType,
                                    TRI_voc_key_t key,
                                    TRI_doc_mptr_t* mptr,
                                    TRI_shaped_json_t const* shaped,
                                    void const* data,
                                    const bool forceSync,
                                    const bool lock) {
          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary, TRI_DOC_UPDATE_ERROR, forceSync);

          if (lock) {
            // WRITE-LOCK START
            this->lockExplicit(primary, TRI_TRANSACTION_WRITE);
          }

          int res = primary->create(&context, markerType, mptr, shaped, data, key);

          if (lock) {
            this->unlockExplicit(primary, TRI_TRANSACTION_WRITE);
            // WRITE-LOCK END
          }

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

          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);
          context._expectedRid = expectedRevision;
          context._previousRid = actualRevision;

          if (lock) {
            // WRITE-LOCK START
            this->lockExplicit(primary, TRI_TRANSACTION_WRITE);
          }

          int res = primary->update(&context, mptr, shaped, (TRI_voc_key_t) key.c_str());

          if (lock) {
            this->unlockExplicit(primary, TRI_TRANSACTION_WRITE);
            // WRITE-LOCK END
          }

          TRI_FreeShapedJson(primary->_shaper, shaped);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief update a single document, using shaped json
////////////////////////////////////////////////////////////////////////////////

        int updateCollectionShaped (TRI_primary_collection_t* const primary,
                                    const string& key,
                                    TRI_doc_mptr_t* mptr,
                                    TRI_shaped_json_t* const shaped,
                                    const TRI_doc_update_policy_e policy,
                                    const TRI_voc_rid_t expectedRevision,
                                    TRI_voc_rid_t* actualRevision,
                                    const bool forceSync,
                                    const bool lock) {
          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);
          context._expectedRid = expectedRevision;
          context._previousRid = actualRevision;

          if (lock) {
            // WRITE-LOCK START
            this->lockExplicit(primary, TRI_TRANSACTION_WRITE);
          }

          int res = primary->update(&context, mptr, shaped, (TRI_voc_key_t) key.c_str());

          if (lock) {
            this->unlockExplicit(primary, TRI_TRANSACTION_WRITE);
            // WRITE-LOCK END
          }

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
                                      const bool forceSync) {
          TRI_doc_operation_context_t context;
          TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);
          context._expectedRid = expectedRevision;
          context._previousRid = actualRevision;

          // WRITE-LOCK START
          this->lockExplicit(primary, TRI_TRANSACTION_WRITE);

          int res = primary->destroy(&context, (TRI_voc_key_t) key.c_str());

          this->unlockExplicit(primary, TRI_TRANSACTION_WRITE);
          // WRITE-LOCK END

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
          TRI_InitContextPrimaryCollection(&context, primary, TRI_DOC_UPDATE_LAST_WRITE, forceSync);
          size_t n = ids.size();

          res = TRI_ERROR_NO_ERROR;

          // WRITE-LOCK START
          this->lockExplicit(primary, TRI_TRANSACTION_WRITE);

          for (size_t i = 0; i < n; ++i) {
            const string& id = ids[i];

            res = primary->destroy(&context, (TRI_voc_key_t) id.c_str());
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

            if (this->_trx == 0) {
              return TRI_ERROR_TRANSACTION_INVALID_STATE;
            }

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
          assert(! this->isEmbedded());

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
/// @brief collection name resolver
////////////////////////////////////////////////////////////////////////////////

        const CollectionNameResolver _resolver;

////////////////////////////////////////////////////////////////////////////////
/// @brief error that occurred on transaction initialisation (before begin())
////////////////////////////////////////////////////////////////////////////////

        int _setupError;

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

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
